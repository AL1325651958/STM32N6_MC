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

uint8_t MC_Get(const MC_World *w, int x, int y, int z)
{
  if (x < 0 || x >= MC_MAP_X || z < 0 || z >= MC_MAP_Z || y < 0) return MC_BEDROCK;
  return y >= MC_MAP_Y ? MC_AIR : w->blocks[index_of(x,y,z)];
}
void MC_Set(MC_World *w, int x, int y, int z, uint8_t b)
{ if (inside(x,y,z) && y > 0 && (b==MC_AIR || MC_IsBlock(b))) w->blocks[index_of(x,y,z)] = b; }
int MC_IsSolid(uint8_t block) { return block != MC_AIR && block != MC_WATER && block != MC_TORCH && block != MC_SAPLING && block != MC_DOOR_OPEN; }

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

static unsigned parts_for(uint8_t b, const MC_Part **parts)
{
  if(b==MC_TORCH) { *parts=torch_parts; return sizeof(torch_parts)/sizeof(torch_parts[0]); }
  *parts=0; return 0;
}

/* Cell-local bounds, shared by ray selection, rendering and solid collision. */
static void bounds(uint8_t b,float lo[3],float hi[3])
{
  lo[0]=lo[1]=lo[2]=0;hi[0]=hi[1]=hi[2]=1;
  if(b==MC_TORCH) {
    const MC_Part *parts; unsigned count=parts_for(b,&parts);
    lo[0]=lo[1]=lo[2]=1; hi[0]=hi[1]=hi[2]=0;
    for(unsigned i=0;i<count;i++) for(unsigned a=0;a<3;a++) {
      lo[a]=fminf(lo[a],parts[i].lo[a]); hi[a]=fmaxf(hi[a],parts[i].hi[a]);
    }
  }
  else if(b==MC_CHEST){lo[0]=lo[2]=1/16.f;hi[0]=hi[2]=15/16.f;hi[1]=14/16.f;}
  else if(b==MC_BED)hi[1]=9/16.f;
  else if(b==MC_SAPLING){lo[0]=lo[2]=2/16.f;hi[0]=hi[2]=14/16.f;hi[1]=13/16.f;}
  else if(b==MC_DOOR){lo[2]=6/16.f;hi[2]=10/16.f;}
  else if(b==MC_DOOR_OPEN){lo[0]=6/16.f;hi[0]=10/16.f;}
  else if(b==MC_SLAB){hi[1]=.5f;}
  else if(b==MC_SLAB_TOP){lo[1]=.5f;}
}
static int body_intersects(uint8_t b,int bx,int by,int bz,float x,float y,float z)
{
  float lo[3],hi[3];bounds(b,lo,hi);
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
      if(MC_IsSolid(b) && ((b!=MC_BED && b!=MC_CHEST) || body_intersects(b,xx,yy,zz,x,y,z)))return 1;
    }
  return 0;
}
int MC_BodyClear(const MC_World *w,float x,float y,float z)
{return !overlaps(w,x,y,z);}
static int has_floor(const MC_World *w,float x,float z)
{
  const float inset=.24f,y=w->y-.08f;
  for(int iz=-1;iz<=1;iz+=2)for(int ix=-1;ix<=1;ix+=2)
    {
      float px=x+ix*inset,pz=z+iz*inset;int bx=(int)floorf(px),by=(int)floorf(y),bz=(int)floorf(pz);
      uint8_t b=MC_Get(w,bx,by,bz);
      float lo[3],hi[3];bounds(b,lo,hi);
      if(!MC_IsSolid(b))continue;
      if(px>=bx+lo[0] && px<=bx+hi[0] && pz>=bz+lo[2] && pz<=bz+hi[2] && y>=by+lo[1] && y<by+hi[1])return 1;
    }
  return 0;
}
void MC_Respawn(MC_World *w)
{
  w->vy=0;w->on_ground=0;w->fall_speed=0;
  memset(w->mobs,0,sizeof(w->mobs));w->mob_target=255;w->spawn_clock=0;
  if(w->spawn[0] && MC_Get(w,w->spawn[0]-1,w->spawn[1]-1,w->spawn[2]-1)==MC_BED) {
    int bx=w->spawn[0]-1,by=w->spawn[1]-1,bz=w->spawn[2]-1;
    for(int dz=-1;dz<=1;dz++)for(int dx=-1;dx<=1;dx++) {
      float lo[3],hi[3];bounds(MC_Get(w,bx+dx,by,bz+dz),lo,hi);
      float x=bx+dx+.5f,z=bz+dz+.5f,y=by+hi[1]+.01f;
      if(MC_IsSolid(MC_Get(w,bx+dx,by,bz+dz)) && !overlaps(w,x,y,z)) {
        w->x=x;w->y=y;w->z=z;return;
      }
    }
  }
  for(int r=0;r<64;r++)for(int dz=-r;dz<=r;dz++)for(int dx=-r;dx<=r;dx++) {
    if(r && abs(dx)!=r && abs(dz)!=r)continue;
    int x=64+dx,z=64+dz;if(x<1||x>=127||z<1||z>=127)continue;
    for(int y=MC_MAP_Y-3;y>=1;y--) if(MC_IsSolid(MC_Get(w,x,y,z)) && MC_Get(w,x,y+1,z)==MC_AIR && MC_Get(w,x,y+2,z)==MC_AIR) {
      float lo[3],hi[3];bounds(MC_Get(w,x,y,z),lo,hi);
      w->x=x+.5f;w->y=y+hi[1]+.01f;w->z=z+.5f;return;
    }
  }
  w->x=64.5f;w->z=64.5f;w->y=MC_MAP_Y+1;
}
void MC_Generate(MC_World *w, uint32_t seed)
{
  /* Clear old world's respawn/container state before selecting a new spawn. */
  MC_NewPlayer(w,0);
  memset(w->blocks,0,MC_MAP_BYTES); w->seed=seed;
  for(int z=0; z<MC_MAP_Z; ++z) for(int x=0; x<MC_MAP_X; ++x) {
    int h=height(x,z,seed); w->blocks[index_of(x,0,z)]=MC_BEDROCK;
    for(int y=1; y<=h; ++y) MC_Set(w,x,y,z,y==h?(h<17?MC_SAND:MC_GRASS):y>h-4?MC_DIRT:MC_STONE);
    for(int y=h+1; y<=16; ++y) MC_Set(w,x,y,z,MC_WATER);
  }
  /* Trees are a second pass, so neighboring columns cannot erase their crowns. */
  for(int z=4; z<MC_MAP_Z-4; z+=6) for(int x=4; x<MC_MAP_X-4; x+=6) {
    if((hash(x,z,seed)&15)>4 || (abs(x-64)<4 && abs(z-64)<4)) continue;
    int h=height(x,z,seed); if(h<18) continue;
    for(int y=h+1; y<=h+4; ++y) MC_Set(w,x,y,z,MC_WOOD);
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
  w->yaw=.7f; w->pitch=-.12f; MC_Respawn(w);
  /* Start on open grass, near wood, with an unobstructed horizon. */
  int best=-100000;
  for(int z=24;z<104;z+=2)for(int x=24;x<104;x+=2) {
    int y=height(x,z,seed);
    if(MC_Get(w,x,y,z)!=MC_GRASS || MC_Get(w,x,y+1,z)!=MC_AIR || MC_Get(w,x,y+2,z)!=MC_AIR)continue;
    int score=y*8-abs(x-64)-abs(z-64);
    if(score>best){best=score;w->x=x+.5f;w->y=y+1.01f;w->z=z+.5f;}
  }
  MC_NewPlayer(w,0);
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
    if(!overlaps(w,w->x,w->y,nz) && (!guard || has_floor(w,w->x,nz))) w->z=nz;
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
{return b==MC_TORCH || b==MC_SAPLING || b==MC_BED || b==MC_CHEST || b==MC_DOOR || b==MC_DOOR_OPEN || b==MC_SLAB || b==MC_SLAB_TOP;}

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
    if(shaped(b)) {
      const float p[3]={px-x,py-y,pz-z},d[3]={dx,dy,dz};float distance;
      int hx,hy,hz;
      if(shape_hit(b,p,d,t,fminf(reach,fminf(tx,fminf(ty,tz))),&distance,&hx,&hy,&hz)) {
        *hit=(Hit){x,y,z,hx,hy,hz,distance,b};return 1;
      }
      b=MC_AIR;
    }
    /* Render cutouts without changing solid collision / touch selection. */
    if(reach>6 && b!=MC_AIR) {
      float hx=px+dx*t,hy=py+dy*t,hz=pz+dz*t;
      unsigned u=(unsigned)(int)floorf((nx?hz:hx)*16)&15;
      unsigned v=(unsigned)(int)floorf((ny?hz:hy)*16)&15;
      if(b==MC_GLASS && u>0 && u<15 && v>0 && v<15 && u!=v && u+1!=v)b=MC_AIR;
      else if(b==MC_LEAVES && (hash(u,v,7)&15)<3)b=MC_AIR;
      else if(b==MC_WATER && MC_Get(w,(int)px,(int)py,(int)pz)==MC_WATER)b=MC_AIR;
    }
    if(b!=MC_AIR) { *hit=(Hit){x,y,z,nx,ny,nz,t,b}; return 1; }
    if(tx<=ty && tx<=tz) { x+=sx; t=tx; tx+=ix; nx=-sx; ny=nz=0; }
    else if(ty<=tz) { y+=sy; t=ty; ty+=iy; ny=-sy; nx=nz=0; }
    else { z+=sz; t=tz; tz+=iz; nz=-sz; nx=ny=0; }
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
int MC_Edit(MC_World *w, int place, uint8_t selected)
{
  int x,y,z,nx,ny,nz;
  if(!MC_Raycast(w,&x,&y,&z,&nx,&ny,&nz,5)) return 0;
  if(place) {
    if(!MC_IsBlock(selected) || selected==MC_BEDROCK || selected==MC_WATER || selected==MC_DOOR_OPEN || !(nx||ny||nz)) return 0;
    x+=nx; y+=ny; z+=nz;
    uint8_t old=MC_Get(w,x,y,z);
    if(!inside(x,y,z) || y==0 || (old!=MC_AIR && old!=MC_WATER)) return 0;
    if(selected==MC_DOOR && !MC_IsSolid(MC_Get(w,x,y-1,z))) return 0;
    if(MC_IsSolid(selected) && body_intersects(selected,x,y,z,w->x,w->y,w->z)) return 0;
    MC_Set(w,x,y,z,selected);
  } else {
    if(!inside(x,y,z) || y==0 || MC_Get(w,x,y,z)==MC_BEDROCK) return 0;
    MC_Set(w,x,y,z,MC_AIR);
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
    float minx=width,miny=height,maxx=0,maxy=0;int behind=0,front=0;
    for(int corner=0;corner<8;corner++) {
      float px=m->x+((corner&1)?.30f:-.30f)-w->x;
      float py=m->y+((corner&2)?1.8f:0)-w->y-1.62f;
      float pz=m->z+((corner&4)?.30f:-.30f)-w->z;
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
  /* Bound dynamic light work: collect at most eight nearby torches once per frame. */
  int lights[8][3],nl=0;
  for(int y=(int)w->y-4;y<=(int)w->y+4 && nl<8;y++)
    for(int z=(int)w->z-6;z<=(int)w->z+6 && nl<8;z++)
      for(int x=(int)w->x-6;x<=(int)w->x+6 && nl<8;x++)
        if(MC_Get(w,x,y,z)==MC_TORCH) {lights[nl][0]=x;lights[nl][1]=y;lights[nl++][2]=z;}
  for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
    float u=(x+.5f-width*.5f)/focal,v=(height*.5f-y-.5f)/focal;
    float dx=syaw*cp+u*cyaw-v*sp*syaw,dy=sp+v*cp,dz=cyaw*cp-u*syaw-v*sp*cyaw;
    float horizon=clamp(1-fabsf(dy)*1.5f,0,1);
    int skyR=(int)((73+64*horizon)*daylight),skyG=(int)((151+48*horizon)*daylight),skyB=(int)((218+20*horizon)*(.3f+.7f*daylight));
    unsigned r=skyR,g=skyG,b=skyB;
    float norm=1/sqrtf(dx*dx+dy*dy+dz*dz);
    float dot=(dx*sunx+dy*suny+dz*sunz)*norm*sun_norm;
    if(dot>.994f && sun>0) {r=255;g=249;b=199;}
    if(dot<-.996f && sun<0) {r=210;g=222;b=239;}
    if(dy>.25f && daylight>.3f) {
      int cx=(int)floorf((w->x+dx/dy*65+w->time*.12f)/20);
      int cz=(int)floorf((w->z+dz/dy*65)/16);
      if((hash(cx,cz,123)&15)<4) {r=(unsigned)(218*daylight);g=(unsigned)(228*daylight);b=(unsigned)(238*daylight);}
    } else if(dy>0 && daylight<.3f && (hash((unsigned)(int)(dx*norm*600),(unsigned)(int)(dz*norm*600),17)&2047)<5) {r=180;g=194;b=220;}
    Hit h;int world_hit=trace(w,dx,dy,dz,40,&h);
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
      int n=(int)(hash(tu,tv,h.block)&31)-15;
      r=120;g=123;b=128;
      switch(h.block) {
        case MC_GRASS: if(h.ny>0 || tv>=13+(tu%3==0)) {r=83;g=153;b=43;} else {r=131;g=91;b=58;}break;
        case MC_DIRT:r=131;g=91;b=58;break;
        case MC_SAND:r=218;g=204;b=145;break;
        case MC_WOOD:r=133;g=97;b=54;
          if(h.ny) {int rr=abs((int)tu-8)+abs((int)tv-8);n+=(rr%3==0?-30:14);}
          else n+=((tu+(tv/5))%4==0?-25:0);
          break;
        case MC_LEAVES:r=56;g=119;b=40;n*=2;break;
        case MC_BEDROCK:r=53;g=56;b=62;break;
        case MC_WATER:r=40;g=113;b=186;n=(int)(sinf(tu*.8f+tv*.6f+w->time*2)*11);break;
        case MC_COBBLE:if((tu+((tv/5)&1)*4)%7==0 || tv%5==0)n=-45;break;
        case MC_PLANKS:r=179;g=137;b=79;if(tv%4==0 || (tu+(tv/4)*7)%16==0)n=-32;break;
        case MC_TABLE:r=147;g=100;b=51;if(h.ny && (tu%5==0 || tv%5==0))n=-50;else if(!h.ny && tu>4 && tu<11)n=-35;break;
        case MC_FURNACE:r=102;g=107;b=111;if(!h.ny && tu>3 && tu<12 && tv>3 && tv<10){r=44;g=31;b=24;if(tv<6){r=247;g=132;b=24;}}break;
        case MC_COAL_ORE:if((hash(tu/2,tv/2,13)&15)<5){r=35;g=38;b=42;}break;
        case MC_IRON_ORE:if((hash(tu/2,tv/2,19)&15)<5){r=192;g=146;b=117;}break;
        case MC_GLASS:r=139;g=206;b=218;n=(tu<2 || tv<2 || tu==tv)?38:-5;break;
        case MC_BRICK:r=161;g=74;b=55;if(tv%4==0 || (tu+((tv/4)&1)*4)%8==0){r=162;g=155;b=139;}break;
        case MC_TORCH:r=123;g=84;b=39;if(py-h.y>=9/16.f){r=255;g=187+(tu&1)*30;b=40;}break;
        case MC_CHEST:r=157;g=103;b=44;if(tv<2 || tv>13 || tu<2 || tu>13)n=-42;
          if(!h.ny && tu>6 && tu<10 && tv>5 && tv<10){r=232;g=191;b=74;}break;
        case MC_BED:r=183;g=43;b=40;if(pz-h.z>.75f){r=230;g=221;b=198;}if(!h.ny && tv<5){r=119;g=79;b=40;}break;
        case MC_SAPLING:r=83;g=180;b=49;if(py-h.y<.3f){r=117;g=82;b=43;}break;
        case MC_DOOR:
        case MC_DOOR_OPEN:
          r=153;g=101;b=48;
          if((tu%4)==0 || tv==0 || tv==15){r=105;g=65;b=32;}
          if(tv>6 && tv<10 && ((h.block==MC_DOOR && tu>10) || (h.block==MC_DOOR_OPEN && tu<5))){r=220;g=177;b=67;}
          break;
        case MC_SLAB:
        case MC_SLAB_TOP:
          r=139;g=143;b=149;
          if((tu+((tv/5)&1)*4)%7==0 || tv%5==0){r=94;g=99;b=106;}
          break;
        default:break;
      }
      float ambient=.22f+.78f*daylight;
      /* Underground surfaces stay dark; nearby torches provide warm local light. */
      if(h.y<14)ambient*=.62f;
      float glow=0;
      for(int l=0;l<nl;l++) {
        float lx=h.x-lights[l][0],ly=h.y-lights[l][1],lz=h.z-lights[l][2];
        glow=fmaxf(glow,clamp(1-(lx*lx+ly*ly+lz*lz)/42,0,1));
      }
      ambient=fmaxf(ambient,glow*.95f);
      if(h.block==MC_TORCH)ambient=1;
      float shade=(h.ny>0?1.0f:h.ny<0?.50f:h.nx?.80f:.68f)*ambient;
      float fog=clamp((h.distance-18)/22,0,1);fog*=fog;
      r=(unsigned)clamp((r+n)*shade*(1-fog)+skyR*fog,0,255);
      g=(unsigned)clamp((g+n)*shade*(1-fog)+skyG*fog,0,255);
      b=(unsigned)clamp((b+n)*shade*(1-fog)+skyB*fog,0,255);
      if(tu==0 || tv==0){r=r*9/10;g=g*9/10;b=b*9/10;}
      if(w->target_valid && h.x==w->target.x && h.y==w->target.y && h.z==w->target.z) {
        if(tu==0 || tu==15 || tv==0 || tv==15){r=246;g=237;b=180;}
        if(w->mining>0 && ((tu+tv*3)%((unsigned)(12-w->mining*8))==0)){r/=3;g/=3;b/=3;}
      }
    }
    for(unsigned i=0;i<MC_DROPS;i++)if((drop_mask&(1u<<i)) && (int)x>=drop_rect[i][0] && (int)x<=drop_rect[i][2] && (int)y>=drop_rect[i][1] && (int)y<=drop_rect[i][3] &&
        (!world_hit || drop_depth[i]<h.distance)) {
      uint16_t c=MC_ItemColor(w->drops[i].item);r=((c>>11)&31)*255/31;g=((c>>5)&63)*255/63;b=(c&31)*255/31;
      int edge=(int)x==drop_rect[i][0] || (int)y==drop_rect[i][1] || (int)x==drop_rect[i][2] || (int)y==drop_rect[i][3];
      if(edge){r=r*2/3;g=g*2/3;b=b*2/3;}
      break;
    }
    unsigned candidates=0;
    for(unsigned i=0;i<MC_MOBS;i++)if((mob_mask&(1u<<i)) && (int)x>=mob_rect[i][0] && (int)x<=mob_rect[i][2] && (int)y>=mob_rect[i][1] && (int)y<=mob_rect[i][3])candidates|=1u<<i;
    if(candidates) {
      int id,axis,sign;float distance;
      if(MC_MobRay(w,dx,dy,dz,world_hit?h.distance:40,candidates,&id,&distance,&axis,&sign)>=0) {
        const MC_Mob *m=&w->mobs[id];float py=w->y+1.62f+dy*distance-m->y;
        float px=w->x+dx*distance-m->x,pz=w->z+dz*distance-m->z;
        r=48;g=114;b=101;
        if(py>1.32f){r=104;g=145;b=64;
          float face=axis==2?px:pz;
          if(axis!=1 && py>1.55f && py<1.65f && fabsf(face)>.07f && fabsf(face)<.21f){r=238;g=214;b=120;}
          if(axis!=1 && py>1.38f && py<1.46f && fabsf(face)<.13f){r=39;g=57;b=32;}
        }else if(py<.62f){r=63;g=61;b=88;if(fabsf(axis==2?px:pz)<.035f){r=30;g=33;b=45;}}
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
uint32_t MC_Crc32(const void *data,unsigned length)
{
  uint32_t crc=~0u; const uint8_t *p=data;
  while(length--) { crc^=*p++; for(unsigned i=0;i<8;i++) crc=(crc>>1)^(0xedb88320u & (uint32_t)-(int32_t)(crc&1)); }
  return ~crc;
}
