/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_filex.c
  * @author  MCD Application Team
  * @brief   FileX applicative file
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
#include "app_filex.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "sdmmc.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define SD_MEDIA_CACHE_SIZE  4096U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static FX_MEDIA sd_media;
static ALIGN_32BYTES(UCHAR sd_media_cache[SD_MEDIA_CACHE_SIZE]);
static UINT sd_media_opened = 0U;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */
/**
  * @brief  Application FileX Initialization.
  * @param  None
  * @retval int
  */
UINT MX_FileX_Init(void)
{
  UINT ret = FX_SUCCESS;
  /* USER CODE BEGIN MX_FileX_Init */

  /* USER CODE END MX_FileX_Init */

  /* Initialize FileX.  */
  fx_system_initialize();

  /* USER CODE BEGIN MX_FileX_Init 1*/
  ret = (HAL_SD_GetState(&hsd2) == HAL_SD_STATE_READY) ? SD_Card_Init() : FX_IO_ERROR;
  /* USER CODE END MX_FileX_Init 1*/

  return ret;
}

/* USER CODE BEGIN 1 */

UINT SD_Card_Init(void)
{
  UINT status;

  if (sd_media_opened != 0U)
  {
    return FX_SUCCESS;
  }

  status = fx_media_open(&sd_media, "SD_CARD", fx_stm32_sd_driver, FX_NULL,
                         sd_media_cache, sizeof(sd_media_cache));
  if (status == FX_SUCCESS)
  {
    sd_media_opened = 1U;
  }

  return status;
}

UINT SD_Card_DeInit(void)
{
  UINT status;

  if (sd_media_opened == 0U)
  {
    return FX_SUCCESS;
  }

  status = fx_media_close(&sd_media);
  if (status == FX_SUCCESS)
  {
    sd_media_opened = 0U;
  }

  return status;
}

UINT SD_File_Write(const CHAR *file_name, const VOID *data, ULONG size)
{
  FX_FILE file;
  UINT status;
  UINT close_status;

  if ((file_name == FX_NULL) || ((data == FX_NULL) && (size != 0U)))
  {
    return FX_PTR_ERROR;
  }

  if (sd_media_opened == 0U)
  {
    return FX_NOT_OPEN;
  }

  status = fx_file_create(&sd_media, (CHAR *)file_name);
  if ((status != FX_SUCCESS) && (status != FX_ALREADY_CREATED))
  {
    return status;
  }

  status = fx_file_open(&sd_media, &file, (CHAR *)file_name, FX_OPEN_FOR_WRITE);
  if (status != FX_SUCCESS)
  {
    return status;
  }

  status = fx_file_truncate(&file, 0U);
  if ((status == FX_SUCCESS) && (size != 0U))
  {
    status = fx_file_write(&file, (VOID *)data, size);
  }

  close_status = fx_file_close(&file);
  if (status == FX_SUCCESS)
  {
    status = close_status;
  }

  if (status == FX_SUCCESS)
  {
    status = fx_media_flush(&sd_media);
  }

  return status;
}

UINT SD_Card_Flush(void)
{return sd_media_opened ? fx_media_flush(&sd_media) : FX_NOT_OPEN;}

UINT SD_File_Delete(const CHAR *file_name)
{
  if(file_name==FX_NULL)return FX_PTR_ERROR;
  if(!sd_media_opened)return FX_NOT_OPEN;
  UINT result=fx_file_delete(&sd_media,(CHAR *)file_name);
  if(result==FX_SUCCESS)result=fx_media_flush(&sd_media);
  return result;
}

UINT SD_File_Exists(const CHAR *file_name)
{
  if(file_name==FX_NULL)return FX_PTR_ERROR;
  if(!sd_media_opened)return FX_NOT_OPEN;
  FX_FILE file;UINT result=fx_file_open(&sd_media,&file,(CHAR *)file_name,FX_OPEN_FOR_READ);
  if(result==FX_SUCCESS)result=fx_file_close(&file);
  return result;
}

UINT SD_File_Read(const CHAR *file_name, VOID *buffer, ULONG capacity,
                  ULONG *actual_size)
{
  FX_FILE file;
  UINT status;
  UINT close_status;

  if ((file_name == FX_NULL) || (buffer == FX_NULL) || (actual_size == FX_NULL))
  {
    return FX_PTR_ERROR;
  }

  *actual_size = 0U;
  if (sd_media_opened == 0U)
  {
    return FX_NOT_OPEN;
  }

  status = fx_file_open(&sd_media, &file, (CHAR *)file_name, FX_OPEN_FOR_READ);
  if (status != FX_SUCCESS)
  {
    return status;
  }

  status = fx_file_read(&file, buffer, capacity, actual_size);
  close_status = fx_file_close(&file);
  if (status == FX_SUCCESS)
  {
    status = close_status;
  }

  return status;
}

/* USER CODE END 1 */
