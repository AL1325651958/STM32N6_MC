/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    dcmipp.c
  * @brief   This file provides code for the configuration
  *          of the DCMIPP instances.
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
#include "dcmipp.h"

/* USER CODE BEGIN 0 */
#ifndef MINICRAFT_GAME_ONLY
#include "i2c.h"
#include "imx335.h"
#include "main.h"
void IMX335_Probe(uint32_t Resolution, uint32_t PixelFormat);
#include "isp_api.h"
#include "isp_param_conf.h"
#define CAMERA_IMX335_ADDRESS 0x34U
extern uint8_t lcd_bg_buffer[800 * 480 * 4];
#endif

/* USER CODE END 0 */

DCMIPP_HandleTypeDef hdcmipp;

/* DCMIPP init function */
void MX_DCMIPP_Init(void)
{

  /* USER CODE BEGIN DCMIPP_Init 0 */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_8, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_SET);
  HAL_Delay(3);



  /* USER CODE END DCMIPP_Init 0 */

  DCMIPP_CSI_PIPE_ConfTypeDef pCSI_PipeConfig = {0};
  DCMIPP_CSI_ConfTypeDef pCSI_Config = {0};
  DCMIPP_PipeConfTypeDef pPipeConfig = {0};

  /* USER CODE BEGIN DCMIPP_Init 1 */

  /* USER CODE END DCMIPP_Init 1 */
  hdcmipp.Instance = DCMIPP;
  if (HAL_DCMIPP_Init(&hdcmipp) != HAL_OK)
  {
    Error_Handler();
  }

  /** Pipe 1 Config
  */
  pCSI_PipeConfig.DataTypeMode = DCMIPP_DTMODE_DTIDA;
  pCSI_PipeConfig.DataTypeIDA = DCMIPP_DT_RAW10;
  pCSI_PipeConfig.DataTypeIDB = DCMIPP_DT_RAW10;
  if (HAL_DCMIPP_CSI_PIPE_SetConfig(&hdcmipp, DCMIPP_PIPE1, &pCSI_PipeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  pCSI_Config.PHYBitrate = DCMIPP_CSI_PHY_BT_1600;
  pCSI_Config.DataLaneMapping = DCMIPP_CSI_PHYSICAL_DATA_LANES;
  pCSI_Config.NumberOfLanes = DCMIPP_CSI_TWO_DATA_LANES;
  HAL_DCMIPP_CSI_SetConfig(&hdcmipp, &pCSI_Config);
  pPipeConfig.FrameRate = DCMIPP_FRAME_RATE_ALL;
  pPipeConfig.PixelPipePitch = 2400;
  pPipeConfig.PixelPackerFormat = DCMIPP_PIXEL_PACKER_FORMAT_RGB888_YUV444_1;
  if (HAL_DCMIPP_PIPE_SetConfig(&hdcmipp, DCMIPP_PIPE1, &pPipeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_DCMIPP_CSI_SetVCConfig(&hdcmipp, 0U, DCMIPP_CSI_DT_BPP10) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN DCMIPP_Init 2 */
  DCMIPP_DownsizeTypeDef DownsizeConf = {0};
DownsizeConf.HRatio      = 25656;
DownsizeConf.VRatio      = 33161;
DownsizeConf.HSize       = 800;
DownsizeConf.VSize       = 480;
DownsizeConf.HDivFactor  = 316;
DownsizeConf.VDivFactor  = 253;

if(HAL_DCMIPP_PIPE_SetDownsizeConfig(&hdcmipp, DCMIPP_PIPE1, &DownsizeConf) != HAL_OK)
{
  Error_Handler();
}
if(HAL_DCMIPP_PIPE_EnableDownsize(&hdcmipp, DCMIPP_PIPE1) != HAL_OK)
{
  Error_Handler();
}

  /* USER CODE BEGIN DCMIPP_Init 2 */

  /* USER CODE END DCMIPP_Init 2 */

}

void HAL_DCMIPP_MspInit(DCMIPP_HandleTypeDef* dcmippHandle)
{

  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
  if(dcmippHandle->Instance==DCMIPP)
  {
  /* USER CODE BEGIN DCMIPP_MspInit 0 */

  /* USER CODE END DCMIPP_MspInit 0 */

  /** Initializes the peripherals clock
  */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_DCMIPP|RCC_PERIPHCLK_CSI;
    PeriphClkInitStruct.DcmippClockSelection = RCC_DCMIPPCLKSOURCE_IC17;
    PeriphClkInitStruct.ICSelection[RCC_IC17].ClockSelection = RCC_ICCLKSOURCE_PLL3;
    PeriphClkInitStruct.ICSelection[RCC_IC17].ClockDivider = 4;
    PeriphClkInitStruct.ICSelection[RCC_IC18].ClockSelection = RCC_ICCLKSOURCE_PLL3;
    PeriphClkInitStruct.ICSelection[RCC_IC18].ClockDivider = 60;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
      Error_Handler();
    }

    /* DCMIPP clock enable */
    __HAL_RCC_DCMIPP_CLK_ENABLE();
    __HAL_RCC_CSI_CLK_ENABLE();
    __HAL_RCC_CSI_FORCE_RESET();
    __HAL_RCC_CSI_RELEASE_RESET();

    /* DCMIPP interrupt Init */
    HAL_NVIC_SetPriority(DCMIPP_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DCMIPP_IRQn);
    HAL_NVIC_SetPriority(CSI_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(CSI_IRQn);
  /* USER CODE BEGIN DCMIPP_MspInit 1 */





  /* USER CODE END DCMIPP_MspInit 1 */
  }
}

void HAL_DCMIPP_MspDeInit(DCMIPP_HandleTypeDef* dcmippHandle)
{

  if(dcmippHandle->Instance==DCMIPP)
  {
  /* USER CODE BEGIN DCMIPP_MspDeInit 0 */

  /* USER CODE END DCMIPP_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_CSI_CLK_DISABLE();
    __HAL_RCC_CSI_FORCE_RESET();
    __HAL_RCC_CSI_RELEASE_RESET();

    /* DCMIPP interrupt Deinit */
    HAL_NVIC_DisableIRQ(DCMIPP_IRQn);
    HAL_NVIC_DisableIRQ(CSI_IRQn);
  /* USER CODE BEGIN DCMIPP_MspDeInit 1 */

  /* USER CODE END DCMIPP_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */
#ifndef MINICRAFT_GAME_ONLY

#define IMX335_I2C_ADDRESS    (0x1A << 1)

static int32_t isp_gain     = 0;    // mdb 单位 (×1000)
static int32_t isp_exposure = 0;    // 微秒 us

int NbMainFrames = 0;
ISP_HandleTypeDef hcamera_isp;

/**
 * @brief 初始化 DCMIPP + ISP + IMX335 传感器
 */
void DCMIPP_ISP_Init(void)
{
    ISP_AppliHelpersTypeDef appliHelpers = {0};
    ISP_StatAreaTypeDef statArea = {0};
    // 填充 ISP middleware 需要的回调函数
    appliHelpers.GetSensorInfo    = GetSensorInfoHelper;
    appliHelpers.SetSensorGain    = SetSensorGainHelper;
    appliHelpers.GetSensorGain    = GetSensorGainHelper;
    appliHelpers.SetSensorExposure = SetSensorExposureHelper;
    appliHelpers.GetSensorExposure = GetSensorExposureHelper;
    // 统计区域（全画幅）
    statArea.X0    = 0;
    statArea.Y0    = 0;
    statArea.XSize = 2592;
    statArea.YSize = 1944;
    // 初始化 ISP 中间件
    if (ISP_Init(&hcamera_isp, &hdcmipp, 0, &appliHelpers, &statArea, ISP_IQParamCacheInit[0]) != ISP_OK)
    {
        Error_Handler();
    }

    // 启动 DCMIPP CSI PIPE
    if (HAL_DCMIPP_CSI_PIPE_Start(&hdcmipp, DCMIPP_PIPE1, DCMIPP_VIRTUAL_CHANNEL0,
                                  (uint32_t)lcd_bg_buffer, DCMIPP_MODE_CONTINUOUS) != HAL_OK)
    {
        Error_Handler();
    }

    // 启动 ISP 处理
    if (ISP_Start(&hcamera_isp) != ISP_OK)
    {
        Error_Handler();
    }

    while (NbMainFrames < 60)
    {
        if (ISP_BackgroundProcess(&hcamera_isp) != ISP_OK)
        {

        }
    }
}


/**
 * @brief ISP Middleware helper: 获取传感器信息
 */
ISP_StatusTypeDef GetSensorInfoHelper(uint32_t Instance, ISP_SensorInfoTypeDef *SensorInfo)
{
    (void)Instance;  // 未使用参数

    IMX335_SensorInfo_t info = {0};

    if (IMX335_GetSensorInfo(&info) != IMX335_OK)
    {
        return ISP_ERR_SENSORINFO;   // 如果你的 isp.h 里没有定义 ISP_ERROR，可以改成 -1 或 ISP_OK
    }

    // 复制字符串（防止缓冲区溢出）
    strncpy(SensorInfo->name, info.name, ISP_SENSOR_INFO_MAX_LENGTH - 1);
    SensorInfo->name[ISP_SENSOR_INFO_MAX_LENGTH - 1] = '\0';

    SensorInfo->bayer_pattern  = info.bayer_pattern;   // 通常 0 = RGGB
    SensorInfo->color_depth    = info.color_depth;     // 10
    SensorInfo->width          = info.width;           // 2592
    SensorInfo->height         = info.height;          // 1944
    SensorInfo->gain_min       = info.gain_min;        // mdb 单位
    SensorInfo->gain_max       = info.gain_max;
    SensorInfo->exposure_min   = info.exposure_min;    // us
    SensorInfo->exposure_max   = info.exposure_max;

    return ISP_OK;
}

/**
 * @brief ISP Middleware helper: 设置传感器模拟增益
 */
ISP_StatusTypeDef SetSensorGainHelper(uint32_t Instance, int32_t Gain)
{
    (void)Instance;

    isp_gain = Gain;
    if (IMX335_SetGain(&hi2c1, IMX335_I2C_ADDRESS, Gain) != IMX335_OK)
    {
        return ISP_ERR_SENSORGAIN;
    }

    return ISP_OK;
}

/**
 * @brief ISP Middleware helper: 获取当前增益值（软件缓存）
 */
ISP_StatusTypeDef GetSensorGainHelper(uint32_t Instance, int32_t *Gain)
{
    (void)Instance;
    *Gain = isp_gain;
    return ISP_OK;
}

/**
 * @brief ISP Middleware helper: 设置传感器曝光时间（单位：微秒）
 */
ISP_StatusTypeDef SetSensorExposureHelper(uint32_t Instance, int32_t Exposure)
{
    (void)Instance;

    isp_exposure = Exposure;

    if (IMX335_SetExposure(&hi2c1, IMX335_I2C_ADDRESS, Exposure) != IMX335_OK)
    {
        return ISP_ERR_SENSOREXPOSURE;
    }

    return ISP_OK;
}

/**
 * @brief ISP Middleware helper: 获取当前曝光值（软件缓存）
 */
ISP_StatusTypeDef GetSensorExposureHelper(uint32_t Instance, int32_t *Exposure)
{
    (void)Instance;
    *Exposure = isp_exposure;
    return ISP_OK;
}


void HAL_DCMIPP_PIPE_VsyncEventCallback(DCMIPP_HandleTypeDef *hdcmipp, uint32_t Pipe)
{
  UNUSED(hdcmipp);
  /* Update the frame counter and call the ISP statistics handler */
  switch (Pipe)
  {
    case DCMIPP_PIPE0 :
      ISP_IncDumpFrameId(&hcamera_isp);
      break;
    case DCMIPP_PIPE1 :
      ISP_IncMainFrameId(&hcamera_isp);
      ISP_GatherStatistics(&hcamera_isp);
      NbMainFrames++;
      break;
    case DCMIPP_PIPE2 :
      ISP_IncAncillaryFrameId(&hcamera_isp);

      break;
  }
}
#endif
/* USER CODE END 1 */

