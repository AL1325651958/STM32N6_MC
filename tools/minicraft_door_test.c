#include "minicraft.h"
#include "mc_save.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Independent rotation of canonical test points about the door cell. */
static void point(unsigned facing,float x,float z,float *out_x,float *out_z)
{
  switch(facing) {
    case 1:*out_x=10+z;*out_z=11-x;break;
    case 2:*out_x=11-x;*out_z=11-z;break;
    case 3:*out_x=11-z;*out_z=10+x;break;
    default:*out_x=10+x;*out_z=10+z;break;
  }
}
static float yaw(unsigned facing){return facing==3?-1.570796327f:facing*1.570796327f;}
static void camera(MC_World *w,unsigned facing,float x,float z,float turn,float pitch)
{point(facing,x,z,&w->x,&w->z);w->y=2.01f;w->yaw=yaw(facing)+turn;w->pitch=pitch;w->on_ground=1;}
static void reset(MC_World *w)
{
  memset(w->blocks,0,MC_MAP_BYTES);memset(w->blocks,MC_BEDROCK,MC_MAP_X*MC_MAP_Z);
  for(int z=4;z<18;z++)for(int x=4;x<18;x++)MC_Set(w,x,1,z,MC_STONE);
  MC_NewPlayer(w,0);w->time=90;
}
static void set_door(MC_World *w,unsigned facing,int open)
{MC_Set(w,10,2,10,MC_DoorState(facing,open,0));MC_Set(w,10,3,10,MC_DoorState(facing,open,1));}
static int segment(MC_World *w,unsigned facing,float x,float z,float ex,float ez,float y)
{float ax,az,bx,bz;point(facing,x,z,&ax,&az);point(facing,ex,ez,&bx,&bz);return MC_BlockSegment(w,ax,y,az,bx,y,bz,0);}
static int clear(MC_World *w,unsigned facing,float x,float z)
{float px,pz;point(facing,x,z,&px,&pz);return MC_BodyClear(w,px,2.01f,pz);}
static void put32(uint8_t *p,uint32_t n){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(n>>(i*8));}
static void version(uint8_t *p,unsigned v){put32(p+4,v);put32(p+60,MC_Crc32(p,60));}
static void roundtrip(MC_World *w,MC_World *loaded,uint8_t *save)
{MC_SaveEncode(w,12,save);assert(save[4]==7&&MC_SaveValidate(save,MC_SAVE_SIZE)==12);assert(MC_SaveDecode(loaded,save,MC_SAVE_SIZE));}
int main(void)
{
  MC_World w={.blocks=calloc(1,MC_MAP_BYTES)},loaded={.blocks=calloc(1,MC_MAP_BYTES)};
  uint8_t *save=malloc(MC_SAVE_SIZE);uint16_t *frame=malloc(MC_VIEW_W*MC_VIEW_H*2);
  assert(w.blocks&&loaded.blocks&&save&&frame);
  unsigned states=0;
  for(unsigned id=0;id<256;id++)if(MC_IsDoor((uint8_t)id)) {
    states++;assert(MC_IsBlock(id)&&MC_IsSolid((uint8_t)id));
    assert(MC_DoorState(MC_DoorFacing((uint8_t)id),MC_DoorOpen((uint8_t)id),MC_DoorUpper((uint8_t)id))==id);
    assert(MC_DoorBase((uint8_t)id)==MC_DoorState(MC_DoorFacing((uint8_t)id),MC_DoorOpen((uint8_t)id),0));
    assert(strcmp(MC_ItemName(id),"EMPTY"));
  }
  assert(states==16);
  for(unsigned facing=0;facing<4;facing++) {
    /* The normal item places a paired door in the player's facing and consumes
       one item. All four physical orientations use the real picking path. */
    reset(&w);camera(&w,facing,.5f,-2,0,-.3f);
    float bx,bz;point(facing,.5f,1.5f,&bx,&bz);MC_Set(&w,(int)floorf(bx),2,(int)floorf(bz),MC_STONE);
    assert(MC_Give(&w,MC_DOOR,2)&&MC_Place(&w)&&w.items[MC_DOOR]==1);
    assert(MC_Get(&w,10,2,10)==MC_DoorState(facing,0,0)&&MC_Get(&w,10,3,10)==MC_DoorState(facing,0,1));
    MC_RemoveBlock(&w,10,3,10);MC_Set(&w,10,3,10,MC_STONE);
    assert(!MC_Place(&w)&&w.items[MC_DOOR]==1&&MC_Get(&w,10,2,10)==MC_AIR);
    MC_Set(&w,10,3,10,MC_AIR);MC_Set(&w,10,1,10,MC_AIR);
    assert(!MC_Place(&w)&&w.items[MC_DOOR]==1&&MC_Get(&w,10,3,10)==MC_AIR);
    for(unsigned half=0;half<2;half++) {
      reset(&w);set_door(&w,facing,0);camera(&w,facing,.5f,-2,0,0);
      float y=half?3.5f:2.5f;
      assert(segment(&w,facing,.5f,-1,.5f,2,y)&&segment(&w,facing,.5f,2,.5f,-1,y));
      assert(!clear(&w,facing,.5f,.1f));
      set_door(&w,facing,1);
      assert(!segment(&w,facing,.5f,-1,.5f,2,y));
      assert(segment(&w,facing,.1f,-1,.1f,2,y)&&segment(&w,facing,-1,.5f,2,.5f,y));
      assert(clear(&w,facing,.5f,.5f)&&!clear(&w,facing,.1f,.5f));
      /* Real projectiles agree with the thin rotated panels. */
      float ax,az,ex,ez;point(facing,.5f,-1,&ax,&az);point(facing,.5f,2,&ex,&ez);
      w.projectiles[0]=(MC_Projectile){ax,y,az,(ex-ax)/.02f,.16f,(ez-az)/.02f,0,1};
      MC_ProjectilesTick(&w,.02f);assert(w.projectiles[0].active);
      set_door(&w,facing,0);
      w.projectiles[0]=(MC_Projectile){ax,y,az,(ex-ax)/.02f,.16f,(ez-az)/.02f,0,1};
      MC_ProjectilesTick(&w,.02f);assert(!w.projectiles[0].active);
    }
    for(unsigned half=0;half<2;half++) {
      reset(&w);set_door(&w,facing,0);camera(&w,facing,.5f,-2,0,half?0:-.45f);
      MC_Hit hit;assert(MC_Pick(&w,400,240,&hit)&&hit.block==MC_DoorState(facing,0,(int)half));
      assert(MC_OpenTarget(&w)&&MC_Get(&w,10,2,10)==MC_DoorState(facing,1,0));
      assert(MC_Get(&w,10,3,10)==MC_DoorState(facing,1,1));
      camera(&w,facing,.1f,-2,0,half?0:-.45f);assert(MC_OpenTarget(&w));
      assert(MC_Get(&w,10,2,10)==MC_DoorState(facing,0,0)&&MC_Get(&w,10,3,10)==MC_DoorState(facing,0,1));
    }
    for(unsigned open=0;open<2;open++) {
      reset(&w);set_door(&w,facing,(int)open);camera(&w,facing,.5f,-1.5f,0,0);
      for(unsigned tick=0;tick<25;tick++)MC_Step(&w,1,0,.05f,0);
      float distance=(w.x-10.5f)*sinf(yaw(facing))+(w.z-10.5f)*cosf(yaw(facing));
      assert(open?distance>.8f:distance<-.7f);
    }
    reset(&w);set_door(&w,facing,0);
    /* The open panel may not swing into the player; rollback both halves. */
    camera(&w,facing,.1f,.6f,3.141592654f,0);assert(clear(&w,facing,.1f,.6f)&&MC_OpenTarget(&w));
    assert(MC_Get(&w,10,2,10)==MC_DoorState(facing,0,0));
    assert(MC_Get(&w,10,3,10)==MC_DoorState(facing,0,1));
    set_door(&w,facing,1);camera(&w,facing,.5f,.1f,-1.570796327f,0);
    assert(clear(&w,facing,.5f,.1f)&&MC_OpenTarget(&w));
    assert(MC_Get(&w,10,2,10)==MC_DoorState(facing,1,0)&&MC_Get(&w,10,3,10)==MC_DoorState(facing,1,1));
    for(unsigned half=0;half<2;half++) {
      reset(&w);set_door(&w,facing,0);camera(&w,facing,.5f,-2,0,half?0:-.45f);
      MC_Hit mined;assert(MC_Pick(&w,400,240,&mined)&&MC_DoorUpper(mined.block)==(int)half);
      assert(MC_Give(&w,MC_AXE,1));
      for(unsigned tick=0;tick<20&&MC_Get(&w,10,2,10)!=MC_AIR;tick++)MC_PlayTick(&w,.1f,1,400,240);
      assert(MC_Get(&w,10,2,10)==MC_AIR&&MC_Get(&w,10,3,10)==MC_AIR&&w.items[MC_DOOR]==1);
    }
    for(unsigned open=0;open<2;open++) {
      reset(&w);set_door(&w,facing,(int)open);camera(&w,facing,.5f,-2,0,0);
      roundtrip(&w,&loaded,save);
      assert(MC_Get(&loaded,10,2,10)==MC_DoorState(facing,(int)open,0));
      assert(MC_Get(&loaded,10,3,10)==MC_DoorState(facing,(int)open,1));
      if(facing){version(save,6);assert(!MC_SaveValidate(save,MC_SAVE_SIZE));}
      else {version(save,6);assert(MC_SaveDecode(&loaded,save,MC_SAVE_SIZE));}
      MC_Set(&w,10,2,10,MC_DoorState((facing+1)%4,(int)open,0));
      MC_SaveEncode(&w,12,save);assert(!MC_SaveValidate(save,MC_SAVE_SIZE));
      set_door(&w,facing,(int)open);MC_Set(&w,10,2,10,MC_AIR);
      MC_SaveEncode(&w,12,save);assert(!MC_SaveValidate(save,MC_SAVE_SIZE));
      if(facing){set_door(&w,facing,(int)open);MC_Set(&w,10,3,10,MC_AIR);MC_SaveEncode(&w,12,save);assert(!MC_SaveValidate(save,MC_SAVE_SIZE));}
    }
    /* Both half selections highlight the whole door, including rotations. */
    reset(&w);set_door(&w,facing,0);camera(&w,facing,.5f,-2,0,-.3f);
    w.target_valid=1;w.target=(MC_Hit){.x=10,.y=2,.z=10,.block=MC_DoorState(facing,0,0)};
    MC_Render(&w,frame,MC_VIEW_W,MC_VIEW_H);uint32_t outlined=MC_Crc32(frame,MC_VIEW_W*MC_VIEW_H*2);
    w.target.y=3;w.target.block=MC_DoorState(facing,0,1);MC_Render(&w,frame,MC_VIEW_W,MC_VIEW_H);
    assert(outlined==MC_Crc32(frame,MC_VIEW_W*MC_VIEW_H*2));
    printf("door facing %u: placement, panels, arrows, hinge rollback, both-half mining, saved state and outline OK\n",facing);
  }
  reset(&w);camera(&w,0,.5f,-2,0,-.3f);MC_Set(&w,10,2,11,MC_STONE);
  for(unsigned id=MC_DOOR_ROTATED_FIRST;id<=MC_DOOR_ROTATED_LAST;id++) {
    assert(!MC_Give(&w,id,1)&&!MC_Edit(&w,1,(uint8_t)id));
    w.slots[0]=(MC_Stack){(uint8_t)id,1,0};MC_SaveEncode(&w,12,save);assert(!MC_SaveValidate(save,MC_SAVE_SIZE));
  }
  free(w.blocks);free(loaded.blocks);free(save);free(frame);
  puts("minicraft directional doors: 16 states / 4 facings / paired geometry and gameplay / v6 migration and v7 rejection invariants: OK");
  return 0;
}
