#ifndef MC_HOST_SDMMC_H
#define MC_HOST_SDMMC_H
#define ALIGN_32BYTES(declaration) declaration __attribute__((aligned(32)))
#define HAL_SD_STATE_READY 1
static int hsd2;
static inline int HAL_SD_GetState(const int *handle){(void)handle;return HAL_SD_STATE_READY;}
#endif
