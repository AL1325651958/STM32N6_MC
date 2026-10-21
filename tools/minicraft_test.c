#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "minicraft.h"

static void flat(MC_World *w)
{
  memset(w->blocks,0,MC_MAP_BYTES);
  for(int z=0;z<MC_MAP_Z;z++) for(int x=0;x<MC_MAP_X;x++) {
    w->blocks[z*MC_MAP_X+x]=MC_BEDROCK;
    MC_Set(w,x,1,z,MC_STONE);
  }
  w->x=10.5f;w->y=2.01f;w->z=10.5f;w->yaw=0;w->pitch=0;w->vy=0;w->on_ground=0;
}
int main(void)
{
  assert(MC_Crc32("123456789",9)==0xcbf43926u&&MC_Crc32("",0)==0);
  uint8_t *allocation=malloc(MC_MAP_BYTES+64);assert(allocation);
  memset(allocation,0xa5,MC_MAP_BYTES+64);
  MC_World w={.blocks=allocation+32};
  MC_Generate(&w,0x12345678);
  assert(MC_Get(&w,-1,0,0)==MC_BEDROCK && MC_Get(&w,MC_MAP_X,0,0)==MC_BEDROCK);
  assert(MC_Get(&w,64,0,64)==MC_BEDROCK && MC_Get(&w,64,MC_MAP_Y,64)==MC_AIR);
  /* Broad terrain regions remain visible using existing block IDs: a desert
     has several sand layers, while grassland/forest retain grass tops. */
  unsigned desert=0,grass=0,wood=0;
  for(int z=0;z<MC_MAP_Z;z++)for(int x=0;x<MC_MAP_X;x++) {
    int top=MC_MAP_Y-1;while(top>0 && (MC_Get(&w,x,top,z)==MC_AIR||MC_Get(&w,x,top,z)==MC_WATER))top--;
    if(MC_Get(&w,x,top,z)==MC_SAND && MC_Get(&w,x,top-1,z)==MC_SAND && MC_Get(&w,x,top-2,z)==MC_SAND)desert++;
    if(MC_Get(&w,x,top,z)==MC_GRASS)grass++;
    for(int y=1;y<MC_MAP_Y;y++)if(MC_Get(&w,x,y,z)==MC_WOOD){wood++;break;}
  }
  assert(desert>100&&grass>100&&wood>0);
  uint32_t crc=MC_Crc32(w.blocks,MC_MAP_BYTES);
  MC_Generate(&w,0x12345678);assert(crc==MC_Crc32(w.blocks,MC_MAP_BYTES));
  MC_Generate(&w,0x12345679);assert(crc!=MC_Crc32(w.blocks,MC_MAP_BYTES));
  MC_Set(&w,-1,3,0,3);MC_Set(&w,MC_MAP_X,3,0,3);MC_Set(&w,0,64,0,3);MC_Set(&w,0,0,0,MC_AIR);
  assert(MC_Get(&w,0,0,0)==MC_BEDROCK);
  for(int i=0;i<32;i++) assert(allocation[i]==0xa5 && allocation[MC_MAP_BYTES+32+i]==0xa5);

  flat(&w);MC_Set(&w,10,3,13,MC_WOOD);
  int x,y,z,nx,ny,nz;
  assert(MC_Raycast(&w,&x,&y,&z,&nx,&ny,&nz,5));
  assert(x==10 && y==3 && z==13 && nx==0 && ny==0 && nz==-1);
  assert(!MC_Raycast(&w,&x,&y,&z,&nx,&ny,&nz,2));
  assert(MC_Edit(&w,1,MC_GRASS));assert(MC_Get(&w,10,3,12)==MC_GRASS);
  assert(MC_Edit(&w,0,0));assert(MC_Get(&w,10,3,12)==MC_AIR);
  MC_Set(&w,10,3,11,MC_STONE);
  assert(!MC_Edit(&w,1,MC_WOOD)); /* Adjacent candidate intersects the player's head. */
  w.pitch=-1.2f;assert(MC_Raycast(&w,&x,&y,&z,&nx,&ny,&nz,5));assert(ny==1);
  w.pitch=0;w.yaw=1.5707963f;MC_Set(&w,13,3,10,MC_STONE);
  assert(MC_Raycast(&w,&x,&y,&z,&nx,&ny,&nz,5));assert(x==13 && nx==-1);
  w.yaw=-1.5707963f;MC_Set(&w,8,3,10,MC_STONE);
  assert(MC_Raycast(&w,&x,&y,&z,&nx,&ny,&nz,5));assert(x==8 && nx==1);

  /* Small props use their visible dimensions for picking and player collision. */
  flat(&w);MC_Set(&w,10,3,13,MC_TORCH);w.yaw=0;w.pitch=0;
  MC_Hit small;
  w.y=2.40f;assert(!MC_Pick(&w,400,240,&small)); /* Above the corrected 14/16 flame. */
  assert(MC_BodyClear(&w,10.5f,2.01f,13.5f)); /* Fire and stem remain passable. */
  w.y=1.88f;assert(MC_Pick(&w,400,240,&small));
  assert(small.block==MC_TORCH&&small.nz==-1&&fabsf(small.distance-2.875f)<.001f);
  assert(!MC_Raycast(&w,&x,&y,&z,&nx,&ny,&nz,2.8f));
  MC_Set(&w,10,3,14,MC_STONE);w.x=10.3f;
  assert(MC_Pick(&w,400,240,&small)&&small.block==MC_STONE); /* Gap continues to background. */
  w.x=10.5f;w.z=13.5f; /* Inside shape: exit surface and normal. */
  assert(MC_Pick(&w,400,240,&small)&&small.nz==1&&fabsf(small.distance-.125f)<.001f);
  w.z=15;w.yaw=3.14159265f;MC_Set(&w,10,3,14,MC_AIR);
  assert(MC_Pick(&w,400,240,&small)&&small.nz==1&&fabsf(small.distance-1.375f)<.001f);
  w.x=10.5f;w.z=13.5f;w.y=2.38f;w.pitch=-1.5707963f;
  assert(MC_Pick(&w,400,240,&small)&&small.ny==1&&fabsf(small.distance-.125f)<.001f);
  /* Starting within either part must exit the union, never its hidden seam. */
  w.y=3.5f-1.62f;w.pitch=1.5707963f;
  assert(MC_Pick(&w,400,240,&small)&&small.ny==1&&fabsf(small.distance-.375f)<.001f);
  w.y=3.75f-1.62f;w.pitch=-1.5707963f;
  assert(MC_Pick(&w,400,240,&small)&&small.ny==-1&&fabsf(small.distance-.75f)<.001f);
  w.x=10.45f;w.z=10.5f;w.y=3.65f-1.62f;w.pitch=0;w.yaw=0;
  assert(MC_Pick(&w,400,240,&small)&&small.block==MC_TORCH&&fabsf(small.distance-2.875f)<.001f);
  /* The stem occupies the standard 4/16 square, including its near edge. */
  w.x=10.38f;w.y=3.40f-1.62f;assert(MC_Pick(&w,400,240,&small)&&small.block==MC_TORCH);
  w.x=10.39f;w.y=3.8f-1.62f;assert(!MC_Pick(&w,400,240,&small)); /* Stepped flame narrows above its base. */
  w.x=10.5f;w.y=3.85f-1.62f;assert(MC_Pick(&w,400,240,&small)); /* The corrected cap reaches 14/16. */
  flat(&w);MC_Set(&w,10,3,13,MC_SAPLING);w.y=1.88f;
  assert(MC_Pick(&w,400,240,&small)&&small.block==MC_SAPLING);
  w.x=10.2f;w.y=1.48f;assert(!MC_Pick(&w,400,240,&small)); /* Beside thin stem. */
  flat(&w);MC_Set(&w,10,3,13,MC_CHEST);w.x=10.03f;
  assert(!MC_Pick(&w,400,240,&small));w.x=10.5f;
  assert(MC_Pick(&w,400,240,&small)&&fabsf(small.distance-2.5625f)<.001f);
  /* Placement must not overwrite a small object's stored cell. */
  flat(&w);MC_Set(&w,10,3,13,MC_STONE);MC_Set(&w,10,3,12,MC_TORCH);
  w.x=10.2f;assert(!MC_Edit(&w,1,MC_WOOD)&&MC_Get(&w,10,3,12)==MC_TORCH);
  flat(&w);MC_Set(&w,10,2,11,MC_TORCH);
  MC_Set(&w,10,2,12,MC_SAPLING);
  for(int i=0;i<30;i++)MC_Step(&w,1,0,.1f,0);
  assert(w.z>13); /* Floor torch and sapling do not behave like solid cubes. */
  flat(&w);MC_Set(&w,10,2,11,MC_BED);
  for(int i=0;i<30;i++)MC_Step(&w,1,0,.1f,0);
  assert(w.z<10.73f); /* Solid lower bed blocks walking into it. */
  w.x=10.5f;w.z=11.5f;w.y=4;w.vy=0;
  for(int i=0;i<80;i++)MC_Step(&w,0,0,.016f,0);
  assert(fabsf(w.y-2.5625f)<.01f); /* Land on actual mattress height. */
  w.sneak=1;
  for(int i=0;i<40;i++)MC_Step(&w,1,0,.067f,0);
  assert(w.z<12.25f&&w.y>2.55f);w.sneak=0;
  /* Doors occupy two cells, hinge at the cell edge and keep actual panel
     collision when open. Upper-half use and removal affect the complete door. */
  flat(&w);MC_Set(&w,10,2,14,MC_STONE);
  w.x=10.5f;w.y=2.01f;w.z=10.5f;w.yaw=0;w.pitch=-.3f;w.on_ground=1;
  assert(MC_Edit(&w,1,MC_DOOR)&&MC_Get(&w,10,2,13)==MC_DOOR&&MC_Get(&w,10,3,13)==MC_DOOR_TOP);
  assert(MC_IsSolid(MC_DOOR)&&MC_IsSolid(MC_DOOR_OPEN));
  w.pitch=0;assert(MC_Pick(&w,400,240,&small)&&small.block==MC_DOOR_TOP);
  assert(MC_OpenTarget(&w)&&MC_Get(&w,10,2,13)==MC_DOOR_OPEN&&MC_Get(&w,10,3,13)==MC_DOOR_TOP_OPEN);
  assert(MC_BodyClear(&w,10.5f,2.01f,13.5f)&&!MC_BodyClear(&w,10.1f,2.01f,13.5f));
  assert(!MC_Raycast(&w,&x,&y,&z,&nx,&ny,&nz,3));
  w.x=10.1f;assert(MC_OpenTarget(&w)&&MC_Get(&w,10,3,13)==MC_DOOR_TOP);
  w.x=10.5f;for(int i=0;i<30;i++)MC_Step(&w,1,0,.1f,0);
  assert(w.z>12.67f&&w.z<12.74f);
  MC_Set(&w,10,2,13,MC_DOOR_OPEN);MC_Set(&w,10,3,13,MC_DOOR_TOP_OPEN);
  w.x=10.5f;w.z=13.1f;w.yaw=-1.5707963f;
  assert(MC_OpenTarget(&w)&&MC_Get(&w,10,2,13)==MC_DOOR_OPEN&&MC_Get(&w,10,3,13)==MC_DOOR_TOP_OPEN);
  MC_RemoveBlock(&w,10,3,13);assert(MC_Get(&w,10,2,13)==MC_AIR&&MC_Get(&w,10,3,13)==MC_AIR);
  flat(&w);MC_Set(&w,10,2,14,MC_STONE);MC_Set(&w,10,3,13,MC_STONE);w.yaw=0;w.pitch=-.3f;
  assert(!MC_Edit(&w,1,MC_DOOR)&&MC_Get(&w,10,2,13)==MC_AIR);
  assert(!MC_Edit(&w,1,MC_DOOR_TOP)&&!MC_Edit(&w,1,MC_DOOR_OPEN));
  /* Half slabs and stair treads are walked up without a jump. Full cubes
     and low ceilings remain barriers. */
  flat(&w);w.on_ground=1;MC_Set(&w,10,2,12,MC_SLAB);
  float step_peak=2;for(int i=0;i<20;i++){MC_Step(&w,1,0,.05f,0);if(w.y>step_peak)step_peak=w.y;}
  assert(w.z>12.5f&&step_peak>2.45f&&step_peak<2.55f);
  flat(&w);w.on_ground=1;MC_Set(&w,10,2,12,MC_STAIR_SOUTH);
  step_peak=2;for(int i=0;i<20;i++){MC_Step(&w,1,0,.05f,0);if(w.y>step_peak)step_peak=w.y;}
  assert(w.z>12.5f&&step_peak>2.95f&&step_peak<3.05f);
  flat(&w);w.x=10.25f;w.pitch=0;MC_Set(&w,10,3,13,MC_STAIR);MC_Set(&w,10,3,15,MC_STONE);
  assert(MC_Pick(&w,400,240,&small)&&small.z==15);
  MC_Set(&w,10,3,13,MC_SLAB);assert(MC_Pick(&w,400,240,&small)&&small.z==15);
  w.x=10.1f;w.y=2.03f;MC_Set(&w,10,3,13,MC_FENCE);assert(MC_Pick(&w,400,240,&small)&&small.z==15);
  flat(&w);w.on_ground=1;MC_Set(&w,10,2,12,MC_SLAB);for(int z0=10;z0<14;z0++)MC_Set(&w,10,4,z0,MC_STONE);
  for(int i=0;i<20;i++){MC_Step(&w,1,0,.05f,0);}assert(w.z<11.74f&&w.y<2.02f);
  flat(&w);MC_Set(&w,10,2,10,MC_CHEST);w.y=4;
  for(int i=0;i<80;i++)MC_Step(&w,0,0,.016f,0);
  assert(fabsf(w.y-2.875f)<.01f);
  flat(&w);MC_Set(&w,64,2,64,MC_BED);w.spawn[0]=65;w.spawn[1]=3;w.spawn[2]=65;
  MC_Respawn(&w);assert(fabsf(w.y-2.01f)<.001f&&fabsf(w.x-64.5f)<=1&&fabsf(w.z-64.5f)<=1);
  memset(w.spawn,0,sizeof(w.spawn));

  flat(&w);
  for(int i=0;i<100;i++) MC_Step(&w,0,0,.016f,0);
  assert(w.on_ground && fabsf(w.y-2)<.01f);
  MC_Step(&w,0,0,.016f,1);float peak=w.y;
  for(int i=0;i<150;i++) {MC_Step(&w,0,0,.016f,0);if(w.y>peak)peak=w.y;}
  assert(peak>3.2f && peak<3.6f && w.on_ground);
  /* Head collision and walls at all body heights / both sides of the AABB. */
  flat(&w);MC_Set(&w,10,4,10,MC_STONE);
  MC_Step(&w,0,0,.1f,0);MC_Step(&w,0,0,.016f,1);
  for(int i=0;i<60;i++) MC_Step(&w,0,0,.016f,0);
  assert(w.y<2.21f && w.on_ground);
  flat(&w);
  for(int yy=2;yy<=4;yy++) for(int xx=9;xx<=12;xx++) MC_Set(&w,xx,yy,12,MC_STONE);
  for(int i=0;i<100;i++) MC_Step(&w,1,0,.1f,0);
  assert(w.z<11.73f && w.z>11.6f);
  /* Sneak keeps a supporting tile beneath the feet, including fractional frames. */
  flat(&w);memset(w.blocks,0,MC_MAP_BYTES);
  for(int z=1;z<=11;z++)for(int x=9;x<=12;x++)MC_Set(&w,x,1,z,MC_STONE);
  w.x=10.5f;w.z=10.8f;w.y=2.01f;w.yaw=0;w.on_ground=1;w.sneak=1;
  for(int i=0;i<80;i++)MC_Step(&w,1,0,.1f,0);
  assert(w.z<12.24f&&w.z>12.1f&&w.y>1.99f);
  w.sneak=0;MC_Step(&w,1,0,.1f,0);
  assert(w.z>12.3f&&w.y<2);
  /* A diagonal cannot bypass the corner; jump deliberately releases the guard. */
  w.x=12.1f;w.z=12.1f;w.y=2;w.vy=0;w.on_ground=1;w.sneak=1;
  for(int i=0;i<40;i++)MC_Step(&w,1,1,.067f,0);
  assert(w.x<13.24f&&w.z<12.24f&&w.y>1.99f);
  w.on_ground=1;MC_Step(&w,1,0,.1f,1);assert(w.y>2.3f&&w.z>12.24f);
  flat(&w);w.sneak=1;MC_Step(&w,1,0,.1f,0);assert(w.z>10.63f&&w.z<10.66f);w.sneak=0;
  /* Stone slabs occupy one half of a cell, support the player at their
     actual surface, and keep the top/bottom state in the map byte. */
  flat(&w);MC_NewPlayer(&w,0);w.x=10.5f;w.y=2.01f;w.z=10.5f;w.yaw=0;w.pitch=0;w.on_ground=1;
  MC_Set(&w,10,3,13,MC_SLAB);MC_Hit slab;
  assert(MC_Pick(&w,400,240,&slab)==0); /* The horizontal eye ray is above a lower slab. */
  w.pitch=-.2f;assert(MC_Pick(&w,400,240,&slab)&&slab.block==MC_SLAB);
  w.pitch=0;MC_Set(&w,10,3,13,MC_SLAB_TOP);
  assert(MC_Pick(&w,400,240,&slab)&&slab.block==MC_SLAB_TOP);
  w.pitch=-.2f;w.x=10.5f;w.z=13.5f;w.y=4.2f;w.on_ground=0;
  for(int i=0;i<100;i++)MC_Step(&w,0,0,.016f,0);
  assert(fabsf(w.y-4.0f)<.02f&&w.on_ground); /* Upper slab surface is y+1.0. */
  flat(&w);MC_NewPlayer(&w,0);w.x=10.5f;w.y=2.01f;w.z=10.5f;w.yaw=0;w.pitch=-.2f;w.on_ground=1;
  MC_Set(&w,10,3,13,MC_STONE);assert(MC_Give(&w,MC_SLAB,1));w.slot=0;MC_SyncInventory(&w);
  assert(MC_Place(&w)&&MC_Get(&w,10,3,12)==MC_SLAB);
  MC_Set(&w,10,3,12,MC_SLAB_TOP);w.pitch=0;assert(MC_Pick(&w,400,240,&slab)&&slab.block==MC_SLAB_TOP);
  assert(MC_Give(&w,MC_STONE_PICK,1));w.slot=0;MC_SyncInventory(&w);
  w.target_valid=1;w.target=slab;w.mining=0;
  for(int i=0;i<20;i++)MC_PlayTick(&w,.1f,1,400,240);
  assert(MC_Get(&w,10,3,12)==MC_AIR&&w.items[MC_SLAB]==1);
  flat(&w);w.x=.3f;w.yaw=-1.5707963f;
  for(int i=0;i<10;i++) MC_Step(&w,1,0,.1f,0);
  assert(w.x>=.28f);
  assert(MC_Crc32("123456789",9)==0xcbf43926);
  /* Cached lights must match a forced rebuild after edits and scene changes. */
  flat(&w);MC_NewPlayer(&w,0);w.time=435;w.pitch=0;w.yaw=0;MC_Set(&w,10,3,13,MC_STONE);
  uint16_t *frame=malloc(MC_VIEW_W*MC_VIEW_H*2);assert(frame);
  MC_Render(&w,frame,MC_VIEW_W,MC_VIEW_H);uint32_t dark=MC_Crc32(frame,MC_VIEW_W*MC_VIEW_H*2);
  MC_Set(&w,11,3,12,MC_TORCH);MC_Render(&w,frame,MC_VIEW_W,MC_VIEW_H);uint32_t lit=MC_Crc32(frame,MC_VIEW_W*MC_VIEW_H*2);assert(dark!=lit);
  w.scene_serial++;MC_Render(&w,frame,MC_VIEW_W,MC_VIEW_H);assert(lit==MC_Crc32(frame,MC_VIEW_W*MC_VIEW_H*2));
  MC_Set(&w,11,3,12,MC_AIR);MC_Render(&w,frame,MC_VIEW_W,MC_VIEW_H);assert(dark==MC_Crc32(frame,MC_VIEW_W*MC_VIEW_H*2));
  flat(&w);MC_NewPlayer(&w,0);MC_Set(&w,10,2,13,MC_DOOR);MC_Set(&w,10,3,13,MC_DOOR_TOP);
  w.target_valid=1;w.target=(MC_Hit){.x=10,.y=2,.z=13,.block=MC_DOOR};MC_Render(&w,frame,MC_VIEW_W,MC_VIEW_H);
  uint32_t door_outline=MC_Crc32(frame,MC_VIEW_W*MC_VIEW_H*2);
  w.target.y=3;w.target.block=MC_DOOR_TOP;MC_Render(&w,frame,MC_VIEW_W,MC_VIEW_H);assert(door_outline==MC_Crc32(frame,MC_VIEW_W*MC_VIEW_H*2));
  w.target_valid=0;MC_Render(&w,frame,MC_VIEW_W,MC_VIEW_H);assert(door_outline!=MC_Crc32(frame,MC_VIEW_W*MC_VIEW_H*2));free(frame);
  free(allocation);puts("minicraft core tests: OK");return 0;
}
