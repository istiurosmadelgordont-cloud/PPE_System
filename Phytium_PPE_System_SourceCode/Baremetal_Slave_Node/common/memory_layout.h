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
 * FilePath: memory_
 * Created Date: 2024-04-29 14:22:47
 * Last Modified: 2025-01-17 18:52:12
 * Description:  This file is for
 * 
 * Modify History:
 *  Ver      Who        Date               Changes
 * -----  ----------  --------  ---------------------------------
 */

#ifndef MEMORY_LAYOUT_H
#define MEMORY_LAYOUT_H


#if defined __cplusplus
extern "C"
{
#endif

/*slave core0*/
/* 与linux共享的内存 */
#define SLAVE00_SOURCE_TABLE_ADDR      0xc0000000 /*与linux协商好的地址*/
#define SLAVE00_KICK_IO_ADDR           0xc0224000

/* MEM = |tx vring|rx vring|share buffer| */
#define SLAVE00_SHARE_MEM_ADDR         0xffffffff /*全F表示等待linux分配*/
#define SLAVE00_SHARE_MEM_SIZE         0x100000   /*共享内存大小*/
#define SLAVE00_VRING_SIZE             0x8000UL
#define SLAVE00_VRING_NUM              0x100
#define SLAVE00_TX_VRING_ADDR          0xffffffff /*全F表示等待linux分配*/
#define SLAVE00_RX_VRING_ADDR          0xffffffff /*全F表示等待linux分配*/

#define SLAVE00_SOURCE_TABLE_ATTRIBUTE (MT_NORMAL | MT_P_RW_U_NA)
#define SLAVE00_SHARE_BUFFER_ATTRIBUTE (MT_NORMAL | MT_P_RW_U_NA)

#if defined __cplusplus
}
#endif

#endif /* MEMORY_LAYOUT_H */
