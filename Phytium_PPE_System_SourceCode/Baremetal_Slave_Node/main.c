/*
 * Copyright (C) 2025, Phytium Technology Co., Ltd.   All Rights Reserved.
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
 * ----- ------         --------    --------------------------------------
 * 1.0   huanghe        2022/06/20      first release
 * 1.1  liushengming    2023/05/16      for openamp test
 */
/***************************** Include Files *********************************/
#include <openamp/version.h>
#include <metal/version.h>
#include <stdio.h>
#include "ftypes.h"
#include "fsleep.h"
#include "fprintk.h"
#include "stdio.h"
#include "fdebug.h"
#include "fcache.h"
#include "sdkconfig.h"
#include "slaver_00_example.h"


/************************** Function Prototypes ******************************/

int main(void)
{
    printf("complier data: %s ,time: %s \r\n", __DATE__, __TIME__);
    printf("openamp lib version: %s (", openamp_version());
    printf("Major: %d, ", openamp_version_major());
    printf("Minor: %d, ", openamp_version_minor());
    printf("Patch: %d)\r\n", openamp_version_patch());

    printf("libmetal lib version: %s (", metal_ver());
    printf("Major: %d, ", metal_ver_major());
    printf("Minor: %d, ", metal_ver_minor());
    printf("Patch: %d)\r\n", metal_ver_patch());
    /*run the example*/
    return slave00_rpmsg_echo_process();
}
