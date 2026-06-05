/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "sh2.h"
#include "sh2_SensorValue.h"
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define CMD_ACK_READY 0x01
#define CMD_START     0xAA
#define CMD_RESET     0xFF

/* BNO08x RST pin control */
#define BNO_RST_HIGH()  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET)
#define BNO_RST_LOW()   HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET)

/* BNO08x INT pin control */
#define BNO_INT_PORT    GPIOA
#define BNO_INT_PIN     GPIO_PIN_0
/* USER CODE END PD */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
extern sh2_Hal_t cevahub_hal;
extern TIM_HandleTypeDef htim2;

volatile uint8_t streaming_active   = 0;
volatile uint8_t waiting_for_start   = 0;
volatile uint8_t init_sensor_trigger = 0;
volatile uint8_t reset_requested     = 0;
volatile uint8_t imu_connected       = 0;

uint8_t  bno_address = 0;
uint8_t  rx_buffer;
char msg[128];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void sensor_event_handler(void *cookie, sh2_SensorEvent_t *pEvent)
{
    sh2_SensorValue_t value;

    if (sh2_decodeSensorEvent(&value, pEvent) != 0) {
        return;
    }

    if (value.sensorId == SH2_ROTATION_VECTOR && streaming_active && imu_connected) {
        // Safe integer parsing: Scaled up x10000 to keep precision without breaking the runtime
        int32_t w_int = (int32_t)(value.un.rotationVector.real * 10000.0f);
        int32_t x_int = (int32_t)(value.un.rotationVector.i * 10000.0f);
        int32_t y_int = (int32_t)(value.un.rotationVector.j * 10000.0f);
        int32_t z_int = (int32_t)(value.un.rotationVector.k * 10000.0f);

        sprintf(msg, "Q:%ld,%ld,%ld,%ld\r\n", w_int, x_int, y_int, z_int);
        HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 10);
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        if (rx_buffer == CMD_ACK_READY) {
            waiting_for_start = 1;
            streaming_active  = 0;
            char msg_out[] = "WAITING_FOR_START\r\n";
            HAL_UART_Transmit(&huart1, (uint8_t*)msg_out, strlen(msg_out), 50);
        }
        else if (rx_buffer == CMD_START && waiting_for_start) {
            init_sensor_trigger = 1;
            waiting_for_start   = 0;
        }
        else if (rx_buffer == CMD_RESET) {
            reset_requested = 1;
        }
        HAL_UART_Receive_IT(&huart1, &rx_buffer, 1);
    }
}

uint8_t init_ceva_sensor(void)
{
    if (bno_address == 0) return 0;

    if (sh2_open(&cevahub_hal, NULL, NULL) == 0) {
        sh2_setSensorCallback(sensor_event_handler, NULL);

        sh2_SensorConfig_t config;
        config.changeSensitivityEnabled = false;
        config.wakeupEnabled            = false;
        config.alwaysOnEnabled          = false;
        config.changeSensitivity        = 0;
        config.reportInterval_us        = 16667; /* ~60Hz */
        config.batchInterval_us         = 0;
        config.sensorSpecific           = 0;

        if (sh2_setSensorConfig(SH2_ROTATION_VECTOR, &config) == 0) {
            return 1;
        }
    }
    return 0;
}
/* USER CODE END 0 */

int main(void)
{
  HAL_Init();
  SystemClock_Config();

  /* Initialize peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  MX_TIM2_Init();

  /* USER CODE BEGIN 2 */
      HAL_TIM_Base_Start(&htim2);

      HAL_UART_Transmit(&huart1, (uint8_t*)"\r\nSTM32 BOOTED\r\n", 17, 100);

      /* Hard Reset Line Pulse to force I2C bus synchronization */
      BNO_RST_LOW();
      HAL_Delay(30);
      BNO_RST_HIGH();
      HAL_Delay(150);

      /* Scan Loop */
      for (uint8_t addr = 0x01; addr < 0x7F; addr++) {
          if (HAL_I2C_IsDeviceReady(&hi2c1, addr << 1, 2, 10) == HAL_OK) {
              if (addr == 0x4A || addr == 0x4B) {
                  bno_address = addr;
                  sprintf(msg, "Target Match: BNO08x locked at address 0x%02X\r\n", addr);
                  HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 100);
              }
          }
      }

      if (bno_address == 0) {
          HAL_UART_Transmit(&huart1, (uint8_t*)"NO BNO08x FOUND\r\n", 17, 100);
      } else {
          if (init_ceva_sensor()) {
              imu_connected    = 1;
              streaming_active = 1;
              HAL_UART_Transmit(&huart1, (uint8_t*)"INIT_SUCCESS: Streaming Active!\r\n", 33, 100);
          } else {
              HAL_UART_Transmit(&huart1, (uint8_t*)"INIT_FAILED\r\n", 13, 100);
          }
      }

      HAL_UART_Receive_IT(&huart1, &rx_buffer, 1);
  /* USER CODE END 2 */

  while (1)
  {
    /* USER CODE BEGIN 3 */
        if (imu_connected && streaming_active) {
            sh2_service();
        }

        if (init_sensor_trigger) {
            init_sensor_trigger = 0;
            if (init_ceva_sensor()) {
                imu_connected    = 1;
                streaming_active = 1;
            }
        }

        if (reset_requested) {
            reset_requested   = 0;
            streaming_active  = 0;
            waiting_for_start = 0;
            if (imu_connected) {
                sh2_close();
                imu_connected = 0;
            }
        }
  } /* USER CODE END 3 */
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL2;
  HAL_RCC_OscConfig(&RCC_OscInitStruct);

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0);
}

void Error_Handler(void)
{
  __disable_irq();
  while (1) {}
}
