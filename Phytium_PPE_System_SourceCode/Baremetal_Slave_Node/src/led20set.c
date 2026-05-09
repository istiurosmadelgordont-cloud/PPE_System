/*
 * Copyright (C) 2022, Phytium Technology Co., Ltd.   All Rights Reserved.
 *
 * Licensed under the BSD 3-Clause License (the "License"); you may not use
 * this file except in compliance with the License. You may obtain a copy of
 * the License at
 *
 *     https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *
 * FilePath: slaver_00_example.c
 * Date: 2022-03-08 22:00:15
 * LastEditTime: 2024-02-27 17:08:19
 * Description:  This is a sample demonstration application that showcases usage of rpmsg
 *  This application is meant to run on the remote CPU running baremetal code.
 *  This application echoes back data that was sent to it by the master core.
 *
 * Modify History:
 *  Ver   Who        Date         Changes
 * ----- ------     --------    --------------------------------------
 * 1.0   huanghe    2022/06/20      first release
 * 1.1   liusm      2024/02/27      fix bug
 * 1.2   liusm      2024/06/07      update for new rpmsg framework
 * 1.3   liusm      2025/02/28      update for start/stop framework
 */

/***************************** Include Files *********************************/

#include <stdio.h>
#include "sdkconfig.h"
#include "fdebug.h"
/*common*/
#include "fio.h"
#include"ftypes.h"
#include"fkernel.h"
#include"fsleep.h"
#include "fassert.h"
/*drivers/iomux/fiopad*/
#include "fiopad.h"
/*drivers/pin/fgpio*/
#include "fgpio_hw.h"
#include "fgpio.h"
/*board/firefly*/
#include "fio_mux.h"
#include "slaver_00_example.h"

int initFLag = 0;
u32 gpIO1_8_FGpio_id ;
const FGpioConfig *gpI01_8_FGpioconfig = NULL;
FGpio gpI01_8_FGpio;



void led20Set(int flag)        
{
    /* 1. 硬件初始化阶段（只执行一次） */
    if(0 == initFLag)
    {
        gpIO1_8_FGpio_id = FGPIO_ID(FGPIO_CTRL_1,FGPIO_PIN_8);
        gpI01_8_FGpioconfig = FGpioLookupConfig(gpIO1_8_FGpio_id);
        if(NULL == gpI01_8_FGpioconfig)
        {
            printf("led20Set():FGpioLookupConfig error\r\n");
            return;
        }

        memset(&gpI01_8_FGpio, 0, sizeof(gpI01_8_FGpio));
        if(FGPIO_SUCCESS != FGpioCfgInitialize(&gpI01_8_FGpio, gpI01_8_FGpioconfig))
        {
            printf("led20Set():FGpioCfgInitialize() error\r\n");
            return;
        }

        FIOMuxInit();
        FIOPadSetGpioMux(FGPIO_CTRL_1, FGPIO_PIN_8);
        FGpioSetDirection(&gpI01_8_FGpio, FGPIO_DIR_OUTPUT);

        initFLag = 1;
    }

    /* 2. 核心执行阶段（每次调用都会执行，这才是你遗漏的灵魂！） */
    if (flag == 1) 
    {
        /* 输出高电平点亮 LED。注意：如果你的板子是低电平点亮，把这里的 1 改成 0 */
       FGpioSetOutputValue(&gpI01_8_FGpio, 1);
    } 
    else 
    {
        /* 输出低电平熄灭 LED */
       FGpioSetOutputValue(&gpI01_8_FGpio, 0);
    }
}
