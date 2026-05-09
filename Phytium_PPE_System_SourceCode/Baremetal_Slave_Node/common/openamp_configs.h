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
 * FilePath: memory_attr.h
 * Created Date: 2024-05-06 19:20:51
 * Last Modified: 2024-06-24 16:05:11
 * Description:  This file is for
 * 
 * Modify History:
 *  Ver      Who        Date               Changes
 * -----  ----------  --------  ---------------------------------
 */

#ifndef OPENAMP_CONFIGS_H
#define OPENAMP_CONFIGS_H

#include "fmmu.h"
#include "memory_layout.h"

#if defined __cplusplus
extern "C"
{
#endif

/* 从核发送消息时，需要指定发送的cpu的核号，用来确定软件中断的发送到哪个核上 */

#define MASTER_CORE_MASK \
    255 /* 采用协商好的方式，给所有核心都发送中断，注意：裸机接口层PhytiumProcNotify()做了区分 */

#if defined __cplusplus
}
#endif

#endif /* OPENAMP_CONFIGS_H */
