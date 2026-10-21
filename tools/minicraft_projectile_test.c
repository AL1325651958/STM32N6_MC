#include "minicraft.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void reset(MC_World *w)
{
  memset(w->blocks,0,MC_MAP_BYTES);MC_NewPlayer(w,0);
  for(int z=6;z<16;z++)for(int x=6;x<16;x++)MC_Set(w,x,1,z,MC_STONE);
  w->x=10.5f;w->y=2.01f;w->z=8.5f;w->yaw=w->pitch=0;
}
static void door(MC_World *w,int open)
{
  MC_Set(w,10,2,10,open?MC_DOOR_OPEN:MC_DOOR);
  MC_Set(w,10,3,10,open?MC_DOOR_TOP_OPEN:MC_DOOR_TOP);
}
static void fire(MC_World *w,float x,float y)
{
  /* Cross three cells in a single substep, compensating its gravity. */
  w->projectiles[0]=(MC_Projectile){x,y,9,0,.16f,150,0,1};
  MC_ProjectilesTick(w,.02f);
}
int main(void)
{
  MC_World w={.blocks=calloc(1,MC_MAP_BYTES)};assert(w.blocks);reset(&w);
  float t=-1;
  door(&w,0);
  assert(MC_BlockSegment(&w,10.5f,2.7f,9,10.5f,2.7f,12,&t)&&fabsf(t-1/3.f)<1e-5f);
  assert(MC_BlockSegment(&w,10.5f,3.7f,9,10.5f,3.7f,12,&t));
  assert(MC_BlockSegment(&w,10.5f,2.7f,12,10.5f,2.7f,9,&t)&&fabsf(t-(2-3/16.f)/3)<1e-5f);
  assert(MC_BlockSegment(&w,10.5f,3.7f,10.1f,10.5f,3.7f,10.11f,&t)&&t==0);
  door(&w,1);
  assert(!MC_BlockSegment(&w,10.5f,2.7f,9,10.5f,2.7f,12,&t));
  assert(!MC_BlockSegment(&w,10.5f,3.7f,9,10.5f,3.7f,12,0));
  assert(MC_BlockSegment(&w,10.1f,3.7f,9,10.1f,3.7f,12,&t));
  assert(MC_BlockSegment(&w,9,3.7f,10.5f,12,3.7f,10.5f,&t));
  assert(!MC_BlockSegment(&w,10.5f,2.7f,10.5f,10.5f,2.7f,10.5f,0));
  assert(MC_BlockSegment(&w,10.1f,2.7f,10.5f,10.1f,2.7f,10.5f,&t)&&t==0);
  MC_Set(&w,10,3,10,MC_AIR);
  MC_Set(&w,10,2,10,MC_SLAB);
  assert(!MC_BlockSegment(&w,10.5f,2.7f,9,10.5f,2.7f,12,0));
  assert(MC_BlockSegment(&w,10.5f,2.2f,9,10.5f,2.2f,12,0));
  MC_Set(&w,10,2,10,MC_SLAB_TOP);
  assert(MC_BlockSegment(&w,10.5f,2.7f,9,10.5f,2.7f,12,0));
  assert(!MC_BlockSegment(&w,10.5f,2.2f,9,10.5f,2.2f,12,0));
  MC_Set(&w,10,2,10,MC_STAIR);
  assert(!MC_BlockSegment(&w,10.2f,2.7f,9,10.2f,2.7f,12,0));
  assert(MC_BlockSegment(&w,10.8f,2.7f,9,10.8f,2.7f,12,0));
  MC_Set(&w,10,2,10,MC_FENCE);
  assert(!MC_BlockSegment(&w,10.1f,2.2f,9,10.1f,2.2f,12,0));
  assert(MC_BlockSegment(&w,10.5f,3.3f,9,10.5f,3.3f,12,&t));
  assert(!MC_BlockSegment(&w,10.5f,3.6f,9,10.5f,3.6f,12,0));
  MC_Set(&w,10,2,10,MC_CHEST);
  assert(!MC_BlockSegment(&w,10.02f,2.7f,9,10.02f,2.7f,12,0));
  assert(MC_BlockSegment(&w,10.5f,2.7f,9,10.5f,2.7f,12,0));
  assert(!MC_BlockSegment(&w,10.5f,2.9f,9,10.5f,2.9f,12,0));
  MC_Set(&w,10,2,10,MC_TORCH);
  assert(!MC_BlockSegment(&w,10.5f,2.4f,9,10.5f,2.4f,12,0));
  assert(MC_BlockSegment(&w,.5f,3,10,-.5f,3,10,&t)&&fabsf(t-.5f)<1e-5f);
  assert(MC_BlockSegment(&w,-.1f,3,10,0,3,10,&t)&&t==0);
  assert(!MC_BlockSegment(&w,NAN,3,10,0,3,10,0));

  /* The flight tick, not just the query API, respects both door halves. */
  for(int upper=0;upper<2;upper++)for(int open=0;open<2;open++) {
    reset(&w);door(&w,open);fire(&w,10.5f,2.7f+upper);
    assert(w.projectiles[0].active==open);
    if(open)assert(fabsf(w.projectiles[0].z-12)<1e-5f);
  }
  reset(&w);door(&w,1);fire(&w,10.1f,3.7f);assert(!w.projectiles[0].active);
  for(int upper=0;upper<2;upper++)for(int high=0;high<2;high++) {
    reset(&w);MC_Set(&w,10,2,10,upper?MC_SLAB_TOP:MC_SLAB);fire(&w,10.5f,high?2.7f:2.2f);
    assert(w.projectiles[0].active==(upper!=high));
  }
  /* Hit the closest visible part independently of mob slot order. A head
     and an animal's side head are outside the former spherical target. */
  reset(&w);w.mobs[0]=(MC_Mob){10.5f,2,11.5f,0,0,0,12,1,0,0};
  w.mobs[1]=(MC_Mob){10.5f,2,10.5f,0,0,0,12,1,0,0};
  fire(&w,10.5f,3.6f);assert(w.mobs[0].health==12&&w.mobs[1].health==7&&!w.projectiles[0].active);
  reset(&w);w.mobs[0]=(MC_Mob){10.5f,2,10.5f,0,0,0,12,1,0,0};
  fire(&w,10.5f,2.4f);assert(w.mobs[0].health==12&&w.projectiles[0].active);
  fire(&w,10.6f,2.4f);assert(w.mobs[0].health==7&&!w.projectiles[0].active);
  reset(&w);w.mobs[0]=(MC_Mob){10.5f,2,10.5f,0,0,0,8,1,1,0};
  fire(&w,10.9f,3.15f);assert(w.mobs[0].health==3);
  reset(&w);door(&w,0);w.mobs[0]=(MC_Mob){10.5f,2,11.5f,0,0,0,12,1,0,0};
  fire(&w,10.5f,3.6f);assert(w.mobs[0].health==12&&!w.projectiles[0].active);
  door(&w,1);fire(&w,10.5f,3.6f);assert(w.mobs[0].health==7&&!w.projectiles[0].active);
  reset(&w);door(&w,0);w.mobs[0]=(MC_Mob){10.5f,2,9.6f,0,0,0,12,1,0,0};
  fire(&w,10.5f,3.6f);assert(w.mobs[0].health==7);
  /* Starting inside a visible part, even for a zero-length segment, hits. */
  w.projectiles[0]=(MC_Projectile){10.5f,3.6f,9.6f,0,.16f,0,0,1};
  MC_ProjectilesTick(&w,.02f);assert(w.mobs[0].health==2&&!w.projectiles[0].active);
  reset(&w);door(&w,0);w.z=9.7f;
  assert(MC_Give(&w,MC_BOW,1)&&MC_Give(&w,MC_ARROW,2));w.slot=0;MC_SyncInventory(&w);
  assert(MC_Shoot(&w,400,240)&&w.items[MC_ARROW]==1);
  for(int i=0;i<5;i++)MC_ProjectilesTick(&w,.02f);
  assert(!w.projectiles[0].active&&!strcmp(w.notice,"ARROW HIT BLOCK"));
  reset(&w);w.projectiles[0]=(MC_Projectile){10.5f,3.6f,9,0,0,18,0,1};w.bag=1;
  MC_Projectile paused=w.projectiles[0];MC_ProjectilesTick(&w,.1f);
  assert(!memcmp(&paused,&w.projectiles[0],sizeof(paused)));
  free(w.blocks);puts("minicraft arrows: part geometry / swept segments / nearest hit / heads / point-blank launch / pause: OK");return 0;
}
