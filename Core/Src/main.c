/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
#include "dma.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* USER CODE BEGIN Includes */
#include "bno08x.h"
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* USER CODE BEGIN PD */
#define CMD_ACK_READY 0x01
#define CMD_START     0xAA
#define CMD_RESET     0xFF

/* BNO08x RST pin control */
#define BNO_RST_HIGH()  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET)
#define BNO_RST_LOW()   HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET)
/* USER CODE END PD */

/* USER CODE BEGIN PV */
volatile uint8_t sensor_tick      = 0;
volatile uint8_t streaming_active = 0;
volatile uint8_t waiting_for_ack  = 1;
volatile uint8_t reset_requested  = 0;
volatile uint8_t imu_connected    = 0;
uint8_t  rx_buffer;
uint32_t last_heartbeat = 0;
/* USER CODE END PV */

void SystemClock_Config(void);

/* USER CODE BEGIN 0 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        if (rx_buffer == CMD_ACK_READY) {
            waiting_for_ack  = 0;
            streaming_active = 0;
            char msg[] = "WAITING_FOR_START\r\n";
            HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 100);
        }
        else if (rx_buffer == CMD_START && !waiting_for_ack && imu_connected) {
            streaming_active = 1;
            char msg[] = "STREAMING_START\r\n";
            HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 100);
        }
        else if (rx_buffer == CMD_RESET) {
            reset_requested = 1;
        }
        HAL_UART_Receive_IT(&huart1, &rx_buffer, 1);
    }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2) {
        sensor_tick = 1;
    }
}
/* USER CODE END 0 */

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_DMA_Init();
    MX_I2C1_Init();
    MX_USART1_UART_Init();
    MX_TIM2_Init();

    /* USER CODE BEGIN 2 */
    char msg[128];

    HAL_UART_Transmit(&huart1, (uint8_t*)"\r\nSTM32 BOOTED\r\n", 17, 100);
    HAL_Delay(100);

    /* ====== RESET BNO08x using PA1 ====== */
    HAL_UART_Transmit(&huart1, (uint8_t*)"Resetting BNO08x via PA1...\r\n", 29, 100);

    /* Pull RST low to reset */
    BNO_RST_LOW();
    HAL_Delay(50);

    /* Pull RST high to bring sensor out of reset */
    BNO_RST_HIGH();
    HAL_Delay(500);  /* Wait for sensor to boot */

    HAL_UART_Transmit(&huart1, (uint8_t*)"BNO08x reset complete!\r\n", 25, 100);

    /* ====== I2C SCANNER ====== */
    HAL_UART_Transmit(&huart1, (uint8_t*)"\r\nScanning I2C bus...\r\n", 24, 100);

    uint8_t found       = 0;
    uint8_t bno_address = 0;

    for (uint8_t addr = 0x01; addr < 0x7F; addr++) {
        if (HAL_I2C_IsDeviceReady(&hi2c1, addr << 1, 2, 20) == HAL_OK) {
            sprintf(msg, "Found device at 0x%02X\r\n", addr);
            HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 100);
            found = 1;
            if (addr == 0x4A || addr == 0x4B) {
                bno_address = addr;
                sprintf(msg, "*** BNO08x detected at 0x%02X! ***\r\n", addr);
                HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 100);
            }
        }
    }

    if (!found) {
        HAL_UART_Transmit(&huart1, (uint8_t*)"\r\nNo I2C devices found!\r\n", 26, 100);
        HAL_UART_Transmit(&huart1, (uint8_t*)"TROUBLESHOOTING:\r\n", 19, 100);
        HAL_UART_Transmit(&huart1, (uint8_t*)"1. CS pin MUST be 3.3V (NOT GND!)\r\n", 35, 100);
        HAL_UART_Transmit(&huart1, (uint8_t*)"2. PS0 and PS1 must be GND\r\n", 29, 100);
        HAL_UART_Transmit(&huart1, (uint8_t*)"3. Add 4.7kΩ pull-ups on SDA/SCL\r\n", 35, 100);
        HAL_UART_Transmit(&huart1, (uint8_t*)"4. Check VCC is 3.3V\r\n", 23, 100);
    }

    /* ====== BNO08x INITIALIZATION ====== */
    if (bno_address != 0) {
        HAL_UART_Transmit(&huart1, (uint8_t*)"\r\nInitializing BNO08x...\r\n", 27, 100);

        if (BNO08x_Init(&hi2c1) == 0) {
            HAL_UART_Transmit(&huart1, (uint8_t*)"BNO08x Init OK!\r\n", 18, 100);

            if (BNO08x_EnableRotationVector(&hi2c1) == 0) {
                imu_connected = 1;
                HAL_UART_Transmit(&huart1, (uint8_t*)"Rotation Vector Enabled!\r\n", 27, 100);
            } else {
                HAL_UART_Transmit(&huart1, (uint8_t*)"Enable Rotation Vector failed!\r\n", 33, 100);
            }
        } else {
            sprintf(msg, "BNO08x init failed! I2C state: %d\r\n",
                    (int)HAL_I2C_GetState(&hi2c1));
            HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 100);
        }
    }

    /* ====== START UART RECEIVE ====== */
    HAL_UART_Receive_IT(&huart1, &rx_buffer, 1);
    HAL_TIM_Base_Start_IT(&htim2);

    if (imu_connected) {
        HAL_UART_Transmit(&huart1, (uint8_t*)"\r\nWAITING_FOR_PYTHON_ACK\r\n", 27, 100);
        HAL_UART_Transmit(&huart1, (uint8_t*)"Send 0xAA from Python to start streaming\r\n", 44, 100);
    } else {
        HAL_UART_Transmit(&huart1, (uint8_t*)"IMU_NOT_FOUND\r\n", 15, 100);
    }
    /* USER CODE END 2 */

    while (1)
    {
        /* USER CODE BEGIN 3 */
        if (reset_requested) {
            reset_requested  = 0;
            waiting_for_ack  = 1;
            streaming_active = 0;
            HAL_UART_Transmit(&huart1, (uint8_t*)"RESET_ACK\r\n", 11, 100);

            /* Also reset BNO08x hardware */
            BNO_RST_LOW();
            HAL_Delay(50);
            BNO_RST_HIGH();
            HAL_Delay(500);
        }

        uint32_t now = HAL_GetTick();
        if (waiting_for_ack && imu_connected) {
            if (now - last_heartbeat > 1000) {
                last_heartbeat = now;
                HAL_UART_Transmit(&huart1,
                    (uint8_t*)"WAITING_FOR_PYTHON_ACK\r\n", 24, 100);
            }
        }

        /* Read and stream IMU data */
        if (streaming_active && imu_connected && sensor_tick) {
            sensor_tick = 0;

            float w, x, y, z;
            if (BNO08x_ReadQuat(&hi2c1, &w, &x, &y, &z) == 0) {
                sprintf(msg, "Q:%.4f,%.4f,%.4f,%.4f\r\n", w, x, y, z);
                HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 100);
            }
        }
        /* USER CODE END 3 */
    }
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState            = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue      = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL          = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
        Error_Handler();
    }
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) {}
#endif
