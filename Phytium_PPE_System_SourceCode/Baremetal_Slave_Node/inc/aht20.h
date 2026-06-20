#ifndef AHT20_H
#define AHT20_H

#include "ftypes.h"
#include "ferror_code.h"

#define AHT20_I2C_ADDR      0x38  // AHT20 默认 I2C 地址
#define AHT20_MIO_ID        1     // 飞腾派 J1 Pin3(SDA)/Pin5(SCL) 对应 MIO1

FError AHT20_Init(void);
FError AHT20_Read_Sensor(float *temp, float *humid);

#endif
