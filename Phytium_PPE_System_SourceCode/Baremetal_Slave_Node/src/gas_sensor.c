#include "gas_sensor.h"
#include "fgpio.h"
#include "fio_mux.h"
#include "fiopad_hw.h"
#include "fiopad.h"
#include "fsleep.h"
#include <stdio.h>

static FGpio gas_gpio;
extern FIOPadCtrl iopad_ctrl;

void Gas_Sensor_Init(void)
{
    u32 gas_gpio_id = FGPIO_ID(FGPIO_CTRL_1, FGPIO_PIN_12); // GPIO1_12 (Pin 13)
    const FGpioConfig *config = FGpioLookupConfig(gas_gpio_id);
    
    if (NULL == config) {
        printf("[GAS] GPIO1_12 查找配置失败\r\n");
        return;
    }

    FIOMuxInit();
    FGpioCfgInitialize(&gas_gpio, config);
    FIOPadSetGpioMux(FGPIO_CTRL_1, FGPIO_PIN_12);
    FGpioSetDirection(&gas_gpio, FGPIO_DIR_INPUT);
    printf("[GAS] MQ-2 GPIO 初始化完毕\r\n");
}

int Gas_Sensor_Read_Level(void)
{
    return FGpioGetInputValue(&gas_gpio);
}

int Gas_Sensor_Read_Level_With_Detect(int *disconnected)
{
    // 1. 设置为弱下拉并读取
    FIOPadSetPull(&iopad_ctrl, FIOPAD_AW55_REG0_OFFSET, FIOPAD_PULL_DOWN);
    fsleep_millisec(1); // 给电平响应时间
    int val_down = FGpioGetInputValue(&gas_gpio);

    // 2. 设置为弱上拉并读取
    FIOPadSetPull(&iopad_ctrl, FIOPAD_AW55_REG0_OFFSET, FIOPAD_PULL_UP);
    fsleep_millisec(1);
    int val_up = FGpioGetInputValue(&gas_gpio);

    // 3. 恢复无拉状态
    FIOPadSetPull(&iopad_ctrl, FIOPAD_AW55_REG0_OFFSET, FIOPAD_PULL_NONE);

    if (val_down != val_up) {
        *disconnected = 1;
        return 0; // 认为断开，默认返回安全值 0
    } else {
        *disconnected = 0;
        return val_down;
    }
}

