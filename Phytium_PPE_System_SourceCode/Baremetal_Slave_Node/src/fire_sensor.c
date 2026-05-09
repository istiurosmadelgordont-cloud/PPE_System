/**
 * @file      fire_sensor.c
 * @brief     [Bare-metal] GPIO 中断级环境异常感知节点
 * @author    [双生序章]
 * @version   2.0.0
 * @date      2026-04-18
 * @note      物理映射：外设 GPIO2_10。
 *            安全设计：配置为边缘触发中断 (Edge-Triggered EXTI)。
 *            为满足工业极速灾害响应需求，系统摒弃传统的轮询与下半部(Bottom Half)推迟策略，
 *            在 ISR (中断服务例程) 唤醒的微秒级瞬间，直接劫持指令流执行 Execute_Alarm_Arbitration
 *            强制进行声光阻断及总线发包。这是用微架构风险换取极限低延迟的激进设计。
 */
#include "sdkconfig.h"
#include "fgpio.h"
#include "fio_mux.h"
#include "finterrupt.h"
#include "fcpu_info.h"
#include <stdio.h>

static FGpio fire_gpio;

/* 外部引入全局仲裁器 */
extern void Execute_Alarm_Arbitration(void);

static void Fire_Sensor_IRQ_Handler(s32 vector, void *param)
{
    if (FGpioGetInputValue(&fire_gpio) == 0) {
        FGpioSetInterruptType(&fire_gpio, FGPIO_IRQ_TYPE_EDGE_RISING);
    } else {
        FGpioSetInterruptType(&fire_gpio, FGPIO_IRQ_TYPE_EDGE_FALLING);
    }
    
    /* 【核心反杀】：在中断苏醒的瞬间，直接执行仲裁！决不回传主循环！ */
    Execute_Alarm_Arbitration();
}

void Fire_Sensor_Intr_Init(void) {
    u32 fire_gpio_id = FGPIO_ID(FGPIO_CTRL_2, FGPIO_PIN_10);
    const FGpioConfig *config = FGpioLookupConfig(fire_gpio_id);

    if(NULL == config) {
        printf("🚨 Fatal: GPIO2_10 Config Failed\r\n");
        return;
    }

    FIOMuxInit();
    FGpioCfgInitialize(&fire_gpio, config);
    FIOPadSetGpioMux(fire_gpio.config.ctrl, (u32)fire_gpio.config.pin);
    FGpioSetDirection(&fire_gpio, FGPIO_DIR_INPUT);

    u32 irq_num = fire_gpio.config.irq_num;
    u32 cpu_id;
    GetCpuId(&cpu_id);

    InterruptSetTargetCpus(irq_num, cpu_id);
    InterruptSetPriority(irq_num, 0); 

    FGpioRegisterInterruptCB(&fire_gpio, Fire_Sensor_IRQ_Handler, NULL);
    InterruptInstall(irq_num, FGpioInterruptHandler, NULL, NULL);
    InterruptUmask(irq_num);

    FGpioSetInterruptType(&fire_gpio, FGPIO_IRQ_TYPE_EDGE_FALLING);
    FGpioSetInterruptMask(&fire_gpio, TRUE); 
}

int Fire_Sensor_Read_Level(void) {
    return FGpioGetInputValue(&fire_gpio);
}
