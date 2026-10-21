#ifndef MC_VIEW_CACHE_H
#define MC_VIEW_CACHE_H

#include "minicraft.h"

/* Exact visible-state snapshot, separate from inventory and UI timers.
   Keep this list in step with MC_Render when adding new scene effects. */
typedef struct {float x,y,z;uint8_t active,type,hurt;} MC_ViewMob;
typedef struct {float x,y,z;uint8_t active,item;} MC_ViewDrop;
typedef struct {float x,y,z,vx,vy,vz;uint8_t active;} MC_ViewProjectile;
typedef struct {
  const MC_World *world;
  const uint8_t *blocks;
  uint32_t serial,revision,seed;
  float x,y,z,yaw,pitch,time,mining;
  int target_x,target_y,target_z;
  uint8_t target_valid,target_block;
  MC_ViewMob mobs[MC_MOBS];
  MC_ViewDrop drops[MC_DROPS];
  MC_ViewProjectile projectiles[MC_PROJECTILES];
} MC_ViewKey;
typedef struct {
  /* Owned scene buffer. Consumers may read it, but must not paint UI into it. */
  uint16_t pixels[MC_VIEW_W*MC_VIEW_H];
  MC_ViewKey key;
  uint8_t valid;
} MC_View;

/* Zero-initialize before first use. Returns 1 after a scene render, 0 when
   a paused inventory can reuse its pixels. Active gameplay always renders. */
int MC_ViewUpdate(MC_View *view,const MC_World *world);
/* Use after any external write to pixels or raw map edits outside MC_Set. */
void MC_ViewInvalidate(MC_View *view);
#endif
