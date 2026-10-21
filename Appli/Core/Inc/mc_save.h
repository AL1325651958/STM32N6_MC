#ifndef MC_SAVE_H
#define MC_SAVE_H
#include "minicraft.h"
#define MC_SAVE_VERSION 7U
#define MC_SAVE_HEADER 64U
#define MC_SAVE_V1_SIZE (MC_SAVE_HEADER + MC_MAP_BYTES)
#define MC_SAVE_V2_META 128U
#define MC_SAVE_V2_SIZE (MC_SAVE_V1_SIZE + MC_SAVE_V2_META)
#define MC_SAVE_V3_META 1280U
#define MC_SAVE_META 1536U
#define MC_SAVE_SIZE (MC_SAVE_V1_SIZE + MC_SAVE_META)
#define MC_SAVE_V3_SIZE (MC_SAVE_V1_SIZE + MC_SAVE_V3_META)
/* Little-endian format; independent of struct padding. Zero means invalid. */
void MC_SaveEncode(const MC_World *world, uint32_t generation, uint8_t *buffer);
uint32_t MC_SaveValidate(const uint8_t *buffer, unsigned length);
int MC_SaveDecode(MC_World *world, const uint8_t *buffer, unsigned length);
/* remove: 0 on success/already absent; sync: 0 on success; absent: 1 only
   when absence is confirmed (errors must return 0). No directory traversal. */
typedef struct {
  void *context;
  int (*remove)(void *context,const char *name);
  int (*sync)(void *context);
  int (*absent)(void *context,const char *name);
} MC_Storage;
const char *MC_WorldFile(unsigned bank,unsigned slot);
int MC_DeleteWorld(unsigned bank,const MC_Storage *storage);
#endif
