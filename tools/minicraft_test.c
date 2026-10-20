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
  uint8_t *allocation=malloc(MC_MAP_BYTES+64);assert(allocation);
  memset(allocation,0xa5,MC_MAP_BYTES+64);
  MC_World w={.blocks=allocation+32};
  MC_Generate(&w,0x12345678);
  assert(MC_Get(&w,-1,0,0)==MC_BEDROCK && MC_Get(&w,MC_MAP_X,0,0)==MC_BEDROCK);
  assert(MC_Get(&w,64,0,64)==MC_BEDROCK && MC_Get(&w,64,MC_MAP_Y,64)==MC_AIR);
  uint32_t crc=MC_Crc32(w.blocks,MC_MAP_BYTES);
  MC_Generate(&w,0x12345678);assert(crc==MC_Crc32(w.blocks,MC_MAP_BYTES));
  MC_Generate(&w,0x12345679);assert(crc!=MC_Crc32(w.blocks,MC_MAP_BYTES));
  MC_Set(&w,-1,3,0,3);MC_Set(&w,128,3,0,3);MC_Set(&w,0,64,0,3);MC_Set(&w,0,0,0,MC_AIR);
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
  /* Doors use a thin closed slab, rotate in place when opened, and remain
     backward-compatible because the map stores the two block IDs directly. */
  flat(&w);MC_Set(&w,10,2,13,MC_DOOR);
  w.x=10.5f;w.y=2.01f;w.z=10.5f;w.yaw=0;w.pitch=-.3f;w.on_ground=1;
  assert(MC_IsSolid(MC_DOOR)&&!MC_IsSolid(MC_DOOR_OPEN));
  assert(MC_Raycast(&w,&x,&y,&z,&nx,&ny,&nz,5)&&x==10&&y==2&&z==13);
  for(int i=0;i<30;i++)MC_Step(&w,1,0,.1f,0);
  assert(w.z<12.72f);
  w.z=10.5f;assert(MC_OpenTarget(&w)&&MC_Get(&w,10,2,13)==MC_DOOR_OPEN);
  assert(MC_Raycast(&w,&x,&y,&z,&nx,&ny,&nz,5)&&z==13);
  w.z=10.5f;for(int i=0;i<30;i++)MC_Step(&w,1,0,.1f,0);assert(w.z>13.2f);
  w.z=10.5f;assert(MC_OpenTarget(&w)&&MC_Get(&w,10,2,13)==MC_DOOR);
  assert(!MC_Edit(&w,1,MC_DOOR_OPEN));
  flat(&w);MC_Set(&w,10,2,10,MC_CHEST);w.y=4;
  for(int i=0;i<80;i++)MC_Step(&w,0,0,.016f,0);
  assert(fabsf(w.y-2.875f)<.01f);
  flat(&w);MC_Set(&w,64,2,64,MC_BED);w.spawn[0]=65;w.spawn[1]=3;w.spawn[2]=65;
  MC_Respawn(&w);assert(fabsf(w.y-2.5725f)<.001f);
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
  free(allocation);puts("minicraft core tests: OK");return 0;
}
