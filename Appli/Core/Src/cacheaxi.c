/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    cacheaxi.c
  * @brief   This file provides code for the configuration
  *          of the CACHEAXI instances.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "cacheaxi.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

CACHEAXI_HandleTypeDef hcacheaxi;

/* CACHEAXI init function */
void MX_CACHEAXI_Init(void)
{

  /* USER CODE BEGIN CACHEAXI_Init 0 */

  /* USER CODE END CACHEAXI_Init 0 */

  /* USER CODE BEGIN CACHEAXI_Init 1 */

  /* USER CODE END CACHEAXI_Init 1 */
  hcacheaxi.Instance = CACHEAXI;
  if (HAL_CACHEAXI_Init(&hcacheaxi) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CACHEAXI_Init 2 */

  /* USER CODE END CACHEAXI_Init 2 */

}

void HAL_CACHEAXI_MspInit(CACHEAXI_HandleTypeDef* cacheaxiHandle)
{

  if(cacheaxiHandle->Instance==CACHEAXI)
  {
  /* USER CODE BEGIN CACHEAXI_MspInit 0 */

  /* USER CODE END CACHEAXI_MspInit 0 */
    /* CACHEAXI clock enable */
    __HAL_RCC_CACHEAXI_CLK_ENABLE();
  /* USER CODE BEGIN CACHEAXI_MspInit 1 */

  /* USER CODE END CACHEAXI_MspInit 1 */
  }
}

void HAL_CACHEAXI_MspDeInit(CACHEAXI_HandleTypeDef* cacheaxiHandle)
{

  if(cacheaxiHandle->Instance==CACHEAXI)
  {
  /* USER CODE BEGIN CACHEAXI_MspDeInit 0 */

  /* USER CODE END CACHEAXI_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_CACHEAXI_CLK_DISABLE();
  /* USER CODE BEGIN CACHEAXI_MspDeInit 1 */

  /* USER CODE END CACHEAXI_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

