/**
 * @file      aht20.c
 * @brief     AHT20 温湿度传感器 I2C 驱动（适配旧版飞腾 Standalone SDK）
 * @note      飞腾派 PE2204 的 I2C 通过 MIO 多功能IO 实现
 *            Pin3 = I2C1_SDA, Pin5 = I2C1_SCL -> MIO1
 *            旧版 SDK 使用 FI2cMasterReadPoll / FI2cMasterWritePoll
 */

#include "aht20.h"
#include "fi2c.h"
#include "fmio.h"
#include "fmio_hw.h"
#include "fio_mux.h"
#include "fiopad.h"
#include "fsleep.h"
#include "fparameters.h"
#include <string.h>
#include <stdio.h>

static FMioCtrl mio_instance;
static FI2c i2c_instance;
static int aht20_init_ok = 0;
static int aht20_present = 1; // 默认认为存在

FError AHT20_Init(void)
{
    if (aht20_init_ok) return FT_SUCCESS;
    if (!aht20_present) return FT_SUCCESS; // 如果之前探测到设备不存在，直接返回成功

    FError ret;
    FI2cConfig input_cfg;

    /* 1. 初始化 MIO，将 MIO1 配置为 I2C 功能 */
    mio_instance.config = *FMioLookupConfig(AHT20_MIO_ID);
    ret = FMioFuncInit(&mio_instance, FMIO_FUNC_SET_I2C);
    if (ret != FT_SUCCESS) {
        printf("[AHT20] MIO init failed: 0x%x\r\n", ret);
        return ret;
    }

    /* 2. 配置引脚复用为 I2C (Pin3=SDA, Pin5=SCL) */
    FIOPadSetMioMux(AHT20_MIO_ID);

    /* 3. 通过 MIO 获取 I2C 基地址，配置 I2C */
    memset(&input_cfg, 0, sizeof(input_cfg));
    input_cfg.base_addr = FMioFuncGetAddress(&mio_instance, FMIO_FUNC_SET_I2C);
    input_cfg.irq_num = FMioFuncGetIrqNum(&mio_instance, FMIO_FUNC_SET_I2C);
    input_cfg.irq_prority = 0;            /* 注意：旧版SDK拼写为 irq_prority */
    input_cfg.ref_clk_hz = FMIO_CLK_FREQ_HZ;  /* 50MHz */
    input_cfg.work_mode = FI2C_MASTER;
    input_cfg.slave_addr = AHT20_I2C_ADDR;
    input_cfg.use_7bit_addr = TRUE;
    input_cfg.speed_rate = FI2C_SPEED_STANDARD_RATE;  /* 100kHz */
    input_cfg.auto_calc = TRUE;

    ret = FI2cCfgInitialize(&i2c_instance, &input_cfg);
    if (ret != FI2C_SUCCESS) {
        printf("[AHT20] I2C init failed: 0x%x\r\n", ret);
        return ret;
    }

    /* 4. 设置 I2C 速率 */
    ret = FI2cSetSpeed(&i2c_instance, FI2C_SPEED_STANDARD_RATE, TRUE);
    if (ret != FI2C_SUCCESS) {
        printf("[AHT20] I2C set speed failed: 0x%x\r\n", ret);
        return ret;
    }

    /* 等待 40ms 上电稳定 */
    fsleep_millisec(40);

    /* 探测设备是否存在，防止无物理传感器时死锁挂起 */
    ret = FI2cMasterProbeDevice(&i2c_instance);
    if (ret != FI2C_SUCCESS) {
        printf("[AHT20] Device not present on I2C bus: 0x%x. Disabling AHT20.\r\n", ret);
        aht20_present = 0;
        return FT_SUCCESS; // 返回成功，避免阻塞启动
    }

    /* 读取状态字以检测是否校准 */
    u8 status = 0;
    /* mem_addr=0, mem_byte_len=0 表示不发送寄存器地址，直接读 */
    ret = FI2cMasterReadPoll(&i2c_instance, 0, 0, &status, 1);
    if (ret != FI2C_SUCCESS) {
        printf("[AHT20] Read status failed: 0x%x\r\n", ret);
        /* 忽略错误继续初始化 */
    }

    if ((status & 0x08) == 0) {
        /* 未初始化校准，发送初始化命令 0xBE 0x08 0x00 */
        u8 cal_cmd[] = {0x08, 0x00};
        /* mem_addr=0xBE(命令字节), mem_byte_len=1, 后面跟2字节参数 */
        ret = FI2cMasterWritePoll(&i2c_instance, 0xBE, 1, cal_cmd, 2);
        if (ret != FI2C_SUCCESS) {
            printf("[AHT20] Calibrate cmd failed: 0x%x\r\n", ret);
        }
        fsleep_millisec(10);
    }

    aht20_init_ok = 1;
    printf("[AHT20] Init OK\r\n");
    return FT_SUCCESS;
}

FError AHT20_Read_Sensor(float *temp, float *humid)
{
    if (!aht20_present) {
        return FI2C_ERR_NOT_READY;
    }

    if (!aht20_init_ok) {
        FError init_ret = AHT20_Init();
        if (init_ret != FT_SUCCESS) return init_ret;
        if (!aht20_present) return FI2C_ERR_NOT_READY;
    }

    /* 触发测量：命令 0xAC, 参数 0x33 0x00 */
    u8 trigger_param[] = {0x33, 0x00};
    FError ret = FI2cMasterWritePoll(&i2c_instance, 0xAC, 1, trigger_param, 2);
    if (ret != FT_SUCCESS) {
        printf("[AHT20] Trigger measure failed: 0x%x\r\n", ret);
        return ret;
    }

    /* 等待 80ms 转换完成 */
    fsleep_millisec(80);

    /* 读取 6 字节数据（状态 + 湿度 + 温度） */
    u8 data[6] = {0};
    ret = FI2cMasterReadPoll(&i2c_instance, 0, 0, data, 6);
    if (ret != FT_SUCCESS) {
        printf("[AHT20] Read data failed: 0x%x\r\n", ret);
        return ret;
    }

    /* 校验状态位 (Bit 7: 0为闲，1为忙) */
    if (data[0] & 0x80) {
        return FI2C_ERR_NOT_READY;
    }

    /* 转换温湿度 */
    u32 raw_humid = ((u32)data[1] << 12) | ((u32)data[2] << 4) | ((u32)data[3] >> 4);
    u32 raw_temp = (((u32)data[3] & 0x0F) << 16) | ((u32)data[4] << 8) | (u32)data[5];

    *humid = ((float)raw_humid / 1048576.0f) * 100.0f;
    *temp = ((float)raw_temp / 1048576.0f) * 200.0f - 50.0f;

    return FT_SUCCESS;
}
