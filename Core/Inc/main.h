/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"

/* USER CODE BEGIN Includes */
/* USER CODE END Includes */

/* USER CODE BEGIN ET */
/* USER CODE END ET */

/* USER CODE BEGIN EC */
/* USER CODE END EC */

/* USER CODE BEGIN EM */
/* USER CODE END EM */

void Error_Handler(void);

/* USER CODE BEGIN EFP */
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define BNO_INT_Pin GPIO_PIN_0
#define BNO_INT_GPIO_Port GPIOA
#define BNO_INT_EXTI_IRQn EXTI0_IRQn
#define BNO_RST_Pin GPIO_PIN_1
#define BNO_RST_GPIO_Port GPIOA

/* USER CODE BEGIN Private defines */
#define BNO_RST_On   HAL_GPIO_WritePin(BNO_RST_GPIO_Port, BNO_RST_Pin, GPIO_PIN_SET)
#define BNO_RST_Off  HAL_GPIO_WritePin(BNO_RST_GPIO_Port, BNO_RST_Pin, GPIO_PIN_RESET)
#define BNO_BOOT_On
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
