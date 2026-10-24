#include "minicraft.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

static int inside(int x, int y, int z)
{ return (unsigned)x < MC_MAP_X && (unsigned)y < MC_MAP_Y && (unsigned)z < MC_MAP_Z; }
static unsigned index_of(int x, int y, int z)
{ return ((unsigned)y * MC_MAP_Z + (unsigned)z) * MC_MAP_X + (unsigned)x; }
static float clamp(float a, float lo, float hi) { return fminf(hi, fmaxf(lo, a)); }
static uint32_t hash(uint32_t x, uint32_t z, uint32_t seed)
{
  uint32_t h = x * 0x8da6b343u ^ z * 0xd8163841u ^ seed * 0xcb1ab31fu;
  h ^= h >> 13; h *= 0x85ebca6bu; h ^= h >> 16; return h;
}
static float noise(int x, int z, unsigned scale, uint32_t seed)
{
  unsigned gx = (unsigned)x / scale, gz = (unsigned)z / scale;
  float u = (float)((unsigned)x % scale) / scale, v = (float)((unsigned)z % scale) / scale;
  u = u*u*(3-2*u); v = v*v*(3-2*v);
  float a = (hash(gx,gz,seed)&65535)/65535.0f, b = (hash(gx+1,gz,seed)&65535)/65535.0f;
  float c = (hash(gx,gz+1,seed)&65535)/65535.0f, d = (hash(gx+1,gz+1,seed)&65535)/65535.0f;
  return (a+(b-a)*u)*(1-v)+(c+(d-c)*u)*v;
}
static int height(int x, int z, uint32_t seed)
{ return 10 + (int)(noise(x,z,24,seed)*18 + noise(x,z,8,seed+1)*7); }
/* Three broad terrain regions reuse existing block IDs, so the map and v4
   save layout stay unchanged: sandy desert, ordinary grassland and denser
   forest.  The centre is kept as grassland for a reliable spawn area. */
static int biome_at(int x,int z,uint32_t seed)
{
  if(abs(x-MC_MAP_X/2)<12 && abs(z-MC_MAP_Z/2)<12)return 0;
  float n=noise(x,z,48,seed+0x4d3bu);
  return n<.30f?1:n>.68f?2:0;
}

/* Small deterministic ruins give exploration a readable destination without
   adding another world data structure.  They are generated only for a new
   world; the chest coordinates and loot use the existing v4 container fields. */
static int ruin_site(int cx,int cz,uint32_t seed,int *base)
{
  int h=height(cx,cz,seed);
  if(h<17 || h>32 || (abs(cx-MC_MAP_X/2)<18 && abs(cz-MC_MAP_Z/2)<18))return 0;
  for(int dz=-3;dz<=3;dz++)for(int dx=-3;dx<=3;dx++) {
    int nh=height(cx+dx,cz+dz,seed);
    if(nh<17 || abs(nh-h)>1)return 0;
  }
  *base=h;return 1;
}
static int place_ruin(MC_World *w,int cx,int cz,uint32_t seed,int chest_slot)
{
  int h;if(!ruin_site(cx,cz,seed,&h))return 0;
  if(fabsf(cx+.5f-w->x)<20 && fabsf(cz+.5f-w->z)<20)return 0;
  /* Clear a small footprint and level its floor so the structure is walkable. */
  for(int dz=-3;dz<=3;dz++)for(int dx=-3;dx<=3;dx++) {
    int x=cx+dx,z=cz+dz,nh=height(x,z,seed);
    for(int y=nh+1;y<=h+6;y++)MC_Set(w,x,y,z,MC_AIR);
    for(int y=nh+1;y<=h;y++)MC_Set(w,x,y,z,MC_BRICK);
    MC_Set(w,x,h+1,z,MC_BRICK);
  }
  /* Three-block walls with a front doorway and a broken roof edge. */
  for(int y=h+2;y<=h+4;y++)for(int d=-3;d<=3;d++) {
    /* Leave the centre of the front wall open at every height.  The old
       bottom-only brick made the doorway catch the player's feet. */
    if(d)MC_Set(w,cx+d,y,cz-3,MC_BRICK);
    MC_Set(w,cx+d,y,cz+3,MC_BRICK);
    MC_Set(w,cx-3,y,cz+d,MC_BRICK);MC_Set(w,cx+3,y,cz+d,MC_BRICK);
  }
  for(int d=-3;d<=3;d+=2)MC_Set(w,cx+d,h+5,cz,MC_BRICK);
  MC_Set(w,cx-2,h+3,cz-2,MC_TORCH);MC_Set(w,cx+2,h+3,cz-2,MC_TORCH);
  MC_Set(w,cx,h+2,cz+1,MC_CHEST);
  if(chest_slot>=0 && chest_slot<MC_CHESTS) {
    w->chest_map[chest_slot][0]=(uint16_t)(cx+1);
    w->chest_map[chest_slot][1]=(uint16_t)(h+3);
    w->chest_map[chest_slot][2]=(uint16_t)(cz+2);
    w->chest_blocks[chest_slot][0]=(MC_Stack){MC_TORCH,8,0};
    w->chest_blocks[chest_slot][1]=(MC_Stack){MC_COAL,4,0};
    w->chest_blocks[chest_slot][2]=(MC_Stack){MC_APPLE,2,0};
    w->chest_blocks[chest_slot][3]=(MC_Stack){MC_STONE_PICK,1,(uint16_t)MC_ToolLife(MC_STONE_PICK)};
  }
  return 1;
}
enum { STRUCT_COLS=(MC_MAP_X-32)/8,STRUCT_ROWS=(MC_MAP_Z-32)/8,STRUCT_SITES=STRUCT_COLS*STRUCT_ROWS };
/* A seed-dependent permutation visits every site once, across the full map.
   Coprime stride avoids repeats and the old north-west-only search bias. */
static unsigned structure_order(uint32_t seed,unsigned *stride)
{
  unsigned step=(hash(seed,19,0x51f2u)%STRUCT_SITES)|1u;
  for(;;step+=2) {
    unsigned a=step,b=STRUCT_SITES;
    while(b){unsigned next=a%b;a=b;b=next;}
    if(a==1)break;
  }
  *stride=step;return hash(seed,31,0x9e37u)%STRUCT_SITES;
}
static void generate_ruins(MC_World *w,uint32_t seed)
{
  int count=0,placed_x[2]={0,0},placed_z[2]={0,0};
  unsigned stride,start=structure_order(seed,&stride);
  for(unsigned i=0;i<STRUCT_SITES && count<2;i++) {
    unsigned site=(start+i*stride)%STRUCT_SITES;
    int x=16+(int)(site%STRUCT_COLS)*8,z=16+(int)(site/STRUCT_COLS)*8,near=0;
    for(int n=0;n<count;n++) {
      int dx=x-placed_x[n],dz=z-placed_z[n];if(dx*dx+dz*dz<64*64)near=1;
    }
    if(!near && place_ruin(w,x,z,seed,count)){placed_x[count]=x;placed_z[count]=z;count++;}
  }
}
/* One underground supply room connects to an existing dry cave, never to
   an isolated pocket carved only for the chest. Reuse one free container
   slot, leaving at least three slots for player-built storage. */
static void generate_cave_cache(MC_World *w,uint32_t seed)
{
  unsigned slot=0;while(slot<MC_CHESTS && w->chest_map[slot][0])slot++;
  if(slot>=3 || slot>=MC_CHESTS)return;
  unsigned stride,start=structure_order(seed^0xc4a7u,&stride);
  for(unsigned i=0;i<STRUCT_SITES;i++) {
    unsigned site=(start+i*stride)%STRUCT_SITES;
    int cx=16+(int)(site%STRUCT_COLS)*8,cz=16+(int)(site/STRUCT_COLS)*8;
    for(int floor=7;floor<=15;floor+=4) {
      if(MC_Get(w,cx,floor+1,cz-3)!=MC_AIR || MC_Get(w,cx,floor+2,cz-3)!=MC_AIR ||
         !MC_IsSolid(MC_Get(w,cx,floor,cz-3)))continue;
      int valid=1;
      for(int dz=-2;dz<=2 && valid;dz++)for(int dx=-2;dx<=2 && valid;dx++) {
        if(height(cx+dx,cz+dz,seed)<floor+7)valid=0;
        for(int y=floor;y<=floor+4 && valid;y++) {
          uint8_t b=MC_Get(w,cx+dx,y,cz+dz);
          if(b!=MC_AIR && b!=MC_STONE && b!=MC_COAL_ORE && b!=MC_IRON_ORE)valid=0;
        }
      }
      if(!valid)continue;
      for(int dz=-2;dz<=2;dz++)for(int dx=-2;dx<=2;dx++)for(int y=floor;y<=floor+4;y++) {
        int shell=y==floor || y==floor+4 || abs(dx)==2 || abs(dz)==2;
        int entrance=dx==0 && dz==-2 && y>floor && y<floor+3;
        MC_Set(w,cx+dx,y,cz+dz,shell&&!entrance?MC_BRICK:MC_AIR);
      }
      MC_Set(w,cx-1,floor+1,cz+1,MC_TORCH);
      MC_Set(w,cx+1,floor+1,cz,MC_CHEST);
      w->chest_map[slot][0]=(uint16_t)(cx+2);w->chest_map[slot][1]=(uint16_t)(floor+2);w->chest_map[slot][2]=(uint16_t)(cz+1);
      w->chest_blocks[slot][0]=(MC_Stack){MC_IRON,2,0};
      w->chest_blocks[slot][1]=(MC_Stack){MC_COAL,6,0};
      w->chest_blocks[slot][2]=(MC_Stack){MC_BREAD,2,0};
      w->chest_blocks[slot][3]=(MC_Stack){MC_ARROW,8,0};
      return;
    }
  }
}

/* A few seed-stable two-cell lava pockets reward deeper cave exploration.
   Candidate count and resulting pool count are bounded; each pocket needs
   cave air, a solid floor, headroom, and separation from water and spawn. */
static void generate_lava_pools(MC_World *w,uint32_t seed)
{
  int placed[3][2],count=0;
  for(unsigned i=0;i<2048 && count<3;i++) {
    uint32_t h=hash(i*17u+1,i*131u+seed,seed^0x1a7a5eedu);
    int x=8+(int)((h>>1)%240),z=8+(int)((h>>9)%240);
    int y=4+(int)((h>>25)%8),dx=(h&1u)?1:0,dz=1-dx,x2=x+dx,z2=z+dz;
    if(abs(x-MC_MAP_X/2)<12 && abs(z-MC_MAP_Z/2)<12)continue;
    if(MC_Get(w,x,y,z)!=MC_AIR || MC_Get(w,x2,y,z2)!=MC_AIR ||
       !MC_IsSolid(MC_Get(w,x,y-1,z)) || !MC_IsSolid(MC_Get(w,x2,y-1,z2)) ||
       MC_Get(w,x,y+1,z)!=MC_AIR || MC_Get(w,x2,y+1,z2)!=MC_AIR)continue;
    int wet=0;
    for(int oz=-1;oz<=1&&!wet;oz++)for(int ox=-1;ox<=1;ox++)
      if(MC_Get(w,x+ox,y,z+oz)==MC_WATER || MC_Get(w,x2+ox,y,z+oz)==MC_WATER){wet=1;break;}
    if(wet)continue;
    int near=0;
    for(int j=0;j<count;j++)if(abs(x-placed[j][0])<12&&abs(z-placed[j][1])<12)near=1;
    if(near)continue;
    MC_Set(w,x,y,z,MC_LAVA);MC_Set(w,x2,y,z2,MC_LAVA);
    placed[count][0]=x;placed[count][1]=z;count++;
  }
}

uint8_t MC_Get(const MC_World *w, int x, int y, int z)
{
  if (x < 0 || x >= MC_MAP_X || z < 0 || z >= MC_MAP_Z || y < 0) return MC_BEDROCK;
  return y >= MC_MAP_Y ? MC_AIR : w->blocks[index_of(x,y,z)];
}
void MC_Set(MC_World *w, int x, int y, int z, uint8_t b)
{
  if(inside(x,y,z) && y>0 && (b==MC_AIR || MC_IsBlock(b))) {
    unsigned index=index_of(x,y,z);
    if(w->blocks[index]!=b) {
      w->blocks[index]=b;
      if(!++w->map_revision)w->redstone_valid=0;
    }
  }
}
static unsigned explore_index(int x,int z){return (unsigned)z*MC_MAP_X+(unsigned)x;}
int MC_IsExplored(const MC_World *w,int x,int z)
{
  if(!w||x<0||z<0||x>=MC_MAP_X||z>=MC_MAP_Z)return 0;
  unsigned i=explore_index(x,z);return (w->explored[i>>3]>>(i&7))&1u;
}
void MC_UpdateExploration(MC_World *w)
{
  if(!w||!w->blocks)return;
  int px=(int)floorf(w->x),pz=(int)floorf(w->z);
  if(w->explore_cell_x==px&&w->explore_cell_z==pz)return;
  w->explore_cell_x=(int16_t)px;w->explore_cell_z=(int16_t)pz;
  for(int dz=-8;dz<=8;dz++)for(int dx=-8;dx<=8;dx++) {
    int x=px+dx,z=pz+dz;if(x<0||z<0||x>=MC_MAP_X||z>=MC_MAP_Z)continue;
    unsigned i=explore_index(x,z);w->explored[i>>3]|=(uint8_t)(1u<<(i&7));
  }
}
static int rotated_door(uint8_t b){return b>=MC_DOOR_ROTATED_FIRST&&b<=MC_DOOR_ROTATED_LAST;}
uint8_t MC_DoorBase(uint8_t b){if(rotated_door(b))return (uint8_t)(b-((b-MC_DOOR_ROTATED_FIRST)&2));return b==MC_DOOR_TOP?MC_DOOR:b==MC_DOOR_TOP_OPEN?MC_DOOR_OPEN:b;}
int MC_DoorUpper(uint8_t b){return rotated_door(b)?((b-MC_DOOR_ROTATED_FIRST)&2)!=0:b==MC_DOOR_TOP||b==MC_DOOR_TOP_OPEN;}
int MC_IsDoor(uint8_t b){return rotated_door(b)||b==MC_DOOR||b==MC_DOOR_OPEN||b==MC_DOOR_TOP||b==MC_DOOR_TOP_OPEN;}
unsigned MC_DoorFacing(uint8_t b){return rotated_door(b)?1u+(b-MC_DOOR_ROTATED_FIRST)/4u:0;}
int MC_DoorOpen(uint8_t b){return rotated_door(b)?(b-MC_DOOR_ROTATED_FIRST)&1:b==MC_DOOR_OPEN||b==MC_DOOR_TOP_OPEN;}
uint8_t MC_DoorState(unsigned facing,int open,int upper)
{
  facing&=3;
  if(facing)return (uint8_t)(MC_DOOR_ROTATED_FIRST+(facing-1)*4+(upper?2:0)+(open?1:0));
  return upper?(open?MC_DOOR_TOP_OPEN:MC_DOOR_TOP):(open?MC_DOOR_OPEN:MC_DOOR);
}
void MC_RemoveBlock(MC_World *w,int x,int y,int z)
{
  uint8_t b=MC_Get(w,x,y,z);
  if(MC_IsDoor(b)) {
    int other=y+(MC_DoorUpper(b)?-1:1);uint8_t mate=MC_Get(w,x,other,z);
    if(MC_IsDoor(mate)&&MC_DoorUpper(mate)!=MC_DoorUpper(b)&&MC_DoorBase(mate)==MC_DoorBase(b))MC_Set(w,x,other,z,MC_AIR);
  }
  MC_Set(w,x,y,z,MC_AIR);
}
int MC_IsSolid(uint8_t block) { return block != MC_AIR && block != MC_WATER && block != MC_LAVA && block != MC_TORCH && block != MC_SAPLING && block != MC_WHEAT_CROP && block != MC_WHEAT_MATURE && block != MC_DUST && block != MC_DUST_ON && block != MC_LEVER && block != MC_LEVER_ON && block != MC_BUTTON && block != MC_BUTTON_ON; }

typedef struct { float lo[3], hi[3]; } MC_Part;

/* Cell-local parts.  The same data drives bounds, picking and rendering. */
static const MC_Part torch_parts[] = {
  /* The vanilla ground torch stem is a 4/16 square, not a 2/16 stick. */
  {{6/16.f, 0/16.f, 6/16.f}, {10/16.f, 10/16.f, 10/16.f}},
  /* The flame is fuller than the stem and tapers to the standard 14/16 top. */
  {{6/16.f, 9/16.f, 6/16.f}, {10/16.f, 12/16.f, 10/16.f}},
  {{13/32.f, 10/16.f, 13/32.f}, {19/32.f, 13/16.f, 19/32.f}},
  {{7/16.f, 12/16.f, 7/16.f}, {9/16.f, 14/16.f, 9/16.f}}
};
static const MC_Part fence_parts[] = {
  {{6/16.f,0,6/16.f},{10/16.f,1.5f,10/16.f}},
  {{0,6/16.f,6/16.f},{1,10/16.f,10/16.f}},
  {{6/16.f,6/16.f,0},{10/16.f,10/16.f,1}},
  {{0,11/16.f,6/16.f},{1,14/16.f,10/16.f}},
  {{6/16.f,11/16.f,0},{10/16.f,14/16.f,1}}
};
static const MC_Part dust_parts[] = {{{.08f,0,.08f},{.92f,.07f,.92f}}};
static const MC_Part lever_parts[2][4] = {
  {{{.25f,0,.25f},{.75f,.125f,.75f}},{{.4375f,.125f,.4375f},{.5625f,.30f,.5625f}},{{.4375f,.28f,.375f},{.5625f,.46f,.50f}},{{.4375f,.44f,.3125f},{.5625f,.625f,.4375f}}},
  {{{.25f,0,.25f},{.75f,.125f,.75f}},{{.4375f,.125f,.4375f},{.5625f,.30f,.5625f}},{{.4375f,.28f,.50f},{.5625f,.46f,.625f}},{{.4375f,.44f,.5625f},{.5625f,.625f,.6875f}}}
};
static const MC_Part button_parts[] = {{{.25f,.02f,.25f},{.75f,.22f,.75f}}};
/* Stair orientation is the direction in which the step rises. */
static const MC_Part stair_parts[4][2] = {
  {{{0,0,0},{1,.5f,1}},{{.5f,.5f,0},{1,1,1}}},
  {{{0,0,0},{1,.5f,1}},{{0,.5f,0},{.5f,1,1}}},
  {{{0,0,0},{1,.5f,1}},{{0,.5f,.5f},{1,1,1}}},
  {{{0,0,0},{1,.5f,1}},{{0,.5f,0},{1,1,.5f}}}
};

static unsigned parts_for(uint8_t b, const MC_Part **parts)
{
  if(b==MC_TORCH) { *parts=torch_parts; return sizeof(torch_parts)/sizeof(torch_parts[0]); }
  if(b==MC_FENCE) { *parts=fence_parts; return sizeof(fence_parts)/sizeof(fence_parts[0]); }
  if(b==MC_DUST || b==MC_DUST_ON) { *parts=dust_parts; return 1; }
  if(b==MC_LEVER || b==MC_LEVER_ON) { *parts=lever_parts[b==MC_LEVER_ON]; return 4; }
  if(b==MC_BUTTON || b==MC_BUTTON_ON) { *parts=button_parts; return 1; }
  if(b>=MC_STAIR && b<=MC_STAIR_WEST) { *parts=stair_parts[b-MC_STAIR]; return 2; }
  *parts=0; return 0;
}

/* Cell-local bounds, shared by ray selection, rendering and solid collision. */
static void bounds(uint8_t b,float lo[3],float hi[3])
{
  lo[0]=lo[1]=lo[2]=0;hi[0]=hi[1]=hi[2]=1;
  if(MC_IsDoor(b)) {
    /* Rotated doors remain one thin AABB, shared by all collision paths. */
    unsigned facing=MC_DoorFacing(b),open=(unsigned)MC_DoorOpen(b);
    unsigned axis=((facing&1)^(open?0:1))?2:0;
    unsigned high=open?(facing==1||facing==2):facing>=2;
    if(high)lo[axis]=13/16.f;else hi[axis]=3/16.f;
    return;
  }
  if(b==MC_TORCH || b==MC_FENCE || b==MC_DUST || b==MC_DUST_ON || b==MC_LEVER || b==MC_LEVER_ON || b==MC_BUTTON || b==MC_BUTTON_ON || (b>=MC_STAIR && b<=MC_STAIR_WEST)) {
    const MC_Part *parts; unsigned count=parts_for(b,&parts);
    lo[0]=lo[1]=lo[2]=1; hi[0]=hi[1]=hi[2]=0;
    for(unsigned i=0;i<count;i++) for(unsigned a=0;a<3;a++) {
      lo[a]=fminf(lo[a],parts[i].lo[a]); hi[a]=fmaxf(hi[a],parts[i].hi[a]);
    }
  }
  else if(b==MC_CHEST){lo[0]=lo[2]=1/16.f;hi[0]=hi[2]=15/16.f;hi[1]=14/16.f;}
  else if(b==MC_BED)hi[1]=9/16.f;
  else if(b==MC_SAPLING){lo[0]=lo[2]=2/16.f;hi[0]=hi[2]=14/16.f;hi[1]=13/16.f;}
  else if(b==MC_FARMLAND){hi[1]=15/16.f;}
  else if(b==MC_WHEAT_CROP){lo[0]=lo[2]=4/16.f;hi[0]=hi[2]=12/16.f;hi[1]=11/16.f;}
  else if(b==MC_WHEAT_MATURE){lo[0]=lo[2]=2/16.f;hi[0]=hi[2]=14/16.f;hi[1]=1.f;}
  else if(b==MC_SLAB){hi[1]=.5f;}
  else if(b==MC_SLAB_TOP){lo[1]=.5f;}
}
static int body_intersects(uint8_t b,int bx,int by,int bz,float x,float y,float z)
{
  const MC_Part *parts; unsigned count=parts_for(b,&parts);
  if(count) for(unsigned i=0;i<count;i++) {
    const float *lo=parts[i].lo,*hi=parts[i].hi;
    if(x+.28f>bx+lo[0] && x-.28f<bx+hi[0] && y+1.799f>by+lo[1] && y+.001f<by+hi[1] && z+.28f>bz+lo[2] && z-.28f< bz+hi[2]) return 1;
  }
  float lo[3],hi[3];bounds(b,lo,hi);
  if(count)return 0;
  return x+.28f>bx+lo[0] && x-.28f<bx+hi[0] &&
    y+1.799f>by+lo[1] && y+.001f<by+hi[1] &&
    z+.28f>bz+lo[2] && z-.28f<bz+hi[2];
}
static int overlaps(const MC_World *w, float x, float y, float z)
{
  int x0=(int)floorf(x-.28f), x1=(int)floorf(x+.28f);
  int z0=(int)floorf(z-.28f), z1=(int)floorf(z+.28f);
  int y0=(int)floorf(y+.001f), y1=(int)floorf(y+1.799f);
  for(int yy=y0; yy<=y1; ++yy) for(int zz=z0; zz<=z1; ++zz) for(int xx=x0; xx<=x1; ++xx)
    {
      uint8_t b=MC_Get(w,xx,yy,zz);
      if(MC_IsSolid(b) && body_intersects(b,xx,yy,zz,x,y,z))return 1;
    }
  /* A fence's upper half protrudes from the cell below the player's feet.
     Check it after ordinary hits, preserving the cheap ground-contact path. */
  if(y-y0<.5f)for(int zz=z0;zz<=z1;zz++)for(int xx=x0;xx<=x1;xx++)
    if(MC_Get(w,xx,y0-1,zz)==MC_FENCE && body_intersects(MC_FENCE,xx,y0-1,zz,x,y,z))return 1;
  return 0;
}
int MC_BodyClear(const MC_World *w,float x,float y,float z)
{return !overlaps(w,x,y,z);}
static int floor_at(const MC_World *w,float x,float feet,float z)
{
  const float y=feet-.08f;
  for(unsigned sample=0;sample<5;sample++) {
    float px=x+(sample==4?0:(sample&1)?.24f:-.24f),pz=z+(sample==4?0:(sample&2)?.24f:-.24f);
    int bx=(int)floorf(px),by=(int)floorf(y),bz=(int)floorf(pz);
    for(int below=0;below<2;below++) {
      uint8_t b=MC_Get(w,bx,by-below,bz);
      if(!MC_IsSolid(b)||(below&&b!=MC_FENCE))continue;
      const MC_Part *parts;unsigned count=parts_for(b,&parts);float lo[3],hi[3];
      if(!count){bounds(b,lo,hi);count=1;}
      float p[3]={px-bx,y-by+below,pz-bz};
      for(unsigned i=0;i<count;i++) {
        const float *a=parts?parts[i].lo:lo,*v=parts?parts[i].hi:hi;
        if(p[0]>=a[0]&&p[0]<=v[0]&&p[1]>=a[1]&&p[1]<v[1]&&p[2]>=a[2]&&p[2]<=v[2])return 1;
      }
    }
  }
  return 0;
}
static int has_floor(const MC_World *w,float x,float z)
{return floor_at(w,x,w->y,z);}
/* Bounded startup/respawn work only: choose an open viewing cone rather
   than pointing every new player at the same nearby tree or hillside. */
static float spawn_view(const MC_World *w,float x,float y,float z,float *yaw)
{
  float best=-1,sp=sinf(-.12f),cp=cosf(-.12f);
  unsigned start=hash(w->seed,47,0x5a17u)&15u;
  for(unsigned direction=0;direction<16;direction++) {
    float angle=((start+direction)&15u)*(6.2831853f/16),nearest=8,total=0;
    for(int fan=-2;fan<=2;fan++) {
      float a=angle+fan*.25f,t=1;
      if(MC_BlockSegment(w,x,y+1.62f,z,x+sinf(a)*cp*8,y+1.62f+sp*8,z+cosf(a)*cp*8,&t))t=fmaxf(0,t);
      else t=1;
      float clear=t*8;nearest=fminf(nearest,clear);total+=clear;
    }
    float score=nearest*100+total*2;
    if(score>best){best=score;*yaw=angle;}
  }
  return best;
}
static void face_spawn(MC_World *w)
{spawn_view(w,w->x,w->y,w->z,&w->yaw);w->pitch=-.12f;}
static int spawn_floor(const MC_World *w,int x,int y,int z,float *feet)
{
  uint8_t b=MC_Get(w,x,y,z);if(!MC_IsSolid(b))return 0;
  float lo[3],hi[3];bounds(b,lo,hi);float fy=y+hi[1]+.01f;
  if(overlaps(w,x+.5f,fy,z+.5f) || !floor_at(w,x+.5f,fy,z+.5f) ||
     MC_Get(w,x,(int)floorf(fy+.5f),z)==MC_WATER || MC_Get(w,x,(int)floorf(fy+1.62f),z)==MC_WATER)return 0;
  *feet=fy;return 1;
}
void MC_Respawn(MC_World *w)
{
  w->vy=0;w->on_ground=0;w->fall_speed=0;
  memset(w->mobs,0,sizeof(w->mobs));w->mob_target=255;w->spawn_clock=0;
  memset(w->projectiles,0,sizeof(w->projectiles));w->target_valid=0;w->mining=0;
  if(w->spawn[0] && MC_Get(w,w->spawn[0]-1,w->spawn[1]-1,w->spawn[2]-1)==MC_BED) {
    int bx=w->spawn[0]-1,by=w->spawn[1]-1,bz=w->spawn[2]-1;
    for(int dz=-1;dz<=1;dz++)for(int dx=-1;dx<=1;dx++) {
      if(!dx&&!dz)continue;
      for(int floor=by;floor>=by-1;floor--) {
        float y;
        if(spawn_floor(w,bx+dx,floor,bz+dz,&y)) {
          w->x=bx+dx+.5f;w->y=y;w->z=bz+dz+.5f;face_spawn(w);return;
        }
      }
    }
    float y;
    if(spawn_floor(w,bx,by,bz,&y)){w->x=bx+.5f;w->y=y;w->z=bz+.5f;face_spawn(w);return;}
  }
  for(int pass=0;pass<2;pass++)for(int r=0;r<64;r++)for(int dz=-r;dz<=r;dz++)for(int dx=-r;dx<=r;dx++) {
    if(r && abs(dx)!=r && abs(dz)!=r)continue;
    int x=MC_MAP_X/2+dx,z=MC_MAP_Z/2+dz;if(x<1||x>=MC_MAP_X-1||z<1||z>=MC_MAP_Z-1)continue;
    for(int y=MC_MAP_Y-1;y>=0;y--) {
      uint8_t b=MC_Get(w,x,y,z);
      if(!pass && (b==MC_LEAVES||b==MC_WOOD||b==MC_BIRCH_WOOD||b==MC_DARK_WOOD))continue;
      float feet;
      if(spawn_floor(w,x,y,z,&feet)) {
        w->x=x+.5f;w->y=feet;w->z=z+.5f;face_spawn(w);return;
      }
    }
  }
  w->x=MC_MAP_X/2+.5f;w->z=MC_MAP_Z/2+.5f;w->y=MC_MAP_Y+1;
}
static void generate_world(MC_World *w,uint32_t seed,int structures)
{
  /* Clear old world's respawn/container state before selecting a new spawn. */
  MC_NewPlayer(w,0);
  memset(w->blocks,0,MC_MAP_BYTES); w->seed=seed;
  for(int z=0; z<MC_MAP_Z; ++z) for(int x=0; x<MC_MAP_X; ++x) {
    int h=height(x,z,seed),biome=biome_at(x,z,seed); w->blocks[index_of(x,0,z)]=MC_BEDROCK;
    for(int y=1; y<=h; ++y) {
      uint8_t top=h<17||biome==1?MC_SAND:MC_GRASS;
      uint8_t fill=biome==1&&y>=h-3?MC_SAND:y>h-4?MC_DIRT:MC_STONE;
      MC_Set(w,x,y,z,y==h?top:fill);
    }
    for(int y=h+1; y<=16; ++y) MC_Set(w,x,y,z,MC_WATER);
  }
  /* Trees are a second pass, so neighboring columns cannot erase their crowns. */
  for(int z=4; z<MC_MAP_Z-4; z+=6) for(int x=4; x<MC_MAP_X-4; x+=6) {
    int biome=biome_at(x,z,seed);
    if(biome==1 || (hash(x,z,seed)&15)>(biome==2?8:4) || (abs(x-MC_MAP_X/2)<4 && abs(z-MC_MAP_Z/2)<4)) continue;
    int h=height(x,z,seed); if(h<18 || MC_Get(w,x,h,z)!=MC_GRASS) continue;
    uint8_t trunk=biome==2?MC_DARK_WOOD:((hash(x,z,seed+0x9e37u)&7u)==0u?MC_BIRCH_WOOD:MC_WOOD);
    for(int y=h+1; y<=h+4; ++y) MC_Set(w,x,y,z,trunk);
    for(int dy=3; dy<=6; ++dy) for(int dz=-2; dz<=2; ++dz) for(int dx=-2; dx<=2; ++dx)
      if(abs(dx)+abs(dz)+(dy==6?2:0)<=4 && MC_Get(w,x+dx,h+dy,z+dz)==MC_AIR)
        MC_Set(w,x+dx,h+dy,z+dz,MC_LEAVES);
  }
  /* New worlds only: ore clusters and broad underground caves. */
  for(int z=1;z<MC_MAP_Z-1;z++) for(int x=1;x<MC_MAP_X-1;x++) {
    int top=height(x,z,seed)-5;
    for(int y=2;y<top;y++) {
      if(MC_Get(w,x,y,z)!=MC_STONE)continue;
      uint32_t h=hash(x/2+((unsigned)y/2)*71,z/2,seed+31);
      if((h&127)<9)MC_Set(w,x,y,z,MC_COAL_ORE);
      else if(y<19 && ((h>>8)&127)<6)MC_Set(w,x,y,z,MC_IRON_ORE);
      float cave=noise(x,z,12,seed+83+(unsigned)(y/4));
      if(y>3 && cave>.78f && y<top-2)MC_Set(w,x,y,z,MC_AIR);
    }
  }
  generate_lava_pools(w,seed);
  w->yaw=.7f; w->pitch=-.12f; MC_Respawn(w);
  /* Shortlist eight flat grass candidates before doing viewing-cone tests.
     Height is a minor preference, not a reason to choose a tree-lined peak. */
  struct {int x,y,z,score;} candidates[8];unsigned count=0;
  for(int z=MC_MAP_Z/2-40;z<MC_MAP_Z/2+40;z+=2)for(int x=MC_MAP_X/2-40;x<MC_MAP_X/2+40;x+=2) {
    int y=height(x,z,seed);
    if(MC_Get(w,x,y,z)!=MC_GRASS || MC_Get(w,x,y+1,z)!=MC_AIR || MC_Get(w,x,y+2,z)!=MC_AIR)continue;
    float feet;if(!spawn_floor(w,x,y,z,&feet))continue;
    int score=y/4-abs(x-MC_MAP_X/2)-abs(z-MC_MAP_Z/2);
    const int offsets[4][2]={{-1,0},{1,0},{0,-1},{0,1}};
    for(unsigned i=0;i<4;i++) {
      int nx=x+offsets[i][0],nz=z+offsets[i][1];float nf;
      if(spawn_floor(w,nx,y,nz,&nf))score+=20;
    }
    unsigned at=0;while(at<count && candidates[at].score>=score)at++;
    if(at<8) {
      if(count<8)count++;
      for(unsigned i=count-1;i>at;i--)candidates[i]=candidates[i-1];
      candidates[at].x=x;candidates[at].y=y;candidates[at].z=z;candidates[at].score=score;
    }
  }
  float best=-1e30f;
  for(unsigned i=0;i<count;i++) {
    int x=candidates[i].x,y=candidates[i].y,z=candidates[i].z,wood=400;float yaw;
    for(int tz=4+((z-22)/6)*6;tz<=z+18;tz+=6)for(int tx=4+((x-22)/6)*6;tx<=x+18;tx+=6) {
      int dx=tx-x,dz=tz-z,d=dx*dx+dz*dz;if(d>=wood)continue;
      uint8_t b=MC_Get(w,tx,height(tx,tz,seed)+1,tz);
      if(b==MC_WOOD||b==MC_BIRCH_WOOD||b==MC_DARK_WOOD)wood=d;
    }
    float score=spawn_view(w,x+.5f,y+1.01f,z+.5f,&yaw)+candidates[i].score+(wood<=324?50-wood/10:-20);
    if(score>best){best=score;w->x=x+.5f;w->y=y+1.01f;w->z=z+.5f;w->yaw=yaw;w->pitch=-.12f;}
  }
  MC_NewPlayer(w,0);
  if(structures){generate_ruins(w,seed);generate_cave_cache(w,seed);}
}
void MC_Generate(MC_World *w,uint32_t seed){generate_world(w,seed,1);}
void MC_GenerateTerrain(MC_World *w,uint32_t seed){generate_world(w,seed,0);}
static int step_half(MC_World *w,float x,float z)
{
  if(w->vy>0 || !has_floor(w,w->x,w->z))return 0;
  int part=0,yy=(int)floorf(w->y+.1f);
  for(int dz=-1;dz<=1;dz+=2)for(int dx=-1;dx<=1;dx+=2) {
    uint8_t b=MC_Get(w,(int)floorf(x+dx*.28f),yy,(int)floorf(z+dz*.28f));
    if(b==MC_SLAB || (b>=MC_STAIR&&b<=MC_STAIR_WEST))part=1;
  }
  if(!part || overlaps(w,w->x,w->y+.5f,w->z) || overlaps(w,x,w->y+.5f,z))return 0;
  w->y+=.5f;w->vy=0;return 1;
}
void MC_Step(MC_World *w, float f, float s, float dt, int jump)
{
  if(!(dt>0)) return;
  dt=fminf(dt,.1f);
  float length=sqrtf(f*f+s*s); if(length>1) { f/=length; s/=length; }
  int wet=MC_Get(w,(int)w->x,(int)(w->y+.5f),(int)w->z)==MC_WATER;
  float vx=(s*cosf(w->yaw)+f*sinf(w->yaw))*4.2f;
  float vz=(f*cosf(w->yaw)-s*sinf(w->yaw))*4.2f;
  if(w->sneak){vx*=.35f;vz*=.35f;}
  if(wet) {vx*=.55f;vz*=.55f;if(jump)w->vy=3.5f;}
  if(jump && w->on_ground) { w->vy=7.2f; w->on_ground=0; }
  /* Use actual foot support: tiny collision substeps can briefly clear on_ground. */
  int guard=w->sneak && !wet && w->vy<=0 && has_floor(w,w->x,w->z);
  /* Small steps prevent passing through one-block walls at low frame rates. */
  while(dt>0) {
    float h=fminf(dt,.01f); dt-=h;
    float nx=w->x+vx*h, nz=w->z+vz*h;
    if(!overlaps(w,nx,w->y,w->z) && (!guard || has_floor(w,nx,w->z))) w->x=nx;
    else if(!wet && !guard && step_half(w,nx,w->z))w->x=nx;
    if(!overlaps(w,w->x,w->y,nz) && (!guard || has_floor(w,w->x,nz))) w->z=nz;
    else if(!wet && !guard && step_half(w,w->x,nz))w->z=nz;
    w->vy-=(wet?4.0f:18.0f)*h;if(wet && w->vy < -2)w->vy=-2; float ny=w->y+w->vy*h;
    if(overlaps(w,w->x,ny,w->z)) {
      w->on_ground=w->vy<0;
      /* Move as far as possible without entering a block (also handles ceilings). */
      float a=0,b=1;
      for(int k=0;k<10;++k) { float t=(a+b)*.5f; if(overlaps(w,w->x,w->y+(ny-w->y)*t,w->z)) b=t; else a=t; }
      w->y+=(ny-w->y)*a; w->vy=0;
    } else { w->y=ny; w->on_ground=0; }
  }
  if(w->y<1 || w->y>MC_MAP_Y+4) MC_Respawn(w);
}

typedef MC_Hit Hit;
static int shaped(uint8_t b)
{return b==MC_TORCH || b==MC_SAPLING || b==MC_BED || b==MC_CHEST || MC_IsDoor(b) || b==MC_SLAB || b==MC_SLAB_TOP || b==MC_FARMLAND || b==MC_WHEAT_CROP || b==MC_WHEAT_MATURE || b==MC_FENCE || (b>=MC_DUST && b<=MC_BUTTON_ON) || (b>=MC_STAIR && b<=MC_STAIR_WEST);}

/* Intersect only special objects; ordinary cubes retain the cheap DDA path. */
static int box_hit(const float lo[3],const float hi[3],const float p[3],const float d[3],
                   float start,float end,float *distance,int *nx,int *ny,int *nz)
{
  float near=-1e30f,far=1e30f;int near_axis=0,far_axis=0,near_sign=0,far_sign=0;
  for(int a=0;a<3;a++) {
    if(fabsf(d[a])<1e-7f){if(p[a]<lo[a] || p[a]>hi[a])return 0;continue;}
    float t0=(lo[a]-p[a])/d[a],t1=(hi[a]-p[a])/d[a];int s=d[a]>0?-1:1;
    if(t0>t1){float tmp=t0;t0=t1;t1=tmp;}
    if(t0>near){near=t0;near_axis=a;near_sign=s;}
    if(t1<far){far=t1;far_axis=a;far_sign=-s;}
    if(near>far)return 0;
  }
  int axis=near_axis,sign=near_sign;float t=near;
  if(t<start-.00001f){t=far;axis=far_axis;sign=far_sign;}
  if(t<start-.00001f || t>end)return 0;
  *distance=t;*nx=axis==0?sign:0;*ny=axis==1?sign:0;*nz=axis==2?sign:0;return 1;
}
static int shape_hit(uint8_t b,const float p[3],const float d[3],float start,float end,
                     float *distance,int *nx,int *ny,int *nz)
{
  if(b==MC_TORCH) {
    const MC_Part *parts; unsigned count=parts_for(b,&parts);
    float enter[4],leave[4];int en[4][2],ex[4][2],valid[4]={0,0,0,0};
    for(unsigned part=0;part<count;part++) {
      float a=-1e30f,z=1e30f;int na=0,fa=0,ns=0,fs=0,intersects=1;
      for(int axis=0;axis<3;axis++) {
        if(fabsf(d[axis])<1e-7f) {
          if(p[axis]<parts[part].lo[axis]||p[axis]>parts[part].hi[axis])intersects=0;
          continue;
        }
        float u=(parts[part].lo[axis]-p[axis])/d[axis],v=(parts[part].hi[axis]-p[axis])/d[axis];
        int sign=d[axis]>0?-1:1;
        if(u>v){float t=u;u=v;v=t;}
        if(u>a){a=u;na=axis;ns=sign;}
        if(v<z){z=v;fa=axis;fs=-sign;}
        if(a>z)intersects=0;
      }
      if(!intersects||z<start||a>end)continue;
      valid[part]=1;enter[part]=a;leave[part]=z;
      en[part][0]=na;en[part][1]=ns;ex[part][0]=fa;ex[part][1]=fs;
    }
    float best=end+1; int best_axis=0,best_sign=0;
    for(unsigned first=0;first<count;first++) if(valid[first]) {
      /* Merge overlapping parts so a ray starting in the stem never hits an
         invisible seam where the stem and flame overlap. */
      float a=enter[first],z=leave[first];
      int axis=en[first][0],sign=en[first][1];
      int exit_axis=ex[first][0],exit_sign=ex[first][1];
      int expanded;
      do {
        expanded=0;
        for(unsigned second=0;second<count;second++) if(second!=first&&valid[second] &&
            enter[second]<=z+1e-6f && leave[second]>=a-1e-6f) {
          if(enter[second]<a-1e-6f) { a=enter[second]; axis=en[second][0]; sign=en[second][1]; expanded=1; }
          if(leave[second]>z+1e-6f) { z=leave[second]; exit_axis=ex[second][0]; exit_sign=ex[second][1]; expanded=1; }
        }
      } while(expanded);
      float t; int hit_axis,hit_sign;
      if(a>=start-.00001f) { t=a; hit_axis=axis; hit_sign=sign; }
      else { t=z; hit_axis=exit_axis; hit_sign=exit_sign; }
      if(t>=start-.00001f && t<=end && t<best) { best=t;best_axis=hit_axis;best_sign=hit_sign; }
    }
    if(best>end)return 0;
    int axis=best_axis,sign=best_sign;float t=best;
    *distance=t;*nx=axis==0?sign:0;*ny=axis==1?sign:0;*nz=axis==2?sign:0;
    return 1;
  }
  if(b==MC_SAPLING) {
    float best=end+1;int normal=0;
    for(int plane=0;plane<2;plane++) {
      float sign=plane?1.f:-1.f,den=d[0]+sign*d[2];
      if(fabsf(den)<1e-7f)continue;
      float t=((plane?1.f:0.f)-p[0]-sign*p[2])/den;
      if(t<start-.00001f || t>end || t>=best)continue;
      float u=p[0]+d[0]*t,v=p[1]+d[1]*t;
      /* A thin stem and stepped leafy crown, transparent around the silhouette. */
      float half=v<.25f?1/16.f:v<.4375f?5/16.f:v<.625f?6/16.f:3/16.f;
      if(v<0 || v>13/16.f || fabsf(u-.5f)>half)continue;
      best=t;normal=den>0?-1:1;
    }
    if(!normal)return 0;
    *distance=best;*nx=normal;*ny=*nz=0;return 1;
  }
  {
    const MC_Part *parts; unsigned count=parts_for(b,&parts);
    if(count) {
      float best=end+1; int bx=0,by=0,bz=0;
      for(unsigned i=0;i<count;i++) {
        float t; int nx0,ny0,nz0;
        if(box_hit(parts[i].lo,parts[i].hi,p,d,start,end,&t,&nx0,&ny0,&nz0) && t<best) {
          best=t;bx=nx0;by=ny0;bz=nz0;
        }
      }
      if(best<=end) {*distance=best;*nx=bx;*ny=by;*nz=bz;return 1;}
      return 0;
    }
  }
  float lo[3],hi[3];bounds(b,lo,hi);
  return box_hit(lo,hi,p,d,start,end,distance,nx,ny,nz);
}
/* DDA advances once per crossed block. The parameter is forward depth for rendering. */
static int trace(const MC_World *w,float dx,float dy,float dz,float reach,Hit *hit)
{
  float px=w->x,py=w->y+1.62f,pz=w->z;
  int x=(int)floorf(px),y=(int)floorf(py),z=(int)floorf(pz);
  int sx=dx<0?-1:1,sy=dy<0?-1:1,sz=dz<0?-1:1;
  float ix=fabsf(dx)>1e-7f?1/fabsf(dx):1e30f;
  float iy=fabsf(dy)>1e-7f?1/fabsf(dy):1e30f;
  float iz=fabsf(dz)>1e-7f?1/fabsf(dz):1e30f;
  float tx=(dx<0?px-x:x+1-px)*ix,ty=(dy<0?py-y:y+1-py)*iy,tz=(dz<0?pz-z:z+1-pz)*iz;
  int nx=0,ny=0,nz=0;
  for(float t=0;t<=reach;) {
    if(!inside(x,y,z)) return 0;
    uint8_t b=w->blocks[index_of(x,y,z)];
    if(b!=MC_AIR && shaped(b)) {
      const float p[3]={px-x,py-y,pz-z},d[3]={dx,dy,dz};float distance;
      int hx,hy,hz;
      if(shape_hit(b,p,d,t,fminf(reach,fminf(tx,fminf(ty,tz))),&distance,&hx,&hy,&hz)) {
        *hit=(Hit){x,y,z,hx,hy,hz,distance,b};return 1;
      }
      b=MC_AIR;
    }
    /* Render cutouts without changing solid collision / touch selection. */
    if(reach>6 && (b==MC_GLASS || b==MC_LEAVES)) {
      float hx=px+dx*t,hy=py+dy*t,hz=pz+dz*t;
      unsigned u=(unsigned)(int)floorf((nx?hz:hx)*16)&15;
      unsigned v=(unsigned)(int)floorf((ny?hz:hy)*16)&15;
      if(b==MC_GLASS && u>0 && u<15 && v>0 && v<15 && u!=v && u+1!=v)b=MC_AIR;
      else if(b==MC_LEAVES && (hash(u,v,7)&15)<3)b=MC_AIR;
    }
    if(reach>6 && b==MC_WATER && MC_Get(w,(int)px,(int)py,(int)pz)==MC_WATER)b=MC_AIR;
    if(b!=MC_AIR) { *hit=(Hit){x,y,z,nx,ny,nz,t,b}; return 1; }
    if(tx<=ty && tx<=tz) { x+=sx; t=tx; tx+=ix; nx=-sx; ny=nz=0; }
    else if(ty<=tz) { y+=sy; t=ty; ty+=iy; ny=-sy; nx=nz=0; }
    else { z+=sz; t=tz; tz+=iz; nz=-sz; nx=ny=0; }
  }
  return 0;
}
int MC_BlockSegment(const MC_World *w,float px,float py,float pz,float ex,float ey,float ez,float *fraction)
{
  float d[3]={ex-px,ey-py,ez-pz};
  if(!isfinite(px)||!isfinite(py)||!isfinite(pz)||!isfinite(ex)||!isfinite(ey)||!isfinite(ez))return 0;
  /* Arrows outside the playable volume hit its boundary, including launches
     inside a wall. Limit casts to map-sized coordinates before conversion. */
  if(px<0||px>=MC_MAP_X||pz<0||pz>=MC_MAP_Z||py<0) {if(fraction)*fraction=0;return 1;}
  if(py>MC_MAP_Y+128 || fabsf(ex-px)>MC_MAP_X || fabsf(ey-py)>MC_MAP_Y || fabsf(ez-pz)>MC_MAP_Z)return 0;
  int x=(int)floorf(px),y=(int)floorf(py),z=(int)floorf(pz);
  int sx=d[0]<0?-1:1,sy=d[1]<0?-1:1,sz=d[2]<0?-1:1;
  float ix=fabsf(d[0])>1e-7f?1/fabsf(d[0]):1e30f,iy=fabsf(d[1])>1e-7f?1/fabsf(d[1]):1e30f,iz=fabsf(d[2])>1e-7f?1/fabsf(d[2]):1e30f;
  float tx=(d[0]<0?px-x:x+1-px)*ix,ty=(d[1]<0?py-y:y+1-py)*iy,tz=(d[2]<0?pz-z:z+1-pz)*iz;
  for(float t=0;t<=1;) {
    float end=fminf(1,fminf(tx,fminf(ty,tz))),best=2;
    /* Fence posts extend into the cell above their base. */
    for(int below=0;below<2;below++) {
      uint8_t b=MC_Get(w,x,y-below,z);
      if(!MC_IsSolid(b)||(below&&b!=MC_FENCE))continue;
      float p[3]={px-x,py-y+below,pz-z},lo[3],hi[3];
      const MC_Part *parts;unsigned count=parts_for(b,&parts);
      if(!count){bounds(b,lo,hi);count=1;}
      for(unsigned part=0;part<count;part++) {
        const float *a=parts?parts[part].lo:lo,*v=parts?parts[part].hi:hi;
        float q[3]={p[0]+d[0]*t,p[1]+d[1]*t,p[2]+d[2]*t},hit;int hx,hy,hz;
        if(q[0]>=a[0]&&q[0]<=v[0]&&q[1]>=a[1]&&q[1]<=v[1]&&q[2]>=a[2]&&q[2]<=v[2])best=t;
        else if(box_hit(a,v,p,d,t,end,&hit,&hx,&hy,&hz)&&hit<best)best=hit;
      }
    }
    if(best<=1){if(fraction)*fraction=fmaxf(0,best);return 1;}
    if(tx<=ty&&tx<=tz){x+=sx;t=tx;tx+=ix;}
    else if(ty<=tz){y+=sy;t=ty;ty+=iy;}
    else{z+=sz;t=tz;tz+=iz;}
  }
  return 0;
}
int MC_Raycast(const MC_World *w,int *x,int *y,int *z,int *nx,int *ny,int *nz,float reach)
{
  Hit h;
  if(!trace(w,sinf(w->yaw)*cosf(w->pitch),sinf(w->pitch),cosf(w->yaw)*cosf(w->pitch),reach,&h)) return 0;
  *x=h.x; *y=h.y; *z=h.z; *nx=h.nx; *ny=h.ny; *nz=h.nz; return 1;
}
int MC_Pick(const MC_World *w,float x,float y,MC_Hit *hit)
{
  float u=(x-400)/576,v=(240-y)/576;
  float sy=sinf(w->yaw),cy=cosf(w->yaw),sp=sinf(w->pitch),cp=cosf(w->pitch);
  float dx=sy*cp+u*cy-v*sp*sy,dy=sp+v*cp,dz=cy*cp-u*sy-v*sp*cy;
  float len=sqrtf(dx*dx+dy*dy+dz*dz);
  return trace(w,dx/len,dy/len,dz/len,5,hit);
}
int MC_Edit(MC_World *w,int place,uint8_t selected)
{ return MC_EditAt(w,place,selected,400,240); }
int MC_EditAt(MC_World *w,int place,uint8_t selected,float sx,float sy)
{
  MC_Hit hit;
  if(!MC_Pick(w,sx,sy,&hit))return 0;
  int x=hit.x,y=hit.y,z=hit.z,nx=hit.nx,ny=hit.ny,nz=hit.nz;
  if(place) {
    if((MC_IsDoor(selected)&&selected!=MC_DOOR) || !MC_IsBlock(selected) || selected==MC_BEDROCK || selected==MC_WATER || selected==MC_DOUBLE_SLAB || !(nx||ny||nz)) return 0;
    if((selected==MC_SLAB || selected==MC_SLAB_TOP) &&
       ((hit.block==MC_SLAB && ny>0) || (hit.block==MC_SLAB_TOP && ny<0))) {
      if(body_intersects(MC_DOUBLE_SLAB,x,y,z,w->x,w->y,w->z))return 0;
      MC_Set(w,x,y,z,MC_DOUBLE_SLAB);return 1;
    }
    x+=nx; y+=ny; z+=nz;
    uint8_t old=MC_Get(w,x,y,z);
    if((selected==MC_SLAB && old==MC_SLAB_TOP) || (selected==MC_SLAB_TOP && old==MC_SLAB)) {
      if(body_intersects(MC_DOUBLE_SLAB,x,y,z,w->x,w->y,w->z))return 0;
      MC_Set(w,x,y,z,MC_DOUBLE_SLAB);return 1;
    }
    if(!inside(x,y,z) || y==0 || (old!=MC_AIR && old!=MC_WATER)) return 0;
    if(selected==MC_DOOR) {
      unsigned facing=(unsigned)(int)floorf(w->yaw*(2/3.141592654f)+.5f)&3;
      selected=MC_DoorState(facing,0,0);
      uint8_t above=MC_Get(w,x,y+1,z);
      if(y>=MC_MAP_Y-1 || !MC_IsSolid(MC_Get(w,x,y-1,z)) || (above!=MC_AIR&&above!=MC_WATER) ||
         body_intersects(selected,x,y,z,w->x,w->y,w->z) || body_intersects(selected,x,y+1,z,w->x,w->y,w->z))return 0;
      MC_Set(w,x,y+1,z,MC_DoorState(facing,0,1));
    }
    if(MC_IsSolid(selected) && body_intersects(selected,x,y,z,w->x,w->y,w->z)) return 0;
    MC_Set(w,x,y,z,selected);
  } else {
    if(!inside(x,y,z) || y==0 || MC_Get(w,x,y,z)==MC_BEDROCK) return 0;
    MC_RemoveBlock(w,x,y,z);
  }
  return 1;
}
static uint16_t rgb(unsigned r,unsigned g,unsigned b)
{ return (uint16_t)(((r>>3)<<11)|((g>>2)<<5)|(b>>3)); }

void MC_Render(const MC_World *w,uint16_t *pixels,unsigned width,unsigned height)
{
  float syaw=sinf(w->yaw),cyaw=cosf(w->yaw),sp=sinf(w->pitch),cp=cosf(w->pitch);
  float focal=width*.72f,phase=w->time*(6.2831853f/600),sun=sinf(phase);
  float daylight=clamp(sun*1.5f+.35f,.08f,1);
  float sunx=cosf(phase)*.8f,suny=sun,sunz=.6f;
  float sun_norm=1/sqrtf(1+sun*sun);
  int underwater=MC_Get(w,(int)w->x,(int)(w->y+1.62f),(int)w->z)==MC_WATER;
  /* Project eight bounding boxes once; only candidate pixels cast entity rays. */
  int mob_rect[MC_MOBS][4];unsigned mob_mask=0;
  for(unsigned i=0;i<MC_MOBS;i++)if(w->mobs[i].health) {
    const MC_Mob *m=&w->mobs[i];
    float halfx=m->type?.50f:.30f, halfz=m->type?.28f:.30f, top=m->type?1.25f:1.80f;
    float minx=width,miny=height,maxx=0,maxy=0;int behind=0,front=0;
    for(int corner=0;corner<8;corner++) {
      float px=m->x+((corner&1)?halfx:-halfx)-w->x;
      float py=m->y+((corner&2)?top:0)-w->y-1.62f;
      float pz=m->z+((corner&4)?halfz:-halfz)-w->z;
      float depth=px*syaw*cp+py*sp+pz*cyaw*cp;
      if(depth<=.01f){behind=1;continue;}front=1;
      float sx=width*.5f+focal*(px*cyaw-pz*syaw)/depth;
      float sy=height*.5f-focal*(-px*sp*syaw+py*cp-pz*sp*cyaw)/depth;
      minx=fminf(minx,sx);maxx=fmaxf(maxx,sx);miny=fminf(miny,sy);maxy=fmaxf(maxy,sy);
    }
    if(!front)continue;
    if(behind){minx=miny=0;maxx=width;maxy=height;}
    if(maxx<0||maxy<0||minx>=width||miny>=height)continue;
    mob_rect[i][0]=(int)clamp(floorf(minx),0,width);mob_rect[i][1]=(int)clamp(floorf(miny),0,height);
    mob_rect[i][2]=(int)clamp(ceilf(maxx),0,width);mob_rect[i][3]=(int)clamp(ceilf(maxy),0,height);mob_mask|=1u<<i;
  }
  int drop_rect[MC_DROPS][4];float drop_depth[MC_DROPS];unsigned drop_mask=0;
  for(unsigned i=0;i<MC_DROPS;i++)if(w->drops[i].count) {
    const MC_Drop *d=&w->drops[i];float px=d->x-w->x,py=d->y-w->y-1.62f,pz=d->z-w->z;
    float depth=px*syaw*cp+py*sp+pz*cyaw*cp;if(depth<=.05f)continue;
    float sx=width*.5f+focal*(px*cyaw-pz*syaw)/depth;
    float sy=height*.5f-focal*(-px*sp*syaw+py*cp-pz*sp*cyaw)/depth;
    float half=fmaxf(1.5f,focal*.12f/depth);
    if(sx+half<0||sy+half<0||sx-half>=width||sy-half>=height)continue;
    drop_rect[i][0]=(int)clamp(floorf(sx-half),0,width);drop_rect[i][1]=(int)clamp(floorf(sy-half),0,height);
    drop_rect[i][2]=(int)clamp(ceilf(sx+half),0,width);drop_rect[i][3]=(int)clamp(ceilf(sy+half),0,height);
    drop_depth[i]=depth;drop_mask|=1u<<i;
  }
  /* Project bounded arrow entities once.  The arrow is a short screen-space
     segment, so the pixel loop only checks a tiny rectangle per active slot. */
  int projectile_rect[MC_PROJECTILES][4];
  float projectile_depth[MC_PROJECTILES],projectile_ax[MC_PROJECTILES],projectile_ay[MC_PROJECTILES];
  float projectile_bx[MC_PROJECTILES],projectile_by[MC_PROJECTILES];
  unsigned projectile_mask=0;
  for(unsigned i=0;i<MC_PROJECTILES;i++)if(w->projectiles[i].active) {
    const MC_Projectile *p=&w->projectiles[i];
    float cx=p->x-w->x,cy=p->y-w->y-1.62f,cz=p->z-w->z;
    float tx=p->x-p->vx*.075f-w->x,ty=p->y-p->vy*.075f-w->y-1.62f,tz=p->z-p->vz*.075f-w->z;
    float depth=cx*syaw*cp+cy*sp+cz*cyaw*cp;
    float tail_depth=tx*syaw*cp+ty*sp+tz*cyaw*cp;
    if(depth<=.05f && tail_depth<=.05f)continue;
    if(depth<=.05f)depth=tail_depth;
    float sx=width*.5f+focal*(cx*cyaw-cz*syaw)/fmaxf(depth,.05f);
    float sy=height*.5f-focal*(-cx*sp*syaw+cy*cp-cz*sp*cyaw)/fmaxf(depth,.05f);
    float ex=width*.5f+focal*(tx*cyaw-tz*syaw)/fmaxf(tail_depth,.05f);
    float ey=height*.5f-focal*(-tx*sp*syaw+ty*cp-tz*sp*cyaw)/fmaxf(tail_depth,.05f);
    float pad=fmaxf(1.5f,focal*.035f/fmaxf(depth,.05f));
    float minx=fminf(sx,ex)-pad,maxx=fmaxf(sx,ex)+pad,miny=fminf(sy,ey)-pad,maxy=fmaxf(sy,ey)+pad;
    if(maxx<0||maxy<0||minx>=width||miny>=height)continue;
    projectile_rect[i][0]=(int)clamp(floorf(minx),0,width);projectile_rect[i][1]=(int)clamp(floorf(miny),0,height);
    projectile_rect[i][2]=(int)clamp(ceilf(maxx),0,width);projectile_rect[i][3]=(int)clamp(ceilf(maxy),0,height);
    projectile_depth[i]=depth;projectile_ax[i]=sx;projectile_ay[i]=sy;projectile_bx[i]=ex;projectile_by[i]=ey;projectile_mask|=1u<<i;
  }
  /* Six registered furnaces, collected once; zero reserves inactive slots. */
  uint32_t burning_furnaces[MC_FURNACES]={0};
  for(unsigned i=0;i<MC_FURNACES;i++) {
    const MC_Furnace *f=&w->furnaces[i];
    if(f->x && f->burn>0 && MC_Get(w,f->x-1,f->y-1,f->z-1)==MC_FURNACE)
      burning_furnaces[i]=((uint32_t)(f->y-1)*MC_MAP_Z+f->z-1)*MC_MAP_X+f->x;
  }
  /* Bound local light work to eight sources, including burning furnaces. */
  static struct {const MC_World *world;const uint8_t *blocks;uint32_t serial,revision,furnaces[MC_FURNACES];int x,y,z,valid,n,lights[8][3];} lc;
  int lx0=(int)w->x,ly0=(int)w->y,lz0=(int)w->z;
  if(!lc.valid || lc.world!=w || lc.blocks!=w->blocks || lc.serial!=w->scene_serial ||
     lc.revision!=w->map_revision || lc.x!=lx0 || lc.y!=ly0 || lc.z!=lz0 || memcmp(lc.furnaces,burning_furnaces,sizeof(burning_furnaces))) {
    lc.n=0;
    for(int y=ly0-4;y<=ly0+4 && lc.n<8;y++)for(int z=lz0-6;z<=lz0+6 && lc.n<8;z++)for(int x=lx0-6;x<=lx0+6 && lc.n<8;x++) {
      uint8_t block=MC_Get(w,x,y,z);
      if(block==MC_TORCH || block==MC_LAMP_ON){lc.lights[lc.n][0]=x;lc.lights[lc.n][1]=y;lc.lights[lc.n++][2]=z;}
    }
    for(unsigned i=0;i<MC_FURNACES && lc.n<8;i++)if(burning_furnaces[i]) {
      const MC_Furnace *f=&w->furnaces[i];int x=f->x-1,y=f->y-1,z=f->z-1;
      if(abs(x-lx0)<=6 && abs(y-ly0)<=4 && abs(z-lz0)<=6) {
        lc.lights[lc.n][0]=x;lc.lights[lc.n][1]=y;lc.lights[lc.n++][2]=z;
      }
    }
    memcpy(lc.furnaces,burning_furnaces,sizeof(burning_furnaces));
    lc.world=w;lc.blocks=w->blocks;lc.serial=w->scene_serial;lc.revision=w->map_revision;lc.x=lx0;lc.y=ly0;lc.z=lz0;lc.valid=1;
  }
  const int (*lights)[3]=lc.lights;int nl=lc.n;
  /* Fluid textures have only 256 possible texels at a fixed world time.
     Lava is filled per texel so a distant pool does not evaluate a full tile. */
  int8_t water_noise[256],lava_noise[256];int water_ready=0;
  uint32_t lava_valid[8]={0};
  uint32_t last_furnace=UINT32_MAX;int last_furnace_lit=0;
  /* Lighting depends on block coordinates, not the individual face pixel.
     A frame-local direct-mapped cache remains correct after any world edit. */
  uint32_t light_key[256];float light_value[256];
  for(unsigned i=0;i<256;i++)light_key[i]=UINT32_MAX;
  for(unsigned y=0;y<height;++y) {
    unsigned row_drop=0,row_projectile=0,row_mob=0;
    for(unsigned mask=drop_mask;mask;mask&=mask-1) {
      unsigned i=(unsigned)__builtin_ctz(mask);
      if((int)y>=drop_rect[i][1] && (int)y<=drop_rect[i][3])row_drop|=1u<<i;
    }
    for(unsigned mask=projectile_mask;mask;mask&=mask-1) {
      unsigned i=(unsigned)__builtin_ctz(mask);
      if((int)y>=projectile_rect[i][1] && (int)y<=projectile_rect[i][3])row_projectile|=1u<<i;
    }
    for(unsigned mask=mob_mask;mask;mask&=mask-1) {
      unsigned i=(unsigned)__builtin_ctz(mask);
      if((int)y>=mob_rect[i][1] && (int)y<=mob_rect[i][3])row_mob|=1u<<i;
    }
    for(unsigned x=0;x<width;++x) {
    float u=(x+.5f-width*.5f)/focal,v=(height*.5f-y-.5f)/focal;
    float dx=syaw*cp+u*cyaw-v*sp*syaw,dy=sp+v*cp,dz=cyaw*cp-u*syaw-v*sp*cyaw;
    float horizon=clamp(1-fabsf(dy)*1.5f,0,1);
    int skyR=(int)((73+64*horizon)*daylight),skyG=(int)((151+48*horizon)*daylight),skyB=(int)((218+20*horizon)*(.3f+.7f*daylight));
    unsigned r=skyR,g=skyG,b=skyB;
    Hit h;int world_hit=trace(w,dx,dy,dz,40,&h);
    /* Opaque terrain replaces the sky. Keep the cheap fog sky color, but
       compute celestial discs/clouds/stars only for rays that reach sky. */
    if(!world_hit) {
    float norm=1/sqrtf(dx*dx+dy*dy+dz*dz);
    float dot=(dx*sunx+dy*suny+dz*sunz)*norm*sun_norm;
    if(dot>.994f && sun>0) {r=255;g=249;b=199;}
    if(dot<-.996f && sun<0) {r=210;g=222;b=239;}
    if(dy>.25f && daylight>.3f) {
      int cx=(int)floorf((w->x+dx/dy*65+w->time*.12f)/20);
      int cz=(int)floorf((w->z+dz/dy*65)/16);
      if((hash(cx,cz,123)&15)<4) {r=(unsigned)(218*daylight);g=(unsigned)(228*daylight);b=(unsigned)(238*daylight);}
    } else if(dy>0 && daylight<.3f && (hash((unsigned)(int)(dx*norm*600),(unsigned)(int)(dz*norm*600),17)&2047)<5) {r=180;g=194;b=220;}
    }
    if(world_hit) {
      float px=w->x+dx*h.distance,py=w->y+1.62f+dy*h.distance,pz=w->z+dz*h.distance;
      float a=h.nx?pz:px,c=h.ny?pz:py;
      unsigned tu=(unsigned)(int)floorf(a*16)&15,tv=(unsigned)(int)floorf(c*16)&15;
      if(shaped(h.block)) {
        float lo[3],hi[3];bounds(h.block,lo,hi);
        int ua=h.nx?2:0,va=h.ny?2:1;
        float pos[3]={px-h.x,py-h.y,pz-h.z};
        if(h.block==MC_SAPLING)ua=0;
        tu=(unsigned)clamp(floorf((pos[ua]-lo[ua])*16/(hi[ua]-lo[ua])),0,15);
        tv=(unsigned)clamp(floorf((pos[va]-lo[va])*16/(hi[va]-lo[va])),0,15);
      }
      uint8_t material=h.block;
      if(MC_IsDoor(h.block)) {
        material=MC_DoorOpen(h.block)?MC_DOOR_OPEN:MC_DOOR;
        if(!h.ny) {
          float x=px-h.x,z=pz-h.z,u;
          switch(MC_DoorFacing(h.block)) {
            case 1:u=MC_DoorOpen(h.block)?x:1-z;break;
            case 2:u=MC_DoorOpen(h.block)?1-z:1-x;break;
            case 3:u=MC_DoorOpen(h.block)?1-x:z;break;
            default:u=MC_DoorOpen(h.block)?z:x;break;
          }
          tu=(unsigned)clamp(floorf(u*16),0,15);
        }
      }
      int n=(int)(hash(tu,tv,material)&31)-15,furnace_lit=0;
      r=120;g=123;b=128;
      switch(material) {
        case MC_GRASS: if(h.ny>0 || tv>=13+(tu%3==0)) {r=83;g=153;b=43;} else {r=131;g=91;b=58;}break;
        case MC_DIRT:r=131;g=91;b=58;break;
        case MC_SAND:r=218;g=204;b=145;break;
        case MC_WOOD:r=133;g=97;b=54;
          if(h.ny) {int rr=abs((int)tu-8)+abs((int)tv-8);n+=(rr%3==0?-30:14);}
          else n+=((tu+(tv/5))%4==0?-25:0);
          break;
        case MC_BIRCH_WOOD:r=190;g=176;b=126;
          if(h.ny){int rr=abs((int)tu-8)+abs((int)tv-8);n+=(rr%3==0?-28:10);}
          else if((tu+tv*3)%7==0)n=-35;
          break;
        case MC_DARK_WOOD:r=82;g=53;b=31;
          if(h.ny){int rr=abs((int)tu-8)+abs((int)tv-8);n+=(rr%3==0?-24:12);}
          else if((tu+tv*3)%6==0)n=-28;
          break;
        case MC_LEAVES:r=56;g=119;b=40;n*=2;break;
        case MC_BEDROCK:r=53;g=56;b=62;break;
        case MC_WATER:
          r=40;g=113;b=186;
          if(!water_ready) {
            for(unsigned wy=0;wy<16;wy++)for(unsigned wx=0;wx<16;wx++)
              water_noise[wy*16+wx]=(int8_t)(sinf(wx*.8f+wy*.6f+w->time*2)*11);
            water_ready=1;
          }
          n=water_noise[tv*16+tu];break;
        case MC_LAVA:
          r=224;g=60;b=18;
          {
            unsigned texel=tv*16+tu,word=texel>>5;
            uint32_t bit=1u<<(texel&31);
            if(!(lava_valid[word]&bit)) {
              lava_noise[texel]=(int8_t)(sinf((float)tu*.9f+(float)tv*.55f+w->time*3.0f)*18.0f);
              lava_valid[word]|=bit;
            }
            n=lava_noise[texel];
          }
          break;
        case MC_COBBLE:if((tu+((tv/5)&1)*4)%7==0 || tv%5==0)n=-45;break;
        case MC_PLANKS:r=179;g=137;b=79;if(tv%4==0 || (tu+(tv/4)*7)%16==0)n=-32;break;
        case MC_BIRCH_PLANKS:r=204;g=181;b=126;if(tv%4==0 || (tu+(tv/4)*7)%16==0)n=-30;break;
        case MC_DARK_PLANKS:r=105;g=68;b=39;if(tv%4==0 || (tu+(tv/4)*7)%16==0)n=-28;break;
        case MC_TABLE:r=147;g=100;b=51;if(h.ny && (tu%5==0 || tv%5==0))n=-50;else if(!h.ny && tu>4 && tu<11)n=-35;break;
        case MC_FURNACE:
          {
            uint32_t cell=(uint32_t)((h.y*MC_MAP_Z+h.z)*MC_MAP_X+h.x+1);
            if(last_furnace!=cell) {
              last_furnace=cell;last_furnace_lit=0;
              for(unsigned i=0;i<MC_FURNACES;i++)if(burning_furnaces[i]==cell){last_furnace_lit=1;break;}
            }
            furnace_lit=last_furnace_lit;
          }
          r=102;g=107;b=111;
          if(!h.ny && tu>3 && tu<12 && tv>3 && tv<10){r=44;g=31;b=24;if(furnace_lit && tv<6){r=247;g=132;b=24;}}
          break;
        case MC_COAL_ORE:if((hash(tu/2,tv/2,13)&15)<5){r=35;g=38;b=42;}break;
        case MC_IRON_ORE:if((hash(tu/2,tv/2,19)&15)<5){r=192;g=146;b=117;}break;
        case MC_GLASS:r=139;g=206;b=218;n=(tu<2 || tv<2 || tu==tv)?38:-5;break;
        case MC_BRICK:r=161;g=74;b=55;if(tv%4==0 || (tu+((tv/4)&1)*4)%8==0){r=162;g=155;b=139;}break;
        case MC_TORCH:r=123;g=84;b=39;if(py-h.y>=9/16.f){r=255;g=187+(tu&1)*30;b=40;}break;
        case MC_DUST:
        case MC_DUST_ON:
          r=h.block==MC_DUST_ON?235:105;g=h.block==MC_DUST_ON?24:20;b=h.block==MC_DUST_ON?30:25;break;
        case MC_LAMP:
          r=93;g=87;b=75;break;
        case MC_LAMP_ON:
          r=255;g=204;b=54;break;
        case MC_LEVER:
        case MC_LEVER_ON:
          if(py-h.y<.126f){r=126;g=132;b=138;}
          else {r=118;g=82;b=45;if(h.block==MC_LEVER_ON){r=188;g=133;b=60;}}break;
        case MC_BUTTON:
        case MC_BUTTON_ON:
          r=h.block==MC_BUTTON_ON?214:152;g=h.block==MC_BUTTON_ON?90:96;b=55;break;
        case MC_CHEST:r=157;g=103;b=44;if(tv<2 || tv>13 || tu<2 || tu>13)n=-42;
          if(!h.ny && tu>6 && tu<10 && tv>5 && tv<10){r=232;g=191;b=74;}break;
        case MC_BED:r=183;g=43;b=40;if(pz-h.z>.75f){r=230;g=221;b=198;}if(!h.ny && tv<5){r=119;g=79;b=40;}break;
        case MC_SAPLING:r=83;g=180;b=49;if(py-h.y<.3f){r=117;g=82;b=43;}break;
        case MC_FARMLAND:r=102;g=71;b=43;if(h.ny>0){r=116;g=80;b=47;}break;
        case MC_WHEAT_CROP:r=57;g=151;b=52;n*=2;break;
        case MC_WHEAT_MATURE:r=197;g=161;b=48;n*=2;if(h.ny<0){r=101;g=83;b=31;}break;
        case MC_DOOR:
        case MC_DOOR_OPEN:
        case MC_DOOR_TOP:
        case MC_DOOR_TOP_OPEN:
          r=153;g=101;b=48;
          if((tu%4)==0 || tv==0 || tv==15){r=105;g=65;b=32;}
          if(!MC_DoorUpper(h.block)&&!h.ny&&tv>6&&tv<10&&tu>10){r=220;g=177;b=67;}
          break;
        case MC_SLAB:
        case MC_SLAB_TOP:
        case MC_DOUBLE_SLAB:
          r=139;g=143;b=149;
          if(h.ny>0){r=171;g=175;b=181;}
          if((tu+((tv/5)&1)*4)%7==0 || tv%5==0){r=94;g=99;b=106;}
          break;
        case MC_FENCE:
          r=151;g=103;b=55;
          if((tu+tv)%5==0){r=108;g=70;b=36;}
          if(h.ny>0){r=181;g=131;b=70;}
          break;
        case MC_STAIR:
        case MC_STAIR_EAST:
        case MC_STAIR_SOUTH:
        case MC_STAIR_WEST:
          r=139;g=143;b=149;
          if(h.ny>0){r=171;g=175;b=181;}
          else if(h.ny<0){r=82;g=87;b=94;}
          else if((tu+tv)%6==0){r=101;g=106;b=113;}
          break;
        default:break;
      }
      float ambient=.22f+.78f*daylight;
      /* Underground surfaces stay dark; nearby torches provide warm local light. */
      if(h.y<14)ambient*=.62f;
      float glow=0;
      if(nl) {
        uint32_t key=(uint32_t)((h.y*MC_MAP_Z+h.z)*MC_MAP_X+h.x);
        unsigned slot=(key*2654435761u)>>24;
        if(light_key[slot]==key)glow=light_value[slot];
        else {
          for(int l=0;l<nl;l++) {
            float lx=h.x-lights[l][0],ly=h.y-lights[l][1],lz=h.z-lights[l][2];
            glow=fmaxf(glow,clamp(1-(lx*lx+ly*ly+lz*lz)/42,0,1));
          }
          light_key[slot]=key;light_value[slot]=glow;
        }
      }
      ambient=fmaxf(ambient,glow*.95f);
      if(h.block==MC_TORCH || h.block==MC_LAMP_ON)ambient=1;
      float shade=(h.ny>0?1.0f:h.ny<0?.50f:h.nx?.80f:.68f)*ambient;
      float fog=clamp((h.distance-18)/22,0,1);fog*=fog;
      r=(unsigned)clamp((r+n)*shade*(1-fog)+skyR*fog,0,255);
      g=(unsigned)clamp((g+n)*shade*(1-fog)+skyG*fog,0,255);
      b=(unsigned)clamp((b+n)*shade*(1-fog)+skyB*fog,0,255);
      if(tu==0 || tv==0){r=r*9/10;g=g*9/10;b=b*9/10;}
      if(w->target_valid && h.x==w->target.x && h.z==w->target.z &&
         (h.y==w->target.y || (MC_IsDoor(h.block)&&MC_IsDoor(w->target.block)&&
          h.y-MC_DoorUpper(h.block)==w->target.y-MC_DoorUpper(w->target.block)))) {
        int edge=tu==0 || tu==15 || tv==0 || tv==15;
        if(MC_IsDoor(h.block))edge=tu==0 || tu==15 || (MC_DoorUpper(h.block)?tv==15:tv==0);
        if(edge){r=246;g=237;b=180;}
        if(w->mining>0 && ((tu+tv*3)%((unsigned)(12-w->mining*8))==0)){r/=3;g/=3;b/=3;}
      }
    }
    for(unsigned mask=row_drop;mask;mask&=mask-1) {
      unsigned i=(unsigned)__builtin_ctz(mask);
      if((int)x>=drop_rect[i][0] && (int)x<=drop_rect[i][2] &&
        (!world_hit || drop_depth[i]<h.distance)) {
      uint16_t c=MC_ItemColor(w->drops[i].item);r=((c>>11)&31)*255/31;g=((c>>5)&63)*255/63;b=(c&31)*255/31;
      int edge=(int)x==drop_rect[i][0] || (int)y==drop_rect[i][1] || (int)x==drop_rect[i][2] || (int)y==drop_rect[i][3];
      if(edge){r=r*2/3;g=g*2/3;b=b*2/3;}
      break;
      }
    }
    for(unsigned mask=row_projectile;mask;mask&=mask-1) {
      unsigned i=(unsigned)__builtin_ctz(mask);
      if((int)x>=projectile_rect[i][0] && (int)x<=projectile_rect[i][2] &&
        (!world_hit || projectile_depth[i]<h.distance)) {
      float ax=projectile_ax[i],ay=projectile_ay[i],bx=projectile_bx[i],by=projectile_by[i];
      float vx=bx-ax,vy=by-ay,den=vx*vx+vy*vy,t=den>1e-6f?((((float)x+.5f-ax)*vx+((float)y+.5f-ay)*vy)/den):0;
      if(t<0)t=0;else if(t>1)t=1;
      float qx=ax+vx*t,qy=ay+vy*t,ddx=(float)x+.5f-qx,ddy=(float)y+.5f-qy;
      if(ddx*ddx+ddy*ddy<2.1f) {
        int head=t<.36f;
        r=head?245:216;g=head?242:204;b=head?190:137;
        break;
      }
    }
    }
    unsigned candidates=0;
    for(unsigned mask=row_mob;mask;mask&=mask-1) {
      unsigned i=(unsigned)__builtin_ctz(mask);
      if((int)x>=mob_rect[i][0] && (int)x<=mob_rect[i][2])candidates|=1u<<i;
    }
    if(candidates) {
      int id,axis,sign;float distance;
      if(MC_MobRay(w,dx,dy,dz,world_hit?h.distance:40,candidates,&id,&distance,&axis,&sign)>=0) {
        const MC_Mob *m=&w->mobs[id];float py=w->y+1.62f+dy*distance-m->y;
        float px=w->x+dx*distance-m->x,pz=w->z+dz*distance-m->z;
        if(m->type) {
          /* Passive animal: a squat brown body, pink muzzle and four dark
             legs.  Keep the silhouette deliberately simple for the 160x96
             ray view, but use a head/eye highlight so it reads as an animal. */
          float face=axis==2?px:pz;
          if(py>1.03f){r=206;g=137;b=105;
            if(axis!=1 && py>1.08f && py<1.18f && fabsf(face)>.07f && fabsf(face)<.22f){r=245;g=224;b=184;}
            if(axis!=1 && py>1.00f && py<1.08f && fabsf(face)<.12f){r=55;g=37;b=31;}
          } else if(py>.46f){r=166;g=103;b=57;
            if(axis!=1 && ((int)(fabsf(face)*80)&7)==0){r=193;g=126;b=68;}
          } else {r=104;g=68;b=46;
            if(axis!=1 && fabsf(face)<.035f){r=65;g=48;b=39;}
          }
        } else {
          r=48;g=114;b=101;
          if(py>1.32f){r=104;g=145;b=64;
            float face=axis==2?px:pz;
            if(axis!=1 && py>1.55f && py<1.65f && fabsf(face)>.07f && fabsf(face)<.21f){r=238;g=214;b=120;}
            if(axis!=1 && py>1.38f && py<1.46f && fabsf(face)<.13f){r=39;g=57;b=32;}
          }else if(py<.62f){r=63;g=61;b=88;if(fabsf(axis==2?px:pz)<.035f){r=30;g=33;b=45;}}
        }
        if(m->hurt>0){r=226;g=77;b=62;}
        float shade=(axis==1?1:axis==0?.8f:.68f)*(.55f+.45f*daylight);
        float fog=clamp((distance-18)/22,0,1);fog*=fog;
        r=(unsigned)clamp(r*shade*(1-fog)+skyR*fog,0,255);g=(unsigned)clamp(g*shade*(1-fog)+skyG*fog,0,255);b=(unsigned)clamp(b*shade*(1-fog)+skyB*fog,0,255);
        (void)sign;
      }
    }
    if(underwater){r=r*2/3;g=g*3/4;b=b/2+95;}
    pixels[y*width+x]=rgb(r,g,b);
    }
  }
}
uint32_t MC_Crc32(const void *data,unsigned length)
{
  /* Same IEEE CRC-32 as old saves, two nibble lookups instead of eight
     polynomial branches per byte.  Larger maps need no extra work buffer. */
  static const uint32_t table[16]={0,0x1db71064u,0x3b6e20c8u,0x26d930acu,
    0x76dc4190u,0x6b6b51f4u,0x4db26158u,0x5005713cu,0xedb88320u,0xf00f9344u,
    0xd6d6a3e8u,0xcb61b38cu,0x9b64c2b0u,0x86d3d2d4u,0xa00ae278u,0xbdbdf21cu};
  uint32_t crc=~0u;const uint8_t *p=data;
  while(length--){crc^=*p++;crc=(crc>>4)^table[crc&15];crc=(crc>>4)^table[crc&15];}
  return ~crc;
}
