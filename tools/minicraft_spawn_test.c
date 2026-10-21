#include "minicraft.h"
#include "mc_save.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void flat(MC_World *w)
{
  memset(w->blocks,0,MC_MAP_BYTES);MC_NewPlayer(w,0);
  for(int z=0;z<MC_MAP_Z;z++)for(int x=0;x<MC_MAP_X;x++) {
    w->blocks[z*MC_MAP_X+x]=MC_BEDROCK;MC_Set(w,x,1,z,MC_STONE);
  }
  w->x=w->z=20.5f;w->y=2.01f;
}
static float opening(const MC_World *w)
{
  const unsigned pixels[]={120,260,400,540,680};float nearest=5;
  for(unsigned i=0;i<sizeof(pixels)/sizeof(pixels[0]);i++) {
    MC_Hit hit;if(MC_Pick(w,pixels[i],240,&hit))nearest=fminf(nearest,hit.distance);
  }
  return nearest;
}
int main(void)
{
  MC_World w={.blocks=calloc(1,MC_MAP_BYTES)},restored={.blocks=malloc(MC_MAP_BYTES)};
  uint8_t *save=malloc(MC_SAVE_SIZE);assert(w.blocks&&restored.blocks&&save);
  const uint32_t seeds[]={0,1,2,77,0x20261020u,0xffffffffu,0x12345678u,20261021};
  for(unsigned seed=0;seed<sizeof(seeds)/sizeof(seeds[0]);seed++) {
    MC_Generate(&w,seeds[seed]);float x=w.x,y=w.y,z=w.z,yaw=w.yaw,pitch=w.pitch;
    assert(MC_BodyClear(&w,x,y,z)&&MC_Get(&w,(int)x,(int)floorf(y-.05f),(int)z)==MC_GRASS);
    float clear=opening(&w);assert(clear>=3);
    int near_wood=0;
    for(int dz=-24;dz<=24&&!near_wood;dz++)for(int dx=-24;dx<=24&&!near_wood;dx++)for(int yy=(int)y-8;yy<y+8&&!near_wood;yy++) {
      uint8_t b=MC_Get(&w,(int)x+dx,yy,(int)z+dz);
      if(b==MC_WOOD||b==MC_BIRCH_WOOD||b==MC_DARK_WOOD)near_wood=1;
    }
    assert(near_wood);
    uint32_t crc=MC_Crc32(w.blocks,MC_MAP_BYTES);MC_Generate(&restored,seeds[seed]);
    assert(x==restored.x&&y==restored.y&&z==restored.z&&yaw==restored.yaw&&pitch==restored.pitch);
    assert(crc==MC_Crc32(restored.blocks,MC_MAP_BYTES));
    /* Continue restores the player's own position and view, not a newly
       selected camera. It also preserves the new initial view when untouched. */
    w.yaw=2.35f;w.pitch=-.41f;MC_SaveEncode(&w,3,save);assert(MC_SaveDecode(&restored,save,MC_SAVE_SIZE));
    assert(restored.x==x&&restored.y==y&&restored.z==z&&restored.yaw==2.35f&&restored.pitch==-.41f);
    assert(crc==MC_Crc32(restored.blocks,MC_MAP_BYTES));
    printf("spawn seed %lu: visible cone %.2fm, dry grass, nearby wood, deterministic view and saved camera\n",(unsigned long)seeds[seed],clear);
  }
  /* A bed with a roof that is too low to stand on still has safe ground
     beside it; respawning must not carve or move the player's construction. */
  flat(&w);MC_Set(&w,20,2,20,MC_BED);MC_Set(&w,20,4,20,MC_STONE);
  w.spawn[0]=21;w.spawn[1]=3;w.spawn[2]=21;
  w.projectiles[0]=(MC_Projectile){20.5f,3,20.5f,0,0,18,0,1};w.target_valid=1;w.mining=.5f;
  uint32_t crc=MC_Crc32(w.blocks,MC_MAP_BYTES);assert(MC_Give(&w,MC_PLANKS,12));MC_Respawn(&w);
  assert(MC_BodyClear(&w,w.x,w.y,w.z)&&fabsf(w.y-2.01f)<.001f);
  assert(fabsf(w.x-20.5f)<=1&&fabsf(w.z-20.5f)<=1&&(w.x!=20.5f||w.z!=20.5f));
  assert(w.items[MC_PLANKS]==12&&!w.projectiles[0].active&&!w.target_valid&&!w.mining&&crc==MC_Crc32(w.blocks,MC_MAP_BYTES));
  /* No adjacent floor: the bed itself remains a supported last local option. */
  flat(&w);MC_Set(&w,20,4,20,MC_BED);w.spawn[0]=21;w.spawn[1]=5;w.spawn[2]=21;MC_Respawn(&w);
  assert(w.x==20.5f&&w.z==20.5f&&fabsf(w.y-4.5725f)<.001f&&MC_BodyClear(&w,w.x,w.y,w.z));
  /* A sealed bed falls back to dry land without deleting the bed or walls. */
  for(int z=19;z<=21;z++)for(int x=19;x<=21;x++)for(int y=4;y<=7;y++)if(x!=20||z!=20||y!=4)MC_Set(&w,x,y,z,MC_STONE);
  crc=MC_Crc32(w.blocks,MC_MAP_BYTES);MC_Respawn(&w);
  assert(MC_BodyClear(&w,w.x,w.y,w.z)&&w.x>100&&crc==MC_Crc32(w.blocks,MC_MAP_BYTES));
  flat(&w);MC_Set(&w,20,2,20,MC_BED);w.spawn[0]=21;w.spawn[1]=3;w.spawn[2]=21;
  for(int z=19;z<=21;z++)for(int x=19;x<=21;x++)for(int y=2;y<=4;y++)if(x!=20||z!=20||y!=2)MC_Set(&w,x,y,z,MC_WATER);
  crc=MC_Crc32(w.blocks,MC_MAP_BYTES);MC_Respawn(&w);
  assert(w.x>100&&MC_Get(&w,(int)w.x,(int)(w.y+1.62f),(int)w.z)!=MC_WATER&&crc==MC_Crc32(w.blocks,MC_MAP_BYTES));
  /* Central tree canopy is not preferred over dry ground. */
  flat(&w);for(int y=2;y<7;y++)MC_Set(&w,128,y,128,MC_WOOD);MC_Set(&w,128,7,128,MC_LEAVES);MC_Respawn(&w);
  assert(fabsf(w.y-2.01f)<.001f&&MC_BodyClear(&w,w.x,w.y,w.z));
  /* The 1.5-high fence reaches the cell above its base. Its post is solid
     and supported, but its empty corners do not count as footing. */
  flat(&w);MC_Set(&w,128,2,128,MC_FENCE);
  assert(!MC_BodyClear(&w,128.5f,3.2f,128.5f)&&MC_BodyClear(&w,128.5f,3.51f,128.5f));
  w.x=w.z=128.5f;w.y=3.51f;for(int i=0;i<10;i++)MC_Step(&w,0,0,.05f,0);
  assert(fabsf(w.y-3.5f)<.02f);
  w.x=w.z=128.05f;w.y=3.51f;w.vy=0;assert(MC_BodyClear(&w,w.x,3.2f,w.z));
  for(int i=0;i<5;i++)MC_Step(&w,0,0,.05f,0);
  assert(w.y<3.2f);
  MC_Set(&w,128,5,128,MC_STONE);MC_Respawn(&w);assert(MC_BodyClear(&w,w.x,w.y,w.z));
  /* Maps reduced to bedrock still have a stable spawn at y=1, avoiding
     the former repeated high-altitude fall onto layer zero. */
  flat(&w);for(int z=0;z<MC_MAP_Z;z++)for(int x=0;x<MC_MAP_X;x++)MC_Set(&w,x,1,z,MC_AIR);
  MC_Respawn(&w);assert(fabsf(w.y-1.01f)<.001f&&MC_BodyClear(&w,w.x,w.y,w.z));
  for(int i=0;i<50;i++)MC_Step(&w,0,0,.05f,0);
  assert(w.y>=1&&w.y<1.02f);
  flat(&w);MC_Set(&w,128,MC_MAP_Y-1,128,MC_STONE);MC_Respawn(&w);
  assert(fabsf(w.y-(MC_MAP_Y+.01f))<.001f&&MC_BodyClear(&w,w.x,w.y,w.z));
  free(w.blocks);free(restored.blocks);free(save);
  puts("minicraft spawning: open view / dry support / bed headroom and fallback / canopy / fence / layer zero / saved view: OK");return 0;
}
