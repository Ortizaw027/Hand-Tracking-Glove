/*
 * sh2_hal_stm32.c
 *
 * Created on: May 26, 2026
 * Author: Ortizaw
 */
#include "sh2_hal.h"
#include "main.h"
#include <string.h>

/* Bring in the hardware handles from your main configuration */
extern I2C_HandleTypeDef hi2c1;
extern TIM_HandleTypeDef htim2;

/* Target I2C address for the BNO08x */
#define BNO08x_ADDR  (0x4A << 1)

/* Hardware Pin Assignments */
#define BNO_RST_PORT   GPIOA
#define BNO_RST_PIN    GPIO_PIN_1
#define BNO_INT_PORT   GPIOA
#define BNO_INT_PIN     GPIO_PIN_0

/* 1. Microsecond Clock Wrapper
 * Connects directly to the raw TIM2 counter register configured at 1MHz.
 */
uint32_t stm32_getTimeUs(sh2_Hal_t *self) {
    return __HAL_TIM_GET_COUNTER(&htim2);
}

/* 2. Hardware Boot & Reset */
int stm32_hal_open(sh2_Hal_t *self) {
    HAL_GPIO_WritePin(BNO_RST_PORT, BNO_RST_PIN, GPIO_PIN_RESET);
    HAL_Delay(50);
    HAL_GPIO_WritePin(BNO_RST_PORT, BNO_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(300);
    return 0;
}

void stm32_hal_close(sh2_Hal_t *self) {
    HAL_GPIO_WritePin(BNO_RST_PORT, BNO_RST_PIN, GPIO_PIN_RESET);
}

/* 3. Single-Shot Safe Read Sequence
 * Pulls a hard-coded 32-byte buffer chunk off the bus in a single uninterrupted
 * Master Receive transaction to eliminate timing-sensitive bus desyncs.
 */
int stm32_hal_read(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len, uint32_t *t_us) {
    // Catch the active interrupt low state (low = data ready)
    if (HAL_GPIO_ReadPin(BNO_INT_PORT, BNO_INT_PIN) != GPIO_PIN_RESET) {
        return 0;
    }

    *t_us = stm32_getTimeUs(self);

    // Read an initial standard chunk size safely in one single transaction.
    uint8_t temp_rx[32];
    memset(temp_rx, 0, sizeof(temp_rx));

    if (HAL_I2C_Master_Receive(&hi2c1, BNO08x_ADDR, temp_rx, 32, 100) != HAL_OK) {
        return 0;
    }

    // Extract the actual packet length from the integrated header bytes
    uint16_t packet_len = temp_rx[0] | (temp_rx[1] << 8);
    packet_len &= 0x7FFF;

    // Guard against corrupt noise or empty packets
    if (packet_len <= 4 || packet_len > len) {
        return 0;
    }

    // Safely transfer the matched transaction payload straight into the CEVA buffer
    memcpy(pBuffer, temp_rx, packet_len);

    return packet_len;
}

/* 4. Standard Transmit */
int stm32_hal_write(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len) {
    if (HAL_I2C_Master_Transmit(&hi2c1, BNO08x_ADDR, pBuffer, len, 100) != HAL_OK) {
        return -1;
    }
    return len;
}

/* 5. The HAL Structure Binding */
sh2_Hal_t cevahub_hal = {
    .open = stm32_hal_open,
    .close = stm32_hal_close,
    .read = stm32_hal_read,
    .write = stm32_hal_write,
    .getTimeUs = stm32_getTimeUs
};
