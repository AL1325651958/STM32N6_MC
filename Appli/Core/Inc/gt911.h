#ifndef __GT911_H__
#define __GT911_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32n6xx_hal.h"

#define GT911_MAX_TOUCHES 5U

typedef enum
{
  GT911_OK = 0,
  GT911_NO_DATA = 1,
  GT911_ERROR = -1,
  GT911_NOT_FOUND = -2,
  GT911_INVALID_PARAM = -3
} GT911_StatusTypeDef;

typedef struct
{
  uint8_t track_id;
  uint16_t raw_x;
  uint16_t raw_y;
  uint16_t x;
  uint16_t y;
  uint16_t size;
} GT911_PointTypeDef;

typedef struct
{
  uint8_t product_id[5];
  uint16_t x_max;
  uint16_t y_max;
  uint8_t touch_count;
  GT911_PointTypeDef points[GT911_MAX_TOUCHES];
} GT911_StateTypeDef;

GT911_StatusTypeDef GT911_Init(I2C_HandleTypeDef *hi2c);
GT911_StatusTypeDef GT911_ReadTouches(GT911_StateTypeDef *state);
uint16_t GT911_GetAddress(void);

#ifdef __cplusplus
}
#endif

#endif /* __GT911_H__ */
