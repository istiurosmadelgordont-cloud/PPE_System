/**
 * @file      buzzer.c
 * @brief     [Bare-metal] 物理声光阻断器寄存器级驱动
 * @author    [双生序章]
 * @version   2.0.0
 * @date      2026-04-18
 * @note      物理映射：外设 GPIO3_1。
 *            设计范式：彻底绕过 Linux 虚拟文件系统 (Sysfs/VFS) 的设备树挂载，
 *            在裸机层通过内存映射地址直接写入推挽输出寄存器 (DO)。
 *            确保接收到 AI 判定或中断信号后，物理阻断干预延迟严格收敛于纳秒级别。
 */

#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "fio.h"
#include "fiopad.h"
#include "fgpio_hw.h"
#include "fgpio.h"
#include "fio_mux.h"
#include "buzzer.h"

static int buzzer_init_flag = 0;
static FGpio buzzer_gpio;

void Buzzer_Init(void) 
{
    if(0 == buzzer_init_flag) 
    {
        /* 根据引脚表，使用 GPIO3_1 */
        u32 buzzer_gpio_id = FGPIO_ID(FGPIO_CTRL_3, FGPIO_PIN_1);
        const FGpioConfig *config = FGpioLookupConfig(buzzer_gpio_id);
        
        if(NULL == config) {
            printf("Buzzer_Init(): FGpioLookupConfig error\r\n");
            return;
        }

        memset(&buzzer_gpio, 0, sizeof(buzzer_gpio));
        if(FGPIO_SUCCESS != FGpioCfgInitialize(&buzzer_gpio, config)) {
            printf("Buzzer_Init(): FGpioCfgInitialize error\r\n");
            return;
        }

        FIOMuxInit();
        FIOPadSetGpioMux(FGPIO_CTRL_3, FGPIO_PIN_1);
        FGpioSetDirection(&buzzer_gpio, FGPIO_DIR_OUTPUT);

     
        FGpioSetOutputValue(&buzzer_gpio, 1);

        buzzer_init_flag = 1;
    }
}

void Buzzer_Set(int flag) 
{
    
    if(0 == buzzer_init_flag) {
        Buzzer_Init();
    }

    if (flag == 1) {
        FGpioSetOutputValue(&buzzer_gpio, 0);
    } else {
        FGpioSetOutputValue(&buzzer_gpio, 1);
    }
}
