#ifndef BNO08X_H
#define BNO08X_H

#include "main.h"

/* BNO08x I2C address (0x4A when ADR=GND, 0x4B when ADR=3.3V) */
#define BNO08x_ADDR  0x4A

/* Function prototypes */
uint8_t BNO08x_Init(I2C_HandleTypeDef *hi2c);
uint8_t BNO08x_EnableRotationVector(I2C_HandleTypeDef *hi2c);
uint8_t BNO08x_ReadQuat(I2C_HandleTypeDef *hi2c, float *w, float *x, float *y, float *z);

#endif
