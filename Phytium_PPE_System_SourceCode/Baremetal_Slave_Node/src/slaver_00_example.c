/**
 * @file      slaver_00_example.c
 * @brief     [Bare-metal] 异构多核前哨站核心控制与通信中枢
 * @author    [双生序章]
 * @version   2.0.0
 * @date      2026-04-18
 * @note      运行于 FTC310 (Core 1) 裸机环境。
 *            核心职责：
 *            1. 建立基于 OpenAMP 框架的 RPMsg 双向共享内存总线。
 *            2. 绕开 Linux OS 调度，利用 platform_poll 极速轮询下行干预指令。
 *            3. 承接来自硬件中断层 (EXTI) 的高优先级告警状态，并向上级 Linux 主核
 *               执行微秒级穿透上报。
 *            警告：此模块运行于无操作系统态，严禁调用任何 POSIX 标准库函数。
 */
#include <stdio.h>
#include <stdbool.h>
#include <openamp/open_amp.h>
#include <metal/alloc.h>
#include <metal/sleep.h>
#include "platform_info.h"
#include "rpmsg_service.h"
#include "rsc_table.h"
#include "fcache.h"
#include "fdebug.h"
#include "fpsci.h"
#include "helper.h"
#include "openamp_configs.h"
#include "libmetal_configs.h"
#include "slaver_00_example.h"
#include "buzzer.h"

/************************** 外部驱动声明与全局状态机 *****************************/
extern void led20Set(int flag);
extern void Buzzer_Init(void);
extern void Buzzer_Set(int flag);
extern void Fire_Sensor_Intr_Init(void);
extern int Fire_Sensor_Read_Level(void);

volatile bool flag_ai_alarm_req = false; 
static struct rpmsg_endpoint *g_ept = NULL; /* 全局端点指针，用于主动向上级发信 */

/************************** 宏定义区 (必须在函数前) *****************************/
#define SLAVE_DEBUG_TAG "    SLAVE_00"
#define SLAVE_DEBUG_I(format, ...) FT_DEBUG_PRINT_I(SLAVE_DEBUG_TAG, format, ##__VA_ARGS__)
#define SLAVE_DEBUG_W(format, ...) FT_DEBUG_PRINT_W(SLAVE_DEBUG_TAG, format, ##__VA_ARGS__)
#define SLAVE_DEBUG_E(format, ...) FT_DEBUG_PRINT_E(SLAVE_DEBUG_TAG, format, ##__VA_ARGS__)

#define MAX_DATA_LENGTH      (RPMSG_BUFFER_SIZE / 2)
#define DEVICE_CORE_LED_CTRL 0x0004U 
#define DEVICE_CORE_BUZZER_CTRL 0x0005U 
#define DEVICE_CORE_FIRE_REPORT 0x0006U 
#define DEVICE_CORE_START    0x0001U 
#define DEVICE_CORE_SHUTDOWN 0x0002U 
#define DEVICE_CORE_CHECK    0x0003U 

/************************** 结构体区 (必须在函数前) *****************************/
typedef struct
{
    uint32_t command;           
    uint16_t length;            
    char data[MAX_DATA_LENGTH]; 
} ProtocolData;

static ProtocolData protocol_data;

static volatile int shutdown_req = 0;
struct remoteproc remoteproc_slave_00;
static struct rpmsg_device *rpdev_slave_00 = NULL;

/************************** 资源表与设备树配置 **********/
static struct remote_resource_table __resource resources __attribute__((used)) = {
    1, NUM_TABLE_ENTRIES, {0, 0}, {offsetof(struct remote_resource_table, rpmsg_vdev)},
    {RSC_VDEV, VIRTIO_ID_RPMSG_, VDEV_NOTIFYID, RPMSG_IPU_C0_FEATURES, 0, 0, 0, NUM_VRINGS, {0, 0}},
    {SLAVE00_TX_VRING_ADDR, VRING_ALIGN, SLAVE00_VRING_NUM, 1, 0},
    {SLAVE00_RX_VRING_ADDR, VRING_ALIGN, SLAVE00_VRING_NUM, 2, 0},
};

static metal_phys_addr_t poll_phys_addr = SLAVE00_KICK_IO_ADDR;
struct metal_device kick_driver_00 = {
    .name = SLAVE_00_KICK_DEV_NAME,
    .bus = NULL,
    .num_regions = 1,
    .regions = {{
        .virt = (void *)SLAVE00_KICK_IO_ADDR,
        .physmap = &poll_phys_addr,
        .size = 0x1000,
        .page_shift = -1UL,
        .page_mask = -1UL,
        .mem_flags = SLAVE00_SOURCE_TABLE_ATTRIBUTE,
        .ops = {NULL},
    }},
    .irq_num = 1, 
    .irq_info = (void *)SLAVE_00_SGI,
};

struct remoteproc_priv slave_00_priv = {
    .kick_dev_name = SLAVE_00_KICK_DEV_NAME,
    .kick_dev_bus_name = KICK_BUS_NAME,
    .cpu_id = MASTER_CORE_MASK, 
    .src_table_attribute = SLAVE00_SOURCE_TABLE_ATTRIBUTE,
    .share_mem_va = SLAVE00_SHARE_MEM_ADDR,
    .share_mem_pa = SLAVE00_SHARE_MEM_ADDR,
    .share_buffer_offset = SLAVE00_VRING_SIZE,
    .share_mem_size = SLAVE00_SHARE_MEM_SIZE,
    .share_mem_attribute = SLAVE00_SHARE_BUFFER_ATTRIBUTE
};

/************************** 核心逻辑实现区 ******************************/

/* ==================================================================
 * 【核心仲裁器】
 * 现在它安全地放在了所有声明的后面！
 * ================================================================== */
void Execute_Alarm_Arbitration(void) {
    int sensor_level = Fire_Sensor_Read_Level();
    if ((sensor_level == 0) || flag_ai_alarm_req) {
        Buzzer_Set(1); 
    } else {
        Buzzer_Set(0); 
    }

    /* 向上级汇报底层真实的物理探头状态 */
    if (g_ept) {
        ProtocolData report;
        report.command = DEVICE_CORE_FIRE_REPORT; 
        report.length = 1;
        report.data[0] = (sensor_level == 0) ? '1' : '0'; 
        rpmsg_send(g_ept, &report, sizeof(ProtocolData));
    }
}

int parse_protocol_data(const char *input, size_t input_size, ProtocolData *output)
{
    if (input_size < 6) return -1; 
    output->command = *((uint32_t *)input);
    input += 4;
    output->length = *((uint16_t *)input);
    input += 2;
    if (output->length > MAX_DATA_LENGTH) return -2; 
    memcpy(output->data, input, output->length);
    return 0; 
}

int assemble_protocol_data(const ProtocolData *input, char *output, size_t *output_size)
{
    if (6 + input->length > MAX_DATA_LENGTH) return -1; 
    *output_size = 6 + input->length; 
    *((uint32_t *)output) = input->command;
    *((uint16_t *)(output + 4)) = input->length;
    memcpy(output + 6, input->data, input->length);
    return 0; 
}

static int rpmsg_endpoint_cb(struct rpmsg_endpoint *ept, void *data, size_t len,
                             uint32_t src, void *priv)
{
    int ret;
    (void)priv;
    ept->dest_addr = src;

    ret = parse_protocol_data((char *)data, len, &protocol_data);
    if (ret != 0) return RPMSG_SUCCESS; 

    switch (protocol_data.command)
    {
        case DEVICE_CORE_LED_CTRL:
            if (protocol_data.length > 0) {
                if (protocol_data.data[0] == '1') led20Set(1);
                else if (protocol_data.data[0] == '0') led20Set(0);
            }
            break;
          
        case DEVICE_CORE_BUZZER_CTRL: 
            if (protocol_data.length > 0) {
                flag_ai_alarm_req = (protocol_data.data[0] == '1');
                Execute_Alarm_Arbitration();
            }
            break;

        case DEVICE_CORE_START:
            break;
            
        case DEVICE_CORE_SHUTDOWN:
            shutdown_req = 1;
            break;
            
        case DEVICE_CORE_CHECK:
            ret = rpmsg_send(ept, &protocol_data, len);
            if (ret < 0) return ret;
            break;
            
        default:
            break;
    }
    return RPMSG_SUCCESS;
}

static void rpmsg_service_unbind(struct rpmsg_endpoint *ept)
{
    (void)ept;
    SLAVE_DEBUG_I("Unexpected remote endpoint destroy.\r\n");
    shutdown_req = 1;
}

static int FRpmsgEchoApp(struct rpmsg_device *rdev, void *priv)
{
    int ret = 0;
    static struct rpmsg_endpoint lept = {0};
    shutdown_req = 0;

    ret = rpmsg_create_ept(&lept, rdev, RPMSG_SERVICE_NAME, 0, RPMSG_ADDR_ANY,
                           rpmsg_endpoint_cb, rpmsg_service_unbind);
    if (ret) return -1;

    SLAVE_DEBUG_I("Successfully created rpmsg endpoint.\r\n");

    /* 暴露通信端点给全局仲裁器 */
    g_ept = &lept;

    Fire_Sensor_Intr_Init();
    
    /* 开机基准状态检测 */
    Execute_Alarm_Arbitration();

    while (1)
    {
        platform_poll(priv);
        
        if (shutdown_req || rproc_get_stop_flag())
        {
            rproc_clear_stop_flag();
            break;
        }
    }
    
    rpmsg_destroy_ept(&lept);
    g_ept = NULL; /* 安全清理 */
    return ret;
}

int slave_init(void)
{
    init_system(); 
    Buzzer_Init();
    
    if (!platform_create_proc(&remoteproc_slave_00, &slave_00_priv, &kick_driver_00))
        return -1; 

    remoteproc_slave_00.rsc_table = &resources;

    if (platform_setup_src_table(&remoteproc_slave_00, remoteproc_slave_00.rsc_table))
        return -1;

    if (platform_setup_share_mems(&remoteproc_slave_00))
        return -1;

    rpdev_slave_00 = platform_create_rpmsg_vdev(&remoteproc_slave_00, 0,
                                                VIRTIO_DEV_DEVICE, NULL, NULL);
    if (!rpdev_slave_00) return -1; 

    return 0;
}

int slave00_rpmsg_echo_process(void)
{
    int ret = 0;
    SLAVE_DEBUG_I("Starting application...");
    if (!slave_init())
    {
        ret = FRpmsgEchoApp(rpdev_slave_00, &remoteproc_slave_00);
        if (ret)
        {
            platform_cleanup(&remoteproc_slave_00);
            return -1;
        }
        platform_release_rpmsg_vdev(rpdev_slave_00, &remoteproc_slave_00);
        platform_cleanup(&remoteproc_slave_00);
    }
    else
    {
        platform_cleanup(&remoteproc_slave_00);
    }
    FPsciCpuOff();
    return 0;
}
