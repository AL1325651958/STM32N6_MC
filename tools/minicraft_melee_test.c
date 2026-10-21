#include "minicraft.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void rotate(unsigned facing,float x,float z,float *px,float *pz)
{
  switch(facing) {
    case 1:*px=10+z;*pz=11-x;break;
    case 2:*px=11-x;*pz=11-z;break;
    case 3:*px=11-z;*pz=10+x;break;
    default:*px=10+x;*pz=10+z;break;
  }
}

static void arena(MC_World *w,unsigned facing,int across,int reverse)
{
  memset(w->blocks,0,MC_MAP_BYTES);
  memset(w->blocks,MC_BEDROCK,MC_MAP_X*MC_MAP_Z);
  for(int z=7;z<15;z++)for(int x=7;x<15;x++)MC_Set(w,x,1,z,MC_STONE);
  MC_NewPlayer(w,0);w->time=435;
  float a=reverse?1.05f:-.35f,b=reverse?-.35f:1.05f;
  rotate(facing,across?.5f:b,across?b:.5f,&w->x,&w->z);w->y=2.01f;
  MC_Mob *m=&w->mobs[0];
  *m=(MC_Mob){0,2.01f,0,0,0,0,MC_MOB_HEALTH,1,0,0};
  rotate(facing,across?.5f:a,across?a:.5f,&m->x,&m->z);
}

static void attack(MC_World *w,int expected,unsigned facing,int reverse,int across)
{
  MC_Mob *m=&w->mobs[0];
  if(!MC_BodyClear(w,m->x,m->y,m->z)||!MC_BodyClear(w,w->x,w->y,w->z)) {
    fprintf(stderr,"blocked fixture p=(%.2f,%.2f) m=(%.2f,%.2f)\n",w->x,w->z,m->x,m->z);
    abort();
  }
  unsigned health=w->health;
  MC_MobsTick(w,.1f);
  if(!m->health||w->health!=health-(expected?2u:0u)) {
    fprintf(stderr,"mismatch f=%u reverse=%d across=%d expected=%d player=(%.2f,%.2f) mob=(%.2f,%.2f) health=%u/%u attack=%.2f\n",
            facing,reverse,across,expected,w->x,w->z,m->x,m->z,w->health,health,m->attack);
    abort();
  }
  assert(expected?m->attack>1:m->attack==0);
  if(expected) {
    /* Existing attack and player damage cooldowns still prevent repeats. */
    MC_MobsTick(w,.1f);assert(w->health==health-2);
  }
}

int main(void)
{
  MC_World w={.blocks=calloc(1,MC_MAP_BYTES)};assert(w.blocks);
  for(unsigned facing=0;facing<4;facing++)for(int reverse=0;reverse<2;reverse++)
    for(int across=0;across<2;across++)for(int open=0;open<2;open++) {
      arena(&w,facing,across,reverse);
      MC_Set(&w,10,2,10,MC_DoorState(facing,open,0));
      MC_Set(&w,10,3,10,MC_DoorState(facing,open,1));
      attack(&w,across?open:!open,facing,reverse,across);
    }
  /* Part collision keeps upper and lower slab halves distinct for a chest
     height melee trace, just as it does for arrows and picking. */
  for(int reverse=0;reverse<2;reverse++)for(unsigned b=0;b<3;b++) {
    arena(&w,0,0,reverse);
    MC_Set(&w,10,3,10,b==0?MC_STONE:b==1?MC_SLAB:MC_SLAB_TOP);
    float y=b==0?3.25f:b==1?3.25f:3.75f;
    float z0=reverse?12:9,z1=reverse?9:12;
    assert(MC_BlockSegment(&w,10.5f,y,z0,10.5f,y,z1,0));
    if(b) {
      y=b==1?3.75f:3.25f;
      assert(!MC_BlockSegment(&w,10.5f,y,z0,10.5f,y,z1,0));
    }
  }
  arena(&w,0,0,0);attack(&w,1,0,0,0);
  arena(&w,0,0,0);w.z+=2;attack(&w,0,0,0,0);
  arena(&w,0,0,0);w.bag=1;attack(&w,0,0,0,0);
  arena(&w,0,0,0);w.mobs[0].attack=1;unsigned health=w.health;
  MC_MobsTick(&w,.1f);assert(w.health==health);
  free(w.blocks);
  puts("minicraft melee: four-facing open passages / closed panels / hinge sides / reverse rays / slab halves / range, bag and cooldown: OK");
  return 0;
}
