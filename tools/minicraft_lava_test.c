#include "minicraft.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void scene(MC_World *w)
{
  memset(w->blocks,0,MC_MAP_BYTES);
  for(int z=0;z<24;z++)for(int x=0;x<24;x++){
    MC_Set(w,x,0,z,MC_BEDROCK);MC_Set(w,x,1,z,MC_STONE);
  }
  MC_NewPlayer(w,0);w->x=10.5f;w->y=2.01f;w->z=10.5f;w->on_ground=1;w->hurt_timer=0;
}

int main(void)
{
  MC_World w={.blocks=calloc(1,MC_MAP_BYTES)};assert(w.blocks);scene(&w);
  assert(MC_IsBlock(MC_LAVA)&&!MC_IsSolid(MC_LAVA));
  assert(MC_ItemColor(MC_LAVA)!=0&&!strcmp(MC_ItemName(MC_LAVA),"LAVA"));
  MC_Set(&w,10,3,13,MC_LAVA);w.yaw=w.pitch=0;
  MC_Hit hit;assert(MC_Pick(&w,400,240,&hit)&&hit.block==MC_LAVA);
  MC_Set(&w,10,3,10,MC_LAVA);assert(MC_BodyClear(&w,w.x,w.y,w.z));
  unsigned health=w.health;MC_PlayTick(&w,.1f,0,400,240);assert(w.health==health-3);
  MC_Set(&w,10,3,10,MC_AIR);MC_Set(&w,10,3,13,MC_AIR);MC_Set(&w,3,4,3,MC_LAVA);MC_Set(&w,3,3,3,MC_AIR);
  for(int i=0;i<3;i++)MC_SystemsTick(&w,.1f);
  assert(MC_Get(&w,3,3,3)==MC_LAVA&&MC_Get(&w,3,4,3)==MC_AIR);
  MC_Set(&w,3,3,3,MC_STONE);MC_Set(&w,3,4,3,MC_LAVA);MC_Set(&w,4,4,3,MC_AIR);
  for(int i=0;i<3;i++)MC_SystemsTick(&w,.1f);
  assert(MC_Get(&w,4,4,3)==MC_AIR);
  MC_Set(&w,3,3,3,MC_AIR);MC_Set(&w,3,4,3,MC_AIR);
  MC_Set(&w,3,2,3,MC_STONE);MC_Set(&w,4,2,3,MC_STONE);
  MC_Set(&w,3,3,3,MC_WATER);MC_Set(&w,4,3,3,MC_LAVA);
  w.fluid_clock=0;w.fluid_cursor=0;
  for(int i=0;i<3;i++)MC_SystemsTick(&w,.1f);
  assert(MC_Get(&w,4,3,3)==MC_COBBLE&&MC_Get(&w,3,3,3)==MC_WATER);
  free(w.blocks);
  puts("minicraft lava: map-only fluid identity, selection, damage, bounded downward flow and water contact: OK");
  return 0;
}
