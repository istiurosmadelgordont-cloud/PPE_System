#include "gas_sensor.h"
#include "fgpio.h"
#include "fio_mux.h"
#include <stdio.h>

static FGpio gas_gpio;

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
