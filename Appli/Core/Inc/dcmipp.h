/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    dcmipp.h
  * @brief   This file contains all the function prototypes for
  *          the dcmipp.c file
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
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __DCMIPP_H__
#define __DCMIPP_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */
#ifndef MINICRAFT_GAME_ONLY
#include "IMX335.h"
#include "isp_api.h"
#include "isp_param_conf.h"
#endif

/* USER CODE END Includes */

extern DCMIPP_HandleTypeDef hdcmipp;

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

void MX_DCMIPP_Init(void);

/* USER CODE BEGIN Prototypes */
#ifndef MINICRAFT_GAME_ONLY
ISP_StatusTypeDef GetSensorInfoHelper(uint32_t Instance, ISP_SensorInfoTypeDef *SensorInfo);
ISP_StatusTypeDef SetSensorGainHelper(uint32_t Instance, int32_t Gain);
ISP_StatusTypeDef GetSensorGainHelper(uint32_t Instance, int32_t *Gain);
ISP_StatusTypeDef SetSensorExposureHelper(uint32_t Instance, int32_t Exposure);
ISP_StatusTypeDef GetSensorExposureHelper(uint32_t Instance, int32_t *Exposure);
void IMX335_Probe(uint32_t Resolution, uint32_t PixelFormat);
void DCMIPP_ISP_Init(void);
#endif
/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __DCMIPP_H__ */

