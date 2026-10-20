#include "minicraft.h"
#include <math.h>
#include <string.h>

static uint32_t next_random(MC_World *w)
{
  uint32_t x=w->mob_rng?w->mob_rng:0x6d2b79f5u;
  x^=x<<13;x^=x>>17;x^=x<<5;w->mob_rng=x;return x;
}

int MC_DarkAt(const MC_World *w,int x,int y,int z)
{
  if(sinf(w->time*(6.2831853f/600))>-.38f || y<1 || y>=MC_MAP_Y-2)return 0;
  for(int yy=y-3;yy<=y+3;yy++)for(int zz=z-4;zz<=z+4;zz++)for(int xx=x-4;xx<=x+4;xx++)
    if(MC_Get(w,xx,yy,zz)==MC_TORCH)return 0;
  return 1;
}

int MC_MobSpawn(MC_World *w,int x,int y,int z)
{
  if(w->creative||w->bag||x<1||z<1||x>=MC_MAP_X-1||z>=MC_MAP_Z-1)return 0;
  uint8_t floor=MC_Get(w,x,y,z);
  if(!MC_IsSolid(floor)||floor==MC_BED||floor==MC_CHEST||!MC_DarkAt(w,x,y,z)||
     MC_Get(w,x,y+1,z)!=MC_AIR||MC_Get(w,x,y+2,z)!=MC_AIR)return 0;
  float px=x+.5f,pz=z+.5f,dx=px-w->x,dz=pz-w->z,dist=dx*dx+dz*dz;
  if(dist<64||dist>256||fabsf(y+1-w->y)>5||!MC_BodyClear(w,px,y+1.01f,pz))return 0;
  for(unsigned i=0;i<MC_MOBS;i++)if(w->mobs[i].health&&
      (w->mobs[i].x-px)*(w->mobs[i].x-px)+(w->mobs[i].z-pz)*(w->mobs[i].z-pz)<4)return 0;
  for(unsigned i=0;i<MC_MOBS;i++)if(!w->mobs[i].health) {
    w->mobs[i]=(MC_Mob){px,y+1.01f,pz,0,0,0,MC_MOB_HEALTH,1};return 1;
  }
  return 0;
}

static int ray_box(const float p[3],const float d[3],const float lo[3],const float hi[3],float reach,float *distance,int *axis,int *sign)
{
  float a=-1e30f,b=1e30f;int na=0,fa=0,ns=0,fs=0;
  for(int i=0;i<3;i++) {
    if(fabsf(d[i])<1e-7f){if(p[i]<lo[i]||p[i]>hi[i])return 0;continue;}
    float u=(lo[i]-p[i])/d[i],v=(hi[i]-p[i])/d[i];int s=d[i]>0?-1:1;
    if(u>v){float t=u;u=v;v=t;}
    if(u>a){a=u;na=i;ns=s;}
    if(v<b){b=v;fa=i;fs=-s;}
    if(a>b)return 0;
  }
  int inside=a<0;float t=inside?b:a;
  if(t<0||t>reach)return 0;
  *distance=t;*axis=inside?fa:na;*sign=inside?fs:ns;return 1;
}

int MC_MobRay(const MC_World *w,float dx,float dy,float dz,float reach,unsigned mask,int *index,float *distance,int *axis,int *sign)
{
  static const float parts[6][6]={
    {-.25f,1.30f,-.25f,.25f,1.80f,.25f},{-.20f,.62f,-.13f,.20f,1.30f,.13f},
    {-.20f,0,-.13f,-.035f,.62f,.13f},{.035f,0,-.13f,.20f,.62f,.13f},
    {-.28f,.70f,-.12f,-.20f,1.30f,.12f},{.20f,.70f,-.12f,.28f,1.30f,.12f}};
  float d[3]={dx,dy,dz},best=reach;int found=-1,fa=0,fs=0;
  for(unsigned i=0;i<MC_MOBS;i++)if((mask&(1u<<i))&&w->mobs[i].health) {
    const MC_Mob *m=&w->mobs[i];float p[3]={w->x-m->x,w->y+1.62f-m->y,w->z-m->z};
    float t;int a,s;
    for(unsigned part=0;part<6;part++)
      if(ray_box(p,d,parts[part],parts[part]+3,best,&t,&a,&s)&&t<best){best=t;found=(int)i;fa=a;fs=s;}
  }
  if(found>=0){if(index)*index=found;if(distance)*distance=best;if(axis)*axis=fa;if(sign)*sign=fs;}
  return found;
}

int MC_PickMob(const MC_World *w,float x,float y,float reach,float *distance)
{
  float u=(x-400)/576,v=(240-y)/576,sp=sinf(w->pitch),cp=cosf(w->pitch),sy=sinf(w->yaw),cy=cosf(w->yaw);
  float dx=sy*cp+u*cy-v*sp*sy,dy=sp+v*cp,dz=cy*cp-u*sy-v*sp*cy,inv=1/sqrtf(dx*dx+dy*dy+dz*dz),t;
  int found=MC_MobRay(w,dx*inv,dy*inv,dz*inv,reach,(1u<<MC_MOBS)-1,0,&t,0,0);
  if(found<0)return -1;
  MC_Hit block;if(MC_Pick(w,x,y,&block)&&block.distance<t)return -1;
  if(distance)*distance=t;
  return found;
}

int MC_Attack(MC_World *w,float x,float y)
{
  if(w->creative||w->bag||w->attack_cooldown>0)return 0;
  int target=MC_PickMob(w,x,y,3.5f,0);if(target<0)return 0;
  unsigned weapon=w->slots[w->slot].count?w->hotbar[w->slot]:0;int ti=MC_ToolIndex(weapon);
  unsigned damage=weapon==MC_SWORD?6:weapon==MC_AXE?5:weapon==MC_IRON_PICK?4:ti>=0?3:1;
  MC_Mob *mob=&w->mobs[target];mob->health=mob->health>damage?(uint8_t)(mob->health-damage):0;mob->hurt=.35f;
  w->attack_cooldown=weapon==MC_SWORD?.48f:.68f;w->attack_flash=.18f;
  if(ti>=0){MC_Stack *tool=&w->slots[w->slot];if(tool->wear&&! --tool->wear)memset(tool,0,sizeof(*tool));MC_SyncInventory(w);}
  if(!mob->health){memset(mob,0,sizeof(*mob));w->mob_target=255;w->notice="NIGHT STALKER DOWN";}
  else w->notice="NIGHT STALKER HIT";
  return 1;
}

void MC_Hurt(MC_World *w,unsigned damage)
{
  if(w->creative||w->hurt_timer>0||!damage)return;
  w->hurt_timer=1;
  if(damage>=w->health){MC_Respawn(w);w->health=20;w->air=10;w->notice="RESPAWNED  INVENTORY KEPT";}
  else w->health=(uint8_t)(w->health-damage);
}

static void move_mob(MC_World *w,MC_Mob *m,float vx,float vz,float dt)
{
  while(dt>0) {
    float h=fminf(dt,.02f);dt-=h;float nx=m->x+vx*h,nz=m->z+vz*h;
    if(MC_BodyClear(w,nx,m->y,m->z))m->x=nx;
    if(MC_BodyClear(w,m->x,m->y,nz))m->z=nz;
    m->vy=fmaxf(-16,m->vy-18*h);float ny=m->y+m->vy*h;
    if(!MC_BodyClear(w,m->x,ny,m->z)) {
      float a=0,b=1;
      for(int k=0;k<8;k++){float t=(a+b)*.5f;if(MC_BodyClear(w,m->x,m->y+(ny-m->y)*t,m->z))a=t;else b=t;}
      m->y+=(ny-m->y)*a;m->on_ground=m->vy<0;m->vy=0;
    }else{m->y=ny;m->on_ground=0;}
  }
}

static int clear_to_player(const MC_World *w,const MC_Mob *m,float dist)
{
  for(float t=.125f;t<1;t+=.125f) {
    float x=m->x+(w->x-m->x)*t,y=m->y+1.0f+(w->y-m->y)*t,z=m->z+(w->z-m->z)*t;
    if(MC_IsSolid(MC_Get(w,(int)floorf(x),(int)floorf(y),(int)floorf(z))))return 0;
  }
  return dist<1.5f;
}

void MC_MobsTick(MC_World *w,float dt)
{
  if(!(dt>0)||w->bag)return;
  dt=fminf(dt,.1f);w->attack_cooldown=fmaxf(0,w->attack_cooldown-dt);w->attack_flash=fmaxf(0,w->attack_flash-dt);
  if(w->creative){memset(w->mobs,0,sizeof(w->mobs));w->mob_target=255;w->spawn_clock=w->mob_clock=0;return;}
  w->spawn_clock+=dt;w->mob_clock+=dt;
  if(w->spawn_clock>=4) {
    w->spawn_clock=0;unsigned r=next_random(w);
    if((r&3)==0) {
      float angle=((r>>4)&255)*(6.2831853f/256),radius=9+((r>>12)&7);
      int x=(int)(w->x+cosf(angle)*radius),z=(int)(w->z+sinf(angle)*radius);
      for(int y=(int)w->y+3;y>(int)w->y-5&&y>0;y--)if(MC_MobSpawn(w,x,y,z))break;
    }
  }
  if(w->mob_clock<.1f)return;
  dt=fminf(w->mob_clock,.2f);w->mob_clock=0;
  for(unsigned i=0;i<MC_MOBS;i++) {
    MC_Mob *m=&w->mobs[i];if(!m->health)continue;
    m->hurt=fmaxf(0,m->hurt-dt);m->attack=fmaxf(0,m->attack-dt);
    float dx=w->x-m->x,dz=w->z-m->z,dist=sqrtf(dx*dx+dz*dz);
    if(dist>28||fabsf(w->y-m->y)>7||!MC_BodyClear(w,m->x,m->y,m->z)){memset(m,0,sizeof(*m));continue;}
    float speed=dist<12&&dist>1.15f?.82f:0;
    move_mob(w,m,dist>.01f?dx/dist*speed:0,dist>.01f?dz/dist*speed:0,dt);
    if(m->y<1||m->y>MC_MAP_Y+2){memset(m,0,sizeof(*m));continue;}
    if(dist<1.5f&&fabsf(w->y-m->y)<1.2f&&m->attack<=0&&clear_to_player(w,m,dist)) {
      unsigned health=w->health;m->attack=1.25f;MC_Hurt(w,2);if(health<=2&&w->health==20)return;
    }
  }
}
