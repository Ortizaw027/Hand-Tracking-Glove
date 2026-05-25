#include "bno08x.h"
#include <string.h>
#include <stdio.h>

static uint8_t buffer[256];

/* Send SHTP packet */
static uint8_t send_packet(I2C_HandleTypeDef *hi2c, uint8_t channel, uint8_t *data, uint16_t len) {
    uint8_t packet[256];
    packet[0] = len & 0xFF;
    packet[1] = (len >> 8) & 0xFF;
    packet[2] = channel;
    packet[3] = 0;  /* sequence number */

    if (data && len > 0) {
        memcpy(&packet[4], data, len);
    }

    if (HAL_I2C_Master_Transmit(hi2c, BNO08x_ADDR << 1, packet, len + 4, 100) != HAL_OK) {
        return 1;
    }
    return 0;
}

/* Receive SHTP packet */
static uint8_t receive_packet(I2C_HandleTypeDef *hi2c) {
    uint8_t header[4];

    /* Read header first */
    if (HAL_I2C_Master_Receive(hi2c, BNO08x_ADDR << 1, header, 4, 100) != HAL_OK) {
        return 1;
    }

    uint16_t pkt_len = header[0] | (header[1] << 8);
    pkt_len &= 0x7FFF;  /* Clear continuation bit */

    if (pkt_len > 4 && pkt_len <= 256) {
        /* Read the rest of the packet */
        if (HAL_I2C_Master_Receive(hi2c, BNO08x_ADDR << 1, buffer, pkt_len - 4, 100) == HAL_OK) {
            return 0;
        }
    }
    return 1;
}

/* Initialize BNO08x - just basic setup */
uint8_t BNO08x_Init(I2C_HandleTypeDef *hi2c) {
    char msg[50];

    /* Wait for sensor to boot */
    HAL_Delay(500);

    /* Flush any pending data */
    for (int i = 0; i < 5; i++) {
        receive_packet(hi2c);
        HAL_Delay(10);
    }

    /* Request product ID to wake up sensor */
    uint8_t cmd[] = {0xF9, 0x00, 0x00, 0x00};
    send_packet(hi2c, 0, cmd, 4);
    HAL_Delay(100);

    /* Read response */
    for (int i = 0; i < 3; i++) {
        if (receive_packet(hi2c) == 0) {
            if (buffer[0] == 0xF8) {
                /* Got product ID response - sensor is alive */
                return 0;
            }
        }
        HAL_Delay(50);
    }

    return 1;  /* No response */
}

/* Enable rotation vector data */
uint8_t BNO08x_EnableRotationVector(I2C_HandleTypeDef *hi2c) {
    /* SET_FEATURE_COMMAND for ROTATION_VECTOR */
    uint8_t cmd[12];
    cmd[0] = 0xFD;  /* SET_FEATURE_COMMAND */
    cmd[1] = 0x05;  /* ROTATION_VECTOR report ID */
    cmd[2] = 0;     /* Feature flags */
    cmd[3] = 0;     /* Change sensitivity LSB */
    cmd[4] = 0;     /* Change sensitivity MSB */
    cmd[5] = 0x20;  /* Report interval LSB (100ms = 100000us) */
    cmd[6] = 0xA1;  /* Report interval */
    cmd[7] = 0x01;  /* Report interval */
    cmd[8] = 0x00;  /* Report interval MSB */
    cmd[9] = 0;     /* Batch interval LSB */
    cmd[10] = 0;    /* Batch interval MSB */
    cmd[11] = 0;    /* Sensor specific config */

    if (send_packet(hi2c, 0, cmd, 12) != 0) {
        return 1;
    }

    HAL_Delay(100);

    /* Flush any response packets */
    for (int i = 0; i < 3; i++) {
        receive_packet(hi2c);
        HAL_Delay(10);
    }

    return 0;
}

/* Read quaternion data */
uint8_t BNO08x_ReadQuat(I2C_HandleTypeDef *hi2c, float *w, float *x, float *y, float *z) {
    /* Try to receive a packet */
    if (receive_packet(hi2c) != 0) {
        return 1;
    }

    /* Check if this is a rotation vector report (0x05) */
    if (buffer[0] == 0x05) {
        /* Extract quaternion values (Q14 format, scale by 1/16384) */
        int16_t raw_x = (buffer[4] << 8) | buffer[3];
        int16_t raw_y = (buffer[6] << 8) | buffer[5];
        int16_t raw_z = (buffer[8] << 8) | buffer[7];
        int16_t raw_w = (buffer[10] << 8) | buffer[9];

        *x = (float)raw_x / 16384.0f;
        *y = (float)raw_y / 16384.0f;
        *z = (float)raw_z / 16384.0f;
        *w = (float)raw_w / 16384.0f;

        return 0;  /* Success */
    }

    return 1;  /* Not a rotation vector packet */
}
