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
 * FilePath: slaver_00_example.h
 * Created Date: 2024-04-29 16:17:53
 * Last Modified: 2024-06-07 10:35:22
 * Description:  This file is for
 * 
 * Modify History:
 *  Ver      Who        Date               Changes
 * -----  ----------  --------  ---------------------------------
 */

#ifndef __BUZZER_H__
#define __BUZZER_H__

/* 初始化蜂鸣器引脚 */
void Buzzer_Init(void);

/* 控制蜂鸣器状态：1为响，0为停 */
void Buzzer_Set(int flag);

#endif
