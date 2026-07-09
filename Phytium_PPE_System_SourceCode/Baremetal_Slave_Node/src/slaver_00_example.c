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
#include "aht20.h"
#include "gas_sensor.h"
#include "fgeneric_timer.h"
#include "fwdt.h"
#include "fparameters.h"
#include "fsleep.h"

/************************** 外部驱动声明与全局状态机 *****************************/
extern void led20Set(int flag);
extern void Buzzer_Init(void);
extern void Buzzer_Set(int flag);
extern void Fire_Sensor_Intr_Init(void);
extern int Fire_Sensor_Read_Level(void);

volatile bool flag_ai_alarm_req = false; 
static struct rpmsg_endpoint *g_ept = NULL; /* 全局端点指针，用于主动向上级发信 */

static FWdtCtrl g_wdt_ctrl;
static volatile u32 g_heartbeat_miss_count = 0;
static volatile bool g_fail_safe_active = false;
static volatile bool g_has_received_first_heartbeat = false;
static volatile bool g_wdt_started = false;

/************************** 宏定义区 (必须在函数前) *****************************/
#define SLAVE_DEBUG_TAG "    SLAVE_00"
#define SLAVE_DEBUG_I(format, ...) FT_DEBUG_PRINT_I(SLAVE_DEBUG_TAG, format, ##__VA_ARGS__)
#define SLAVE_DEBUG_W(format, ...) FT_DEBUG_PRINT_W(SLAVE_DEBUG_TAG, format, ##__VA_ARGS__)
#define SLAVE_DEBUG_E(format, ...) FT_DEBUG_PRINT_E(SLAVE_DEBUG_TAG, format, ##__VA_ARGS__)

#define MAX_DATA_LENGTH      255
#define DEVICE_CORE_LED_CTRL 0x0004U 
#define DEVICE_CORE_BUZZER_CTRL 0x0005U 
#define DEVICE_CORE_FIRE_REPORT 0x0006U 
#define DEVICE_CORE_GAS_REPORT  0x0007U
#define DEVICE_CORE_ENV_REPORT  0x0008U
#define DEVICE_CORE_START    0x0001U 
#define DEVICE_CORE_SHUTDOWN 0x0002U 
#define DEVICE_CORE_CHECK    0x0003U 

/************************** 结构体区 (必须在函数前) *****************************/
typedef struct
{
    uint32_t command;           
    uint16_t length;            
    char data[MAX_DATA_LENGTH]; 
    uint8_t crc8;
} ProtocolData;

static ProtocolData protocol_data;

static inline uint8_t calculate_crc8(const uint8_t *data, size_t len) {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; i++) {
        uint8_t inbyte = data[i];
        for (uint8_t j = 0; j < 8; j++) {
            uint8_t mix = (crc ^ inbyte) & 0x01;
            crc >>= 1;
            if (mix) {
                crc ^= 0x8C;
            }
            inbyte >>= 1;
        }
    }
    return crc;
}

int assemble_protocol_data(const ProtocolData *input, char *output, size_t *output_size);

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
        
        char tx_buf[512];
        size_t tx_len = 0;
        assemble_protocol_data(&report, tx_buf, &tx_len);
        tx_buf[tx_len] = calculate_crc8((const uint8_t *)tx_buf, tx_len);
        rpmsg_send(g_ept, tx_buf, tx_len + 1);
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

    if (len < 7) return RPMSG_SUCCESS;

    // CRC-8 校验验证
    uint8_t received_crc = ((uint8_t *)data)[len - 1];
    uint8_t calculated_crc = calculate_crc8((const uint8_t *)data, len - 1);
    if (received_crc != calculated_crc) {
        printf("CRC8 verify failed!\r\n");
        return RPMSG_SUCCESS;
    }
    
    // 只要收到主核任何通过校验的消息，就认为主核已建立握手
    g_has_received_first_heartbeat = true;
    if (!g_wdt_started)
    {
        if (g_wdt_ctrl.is_ready)
        {
            FWdtStart(&g_wdt_ctrl);
            g_wdt_started = true;
        }
    }

    // 【关键修复】：收到任何通过 CRC 校验的主核消息，都刷新心跳计数和看门狗
    // 因为收到消息本身就证明主核还活着，不能只依赖 CHECK 命令喂狗
    g_heartbeat_miss_count = 0;
    if (!g_fail_safe_active && g_wdt_ctrl.is_ready && g_wdt_started) {
        FWdtRefresh(&g_wdt_ctrl);
    }

    ret = parse_protocol_data((char *)data, len - 1, &protocol_data);
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
            g_heartbeat_miss_count = 0;
            if (!g_fail_safe_active && g_wdt_ctrl.is_ready) {
                FWdtRefresh(&g_wdt_ctrl);
            }
            {
                char tx_buf[512];
                size_t tx_len = 0;
                assemble_protocol_data(&protocol_data, tx_buf, &tx_len);
                tx_buf[tx_len] = calculate_crc8((const uint8_t *)tx_buf, tx_len);
                ret = rpmsg_send(ept, tx_buf, tx_len + 1);
                if (ret < 0) return ret;
            }
            break;

        case 0x0099U: // DEVICE_CORE_CRC_TEST (CRC测试触发命令)
            for (int i = 0; i < 5; i++) {
                ProtocolData corrupt_pkt;
                corrupt_pkt.command = DEVICE_CORE_ENV_REPORT;
                int len = snprintf(corrupt_pkt.data, MAX_DATA_LENGTH, "T:0.0,H:0.0");
                corrupt_pkt.length = len;
                
                char tx_buf[512];
                size_t tx_len = 0;
                assemble_protocol_data(&corrupt_pkt, tx_buf, &tx_len);
                // 故意破坏校验码 (取反) 产生损坏的包
                tx_buf[tx_len] = calculate_crc8((const uint8_t *)tx_buf, tx_len) ^ 0xFF;
                rpmsg_send(ept, tx_buf, tx_len + 1);
                
                // 在测试间隙喂狗并处理心跳，防止测试期间触发看门狗冷重启
                g_heartbeat_miss_count = 0;
                if (g_wdt_ctrl.is_ready && g_wdt_started) {
                    FWdtRefresh(&g_wdt_ctrl);
                }
                fsleep_millisec(200); // 缩短间隔到 200ms，减少阻塞时间
            }
            break;
            
        default:
            break;
    }
    return RPMSG_SUCCESS;
}

static void rpmsg_service_unbind(struct rpmsg_endpoint *ept)
{
    (void)ept;
    printf("cpu3: [ALERT] Unexpected remote endpoint destroy! Entering Fail-Safe mode.\r\n");
    
    // 缩短看门狗超时时间到 1s，并立即刷新以装载新值，准备冷重启
    if (g_wdt_ctrl.is_ready)
    {
        FWdtSetTimeout(&g_wdt_ctrl, (u32)(GenericTimerFrequecy() * 1));
        FWdtRefresh(&g_wdt_ctrl);
    }
    
    // 进入自愈死循环等待看门狗复位
    printf("cpu3: Watchdog kick stopped. System will cold reset in 1s...\r\n");
    while (1)
    {
        // 挂起忙等，停止一切喂狗
    }
}

static int FRpmsgEchoApp(struct rpmsg_device *rdev, void *priv)
{
    int ret = 0;
    static struct rpmsg_endpoint lept = {0};
    shutdown_req = 0;

    ret = rpmsg_create_ept(&lept, rdev, RPMSG_SERVICE_NAME, 0, RPMSG_ADDR_ANY,
                           rpmsg_endpoint_cb, rpmsg_service_unbind);
    if (ret) return -1;

    printf("\r\n=== PPE Slave Core v2.1 (heartbeat_threshold=4, 2s) READY ===\r\n");

    /* 暴露通信端点给全局仲裁器 */
    g_ept = &lept;

    Fire_Sensor_Intr_Init();
    
    /* 开机基准状态检测 */
    Execute_Alarm_Arbitration();

    u64 tick_hz = GenericTimerFrequecy();
    u64 period = tick_hz / 2; // 500ms
    u64 last_tick = GenericTimerRead(GENERIC_TIMER_ID0);

    // 重置心跳计数器，避免刚启动时由于连接延迟导致瞬间误判
    g_heartbeat_miss_count = 0;
    g_fail_safe_active = false;
    g_has_received_first_heartbeat = false;
    g_wdt_started = false;

    while (1)
    {
        platform_poll_nonblocking(priv);
        fsleep_millisec(10); // 避免空转烧CPU，同时保证心跳检测不被阻塞
        
        u64 current_tick = GenericTimerRead(GENERIC_TIMER_ID0);
        if (current_tick - last_tick >= period)
        {
            last_tick = current_tick;

            // 心跳丢失判定 (4 次 * 500ms = 2s)
            if (!g_fail_safe_active && g_wdt_started)
            {
                if (g_has_received_first_heartbeat)
                {
                    g_heartbeat_miss_count++;
                    printf("cpu3: heartbeat_miss=%lu/4\r\n", (unsigned long)g_heartbeat_miss_count);
                    if (g_heartbeat_miss_count >= 4)
                    {
                        g_fail_safe_active = true;
                        printf("cpu3: [ALERT] Master Core Link Loss! Entering Fail-Safe mode.\r\n");
                        
                        // 缩短看门狗超时时间到 1s，并立即刷新以装载新值，准备冷重启
                        if (g_wdt_ctrl.is_ready)
                        {
                            FWdtSetTimeout(&g_wdt_ctrl, (u32)(GenericTimerFrequecy() * 1));
                            FWdtRefresh(&g_wdt_ctrl);
                        }
                        
                        // 进入自愈死循环等待看门狗复位
                        printf("cpu3: Watchdog kick stopped. System will cold reset in 1s...\r\n");
                        while (1)
                        {
                            // 挂起忙等，停止一切喂狗
                        }
                    }
                }
            }
            
            // 1. 读取并上报 AHT20 温湿度
            float temp = 0.0f, humid = 0.0f;
            if (AHT20_Read_Sensor(&temp, &humid) == FT_SUCCESS)
            {
                ProtocolData env_pkt;
                env_pkt.command = DEVICE_CORE_ENV_REPORT;
                int len = snprintf(env_pkt.data, MAX_DATA_LENGTH, "T:%.1f,H:%.1f", temp, humid);
                env_pkt.length = len;
                if (g_ept)
                {
                    char tx_buf[512];
                    size_t tx_len = 0;
                    assemble_protocol_data(&env_pkt, tx_buf, &tx_len);
                    tx_buf[tx_len] = calculate_crc8((const uint8_t *)tx_buf, tx_len);
                    rpmsg_send(g_ept, tx_buf, tx_len + 1);
                }
            }
            else
            {
                ProtocolData env_pkt;
                env_pkt.command = DEVICE_CORE_ENV_REPORT;
                int len = snprintf(env_pkt.data, MAX_DATA_LENGTH, "ERR");
                env_pkt.length = len;
                if (g_ept)
                {
                    char tx_buf[512];
                    size_t tx_len = 0;
                    assemble_protocol_data(&env_pkt, tx_buf, &tx_len);
                    tx_buf[tx_len] = calculate_crc8((const uint8_t *)tx_buf, tx_len);
                    rpmsg_send(g_ept, tx_buf, tx_len + 1);
                }
            }
            
            // 2. 读取并上报 MQ-2 可燃气体状态
            int gas_level = Gas_Sensor_Read_Level();
            ProtocolData gas_pkt;
            gas_pkt.command = DEVICE_CORE_GAS_REPORT;
            gas_pkt.data[0] = (gas_level == 1) ? '1' : '0'; // 15号引脚(P11/ADC)接Pin 13(GPIO1_12)：因为无比较器，清洁空气输出低电平0(安全)，超标高电平1(报警)
            gas_pkt.length = 1;
            if (g_ept)
            {
                char tx_buf[512];
                size_t tx_len = 0;
                assemble_protocol_data(&gas_pkt, tx_buf, &tx_len);
                tx_buf[tx_len] = calculate_crc8((const uint8_t *)tx_buf, tx_len);
                rpmsg_send(g_ept, tx_buf, tx_len + 1);
            }
        }
        
        if (shutdown_req || rproc_get_stop_flag())
        {
            rproc_clear_stop_flag();
            break;
        }
    }
    
    // 正常退出程序前关闭看门狗，防止停止从核服务时触发硬件意外重启
    if (g_wdt_ctrl.is_ready)
    {
        FWdtStop(&g_wdt_ctrl);
        g_wdt_started = false;
    }

    rpmsg_destroy_ept(&lept);
    g_ept = NULL; /* 安全清理 */
    return ret;
}

int slave_init(void)
{
    init_system(); 
    Buzzer_Init();
    AHT20_Init();
    Gas_Sensor_Init();
    
    // 初始化和启动飞腾底层硬件看门狗 FWDT0
    FError wdt_ret;
    FWdtConfig wdt_config = *FWdtLookupConfig(FWDT0_ID);
    memset(&g_wdt_ctrl, 0, sizeof(g_wdt_ctrl));
    wdt_ret = FWdtCfgInitialize(&g_wdt_ctrl, &wdt_config);
    if (FWDT_SUCCESS == wdt_ret)
    {
        // 初始超时设置为 10s
        FWdtSetTimeout(&g_wdt_ctrl, (u32)(GenericTimerFrequecy() * 10));
        SLAVE_DEBUG_I("FWDT0 initialized (10s timeout, pending start on handshake).\r\n");
    }
    else
    {
        SLAVE_DEBUG_E("FWDT0 initialization failed: 0x%x\r\n", wdt_ret);
    }
    
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
