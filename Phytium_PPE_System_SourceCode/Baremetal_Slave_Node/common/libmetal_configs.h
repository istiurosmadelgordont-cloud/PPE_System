/*
 * Copyright (C) 2024, Phytium Technology Co., Ltd.   All Rights Reserved.
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
 * FilePath: libmetal_configs.h
 * Created Date: 2024-05-17 10:18:19
 * Last Modified: 2024-06-11 09:07:47
 * Description:  This file is for
 * 
 * Modify History:
 *   Ver      Who        Date               Changes
 * -----  ----------  --------  ---------------------------------
 *  1.0     liusm   2024-05-17        first version
 */

#ifndef LIBMETAL_CONFIGS_H
#define LIBMETAL_CONFIGS_H

#if defined __cplusplus
extern "C"
{
#endif

#include <metal/sys.h>
#include <metal/device.h>
#include <metal/irq.h>

#define KICK_BUS_NAME          "generic"
#define KICK_SGI_NUM_9         9
// #define KICK_SGI_NUM_10           10

/* device name for slave 00 */
#define SLAVE_00_KICK_DEV_NAME "slave_00_kick"
#define SLAVE_00_SGI           KICK_SGI_NUM_9
// #define SLAVE_01_KICK_DEV_NAME "slave_01_kick"
// #define SLAVE_01_SGI            KICK_SGI_NUM_10

#if defined __cplusplus
}
#endif

#endif /* LIBMETAL_CONFIGS_H */
