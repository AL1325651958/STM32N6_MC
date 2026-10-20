#include "gt911.h"

#include <string.h>

#define GT911_ADDR1                  (0x5DU << 1)
#define GT911_ADDR2                  (0x14U << 1)

#define GT911_REG_PRODUCT_ID         0x8140U
#define GT911_REG_X_OUTPUT_MAX       0x8146U
#define GT911_REG_Y_OUTPUT_MAX       0x8148U
#define GT911_REG_STATUS             0x814EU
#define GT911_REG_FIRST_POINT        0x814FU

#define GT911_STATUS_READY_MASK      0x80U
#define GT911_STATUS_TOUCH_MASK      0x0FU

#define GT911_POINT_INFO_SIZE        8U
#define GT911_I2C_TIMEOUT_MS         20U
#define GT911_LCD_WIDTH              800U
#define GT911_LCD_HEIGHT             480U

static I2C_HandleTypeDef *s_gt911_i2c = NULL;
static uint16_t s_gt911_addr = 0U;
static uint16_t s_x_max = 0U;
static uint16_t s_y_max = 0U;
static uint8_t s_product_id[5] = "0000";

static uint16_t gt911_u16_le(const uint8_t *buf)
{
  return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}

static uint16_t gt911_scale(uint16_t value, uint16_t in_max, uint16_t out_max)
{
  if ((in_max <= 1U) || (out_max == 0U))
  {
    return 0U;
  }

  if (value >= in_max)
  {
    value = (uint16_t)(in_max - 1U);
  }

  return (uint16_t)(((uint32_t)value * (uint32_t)(out_max - 1U)) / (uint32_t)(in_max - 1U));
}

static void gt911_map_point(uint16_t raw_x, uint16_t raw_y, uint16_t *x, uint16_t *y)
{
  if ((x == NULL) || (y == NULL))
  {
    return;
  }

  if ((s_x_max == GT911_LCD_HEIGHT) && (s_y_max == GT911_LCD_WIDTH))
  {
    *x = gt911_scale(raw_y, s_y_max, GT911_LCD_WIDTH);
    *y = gt911_scale((uint16_t)(s_x_max - 1U - raw_x), s_x_max, GT911_LCD_HEIGHT);
  }
  else
  {
    *x = gt911_scale(raw_x, s_x_max, GT911_LCD_WIDTH);
    *y = gt911_scale(raw_y, s_y_max, GT911_LCD_HEIGHT);
  }
}

static HAL_StatusTypeDef gt911_read(uint16_t reg, uint8_t *data, uint16_t len)
{
  if ((s_gt911_i2c == NULL) || (data == NULL) || (len == 0U))
  {
    return HAL_ERROR;
  }

  return HAL_I2C_Mem_Read(s_gt911_i2c,
                          s_gt911_addr,
                          reg,
                          I2C_MEMADD_SIZE_16BIT,
                          data,
                          len,
                          GT911_I2C_TIMEOUT_MS);
}

static HAL_StatusTypeDef gt911_write(uint16_t reg, uint8_t value)
{
  if (s_gt911_i2c == NULL)
  {
    return HAL_ERROR;
  }

  return HAL_I2C_Mem_Write(s_gt911_i2c,
                           s_gt911_addr,
                           reg,
                           I2C_MEMADD_SIZE_16BIT,
                           &value,
                           1U,
                           GT911_I2C_TIMEOUT_MS);
}

static GT911_StatusTypeDef gt911_try_addr(uint16_t addr)
{
  uint8_t id[4];
  uint8_t range_buf[4];

  s_gt911_addr = addr;

  if (HAL_I2C_IsDeviceReady(s_gt911_i2c, s_gt911_addr, 2U, GT911_I2C_TIMEOUT_MS) != HAL_OK)
  {
    return GT911_NOT_FOUND;
  }

  if (gt911_read(GT911_REG_PRODUCT_ID, id, sizeof(id)) != HAL_OK)
  {
    return GT911_NOT_FOUND;
  }

  if (gt911_read(GT911_REG_X_OUTPUT_MAX, range_buf, sizeof(range_buf)) != HAL_OK)
  {
    return GT911_ERROR;
  }

  memcpy(s_product_id, id, sizeof(id));
  s_product_id[4] = '\0';
  s_x_max = gt911_u16_le(&range_buf[0]);
  s_y_max = gt911_u16_le(&range_buf[2]);

  return GT911_OK;
}

GT911_StatusTypeDef GT911_Init(I2C_HandleTypeDef *hi2c)
{
  static const uint16_t probe_addrs[] = { GT911_ADDR1, GT911_ADDR2 };
  uint32_t i;

  if (hi2c == NULL)
  {
    return GT911_INVALID_PARAM;
  }

  s_gt911_i2c = hi2c;
  s_gt911_addr = 0U;
  s_x_max = 0U;
  s_y_max = 0U;
  memcpy(s_product_id, "0000", sizeof(s_product_id));

  for (i = 0; i < (sizeof(probe_addrs) / sizeof(probe_addrs[0])); ++i)
  {
    if (gt911_try_addr(probe_addrs[i]) == GT911_OK)
    {
      return GT911_OK;
    }
  }

  s_gt911_i2c = NULL;
  s_gt911_addr = 0U;
  return GT911_NOT_FOUND;
}

GT911_StatusTypeDef GT911_ReadTouches(GT911_StateTypeDef *state)
{
  uint8_t status;
  uint8_t point_buf[GT911_MAX_TOUCHES * GT911_POINT_INFO_SIZE];
  uint8_t touch_count;
  uint32_t i;

  if ((state == NULL) || (s_gt911_i2c == NULL) || (s_gt911_addr == 0U))
  {
    return GT911_INVALID_PARAM;
  }

  if (gt911_read(GT911_REG_STATUS, &status, 1U) != HAL_OK)
  {
    return GT911_ERROR;
  }

  if ((status & GT911_STATUS_READY_MASK) == 0U)
  {
    /* Keep the previous contacts until a fresh controller report arrives. */
    return GT911_NO_DATA;
  }

  memset(state, 0, sizeof(*state));
  memcpy(state->product_id, s_product_id, sizeof(state->product_id));
  state->x_max = s_x_max;
  state->y_max = s_y_max;

  touch_count = status & GT911_STATUS_TOUCH_MASK;
  if (touch_count > GT911_MAX_TOUCHES)
  {
    touch_count = GT911_MAX_TOUCHES;
  }

  if ((touch_count > 0U) &&
      (gt911_read(GT911_REG_FIRST_POINT,
                  point_buf,
                  (uint16_t)touch_count * GT911_POINT_INFO_SIZE) != HAL_OK))
  {
    return GT911_ERROR;
  }

  state->touch_count = touch_count;

  for (i = 0; i < touch_count; ++i)
  {
    const uint8_t *entry = &point_buf[i * GT911_POINT_INFO_SIZE];

    state->points[i].track_id = entry[0];
    state->points[i].raw_x = gt911_u16_le(&entry[1]);
    state->points[i].raw_y = gt911_u16_le(&entry[3]);
    gt911_map_point(state->points[i].raw_x,
                    state->points[i].raw_y,
                    &state->points[i].x,
                    &state->points[i].y);
    state->points[i].size = gt911_u16_le(&entry[5]);
  }

  if (gt911_write(GT911_REG_STATUS, 0U) != HAL_OK)
  {
    return GT911_ERROR;
  }

  return GT911_OK;
}

uint16_t GT911_GetAddress(void)
{
  return s_gt911_addr;
}
