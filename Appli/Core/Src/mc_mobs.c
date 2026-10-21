#include "minicraft.h"
#include <math.h>
#include <string.h>

static uint32_t next_random(MC_World *w)
{
  uint32_t x=w->mob_rng?w->mob_rng:0x6d2b79f5u;
  x^=x<<13;x^=x>>17;x^=x<<5;w->mob_rng=x;return x;
}

/* One bounded column, evaluated only for spawn candidates or live hostiles
   at the 10 Hz entity cadence. Glass admits daylight, opaque roofs do not. */
static int sky_visible(const MC_World *w,int x,int y,int z)
{
  for(int yy=y;yy<MC_MAP_Y;yy++) {
    unsigned b=MC_Get(w,x,yy,z);
    if(b!=MC_AIR && b!=MC_GLASS && b!=MC_TORCH)return 0;
  }
  return 1;
}

int MC_DarkAt(const MC_World *w,int x,int y,int z)
{
  if(x<1 || z<1 || x>=MC_MAP_X-1 || z>=MC_MAP_Z-1 || y<1 || y>=MC_MAP_Y-2)return 0;
  if(sinf(w->time*(6.2831853f/600))>-.38f && sky_visible(w,x,y+1,z))return 0;
  for(int yy=y-3;yy<=y+3;yy++)for(int zz=z-4;zz<=z+4;zz++)for(int xx=x-4;xx<=x+4;xx++)
    if(MC_Get(w,xx,yy,zz)==MC_TORCH || MC_Get(w,xx,yy,zz)==MC_LAMP_ON)return 0;
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
    w->mobs[i]=(MC_Mob){px,y+1.01f,pz,0,0,0,MC_MOB_HEALTH,1,0,0};return 1;
  }
  return 0;
}

int MC_PassiveSpawn(MC_World *w,int x,int y,int z)
{
  if(w->creative||w->bag||sinf(w->time*(6.2831853f/600))<=-.38f||
     x<1||z<1||x>=MC_MAP_X-1||z>=MC_MAP_Z-1)return 0;
  uint8_t floor=MC_Get(w,x,y,z);
  if(floor!=MC_GRASS||MC_Get(w,x,y+1,z)!=MC_AIR||MC_Get(w,x,y+2,z)!=MC_AIR||!sky_visible(w,x,y+1,z))return 0;
  float px=x+.5f,pz=z+.5f,dx=px-w->x,dz=pz-w->z,dist=dx*dx+dz*dz;
  if(dist<49||dist>324||fabsf(y+1-w->y)>6||!MC_BodyClear(w,px,y+1.01f,pz))return 0;
  unsigned count=0;
  for(unsigned i=0;i<MC_MOBS;i++)if(w->mobs[i].health) {
    float mx=w->mobs[i].x-px,mz=w->mobs[i].z-pz;
    if(mx*mx+mz*mz<9)return 0;
    if(w->mobs[i].type==1)count++;
  }
  if(count>=4)return 0;
  for(unsigned i=0;i<MC_MOBS;i++)if(!w->mobs[i].health) {
    unsigned r=next_random(w);
    w->mobs[i]=(MC_Mob){px,y+1.01f,pz,0,0,2+(r&15)*.12f,8,1,1,((r>>8)&255)*(6.2831853f/256)};
    return 1;
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
  static const float hostile[6][6]={
    {-.25f,1.30f,-.25f,.25f,1.80f,.25f},{-.20f,.62f,-.13f,.20f,1.30f,.13f},
    {-.20f,0,-.13f,-.035f,.62f,.13f},{.035f,0,-.13f,.20f,.62f,.13f},
    {-.28f,.70f,-.12f,-.20f,1.30f,.12f},{.20f,.70f,-.12f,.28f,1.30f,.12f}};
  static const float passive[6][6]={
    {-.32f,.40f,-.25f,.32f,1.05f,.25f},{.20f,.72f,-.22f,.47f,1.22f,.22f},
    {-.26f,0,-.20f,-.10f,.46f,.20f},{.10f,0,-.20f,.26f,.46f,.20f},
    {-.30f,0,.05f,-.14f,.48f,.24f},{.14f,0,.05f,.30f,.48f,.24f}};
  float d[3]={dx,dy,dz},best=reach;int found=-1,fa=0,fs=0;
  for(unsigned i=0;i<MC_MOBS;i++)if((mask&(1u<<i))&&w->mobs[i].health) {
    const MC_Mob *m=&w->mobs[i];const float (*parts)[6]=m->type?passive:hostile;
    float p[3]={w->x-m->x,w->y+1.62f-m->y,w->z-m->z};float t;int a,s;
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

static void mob_killed(MC_World *w,unsigned type,float x,float y,float z)
{
  if(type) {
    if(!MC_Give(w,MC_RAW_MEAT,1))MC_DropSpawn(w,MC_RAW_MEAT,1,x,y+.45f,z);
    MC_AddExperience(w,3);
    w->notice="ANIMAL DOWN  RAW MEAT";
  } else {
    int stored=MC_Give(w,MC_ARROW,1);
    if(!stored)MC_DropSpawn(w,MC_ARROW,1,x,y+.45f,z);
    MC_AddExperience(w,5);
    w->notice=stored?"NIGHT STALKER DOWN  ARROW":"NIGHT STALKER DOWN";
  }
}

int MC_Attack(MC_World *w,float x,float y)
{
  if(w->creative||w->bag||w->attack_cooldown>0)return 0;
  int target=MC_PickMob(w,x,y,3.5f,0);if(target<0)return 0;
  unsigned weapon=w->slots[w->slot].count?w->hotbar[w->slot]:0;int ti=MC_ToolIndex(weapon);
  unsigned damage=MC_AttackDamage(w,weapon);
  MC_Mob *mob=&w->mobs[target];mob->health=mob->health>damage?(uint8_t)(mob->health-damage):0;mob->hurt=.35f;
  w->attack_cooldown=weapon==MC_SWORD?.48f:.68f;w->attack_flash=.18f;
  if(ti>=0){MC_Stack *tool=&w->slots[w->slot];if(tool->wear&&! --tool->wear)memset(tool,0,sizeof(*tool));MC_SyncInventory(w);}
  if(!mob->health){
    unsigned type=mob->type;float x=mob->x,y=mob->y,z=mob->z;memset(mob,0,sizeof(*mob));w->mob_target=255;
    mob_killed(w,type,x,y,z);
  } else w->notice=mob->type?"ANIMAL HIT":"NIGHT STALKER HIT";
  return 1;
}

static void camera_ray(const MC_World *w,float sx,float sy,float *dx,float *dy,float *dz)
{
  float u=(sx-400)/576,v=(240-sy)/576,sp=sinf(w->pitch),cp=cosf(w->pitch),syaw=sinf(w->yaw),cyaw=cosf(w->yaw);
  *dx=syaw*cp+u*cyaw-v*sp*syaw;*dy=sp+v*cp;*dz=cyaw*cp-u*syaw-v*sp*cyaw;
  float inv=1/sqrtf((*dx)*(*dx)+(*dy)*(*dy)+(*dz)*(*dz));*dx*=inv;*dy*=inv;*dz*=inv;
}

static void consume_arrow(MC_World *w)
{
  if(w->creative)return;
  for(unsigned i=0;i<MC_SLOTS;i++)if(w->slots[i].item==MC_ARROW && w->slots[i].count) {
    if(!--w->slots[i].count)memset(&w->slots[i],0,sizeof(MC_Stack));
    MC_SyncInventory(w);return;
  }
}

int MC_Shoot(MC_World *w,float x,float y)
{
  if(w->bag || w->hotbar[w->slot]!=MC_BOW || w->shoot_cooldown>0)return 0;
  if(!w->creative && (!w->slots[w->slot].count || w->slots[w->slot].item!=MC_BOW))return 0;
  if(!w->creative && !w->items[MC_ARROW]){w->notice="NEED ARROWS";return 0;}
  int slot=-1;for(unsigned i=0;i<MC_PROJECTILES;i++)if(!w->projectiles[i].active){slot=(int)i;break;}
  if(slot<0){w->notice="TOO MANY ARROWS";return 0;}
  float dx,dy,dz;camera_ray(w,x,y,&dx,&dy,&dz);
  MC_Projectile *p=&w->projectiles[slot];
  p->x=w->x+dx*.55f;p->y=w->y+1.62f+dy*.55f;p->z=w->z+dz*.55f;
  p->vx=dx*18;p->vy=dy*18;p->vz=dz*18;p->age=0;p->active=1;
  if(!w->creative) {
    consume_arrow(w);MC_Stack *bow=&w->slots[w->slot];
    if(bow->wear && !--bow->wear){memset(bow,0,sizeof(*bow));w->equipped=0;w->notice="BOW BROKE";}
    MC_SyncInventory(w);
  }
  w->shoot_cooldown=.42f;w->attack_flash=.18f;w->notice="ARROW FIRED";return 1;
}

static float segment_distance2(float ax,float ay,float az,float bx,float by,float bz,float px,float py,float pz)
{
  float vx=bx-ax,vy=by-ay,vz=bz-az,wx=px-ax,wy=py-ay,wz=pz-az;
  float vv=vx*vx+vy*vy+vz*vz,t=vv>1e-8f?(wx*vx+wy*vy+wz*vz)/vv:0;
  if(t<0)t=0;else if(t>1)t=1;
  float dx=ax+vx*t-px,dy=ay+vy*t-py,dz=az+vz*t-pz;return dx*dx+dy*dy+dz*dz;
}

static void arrow_kill_mob(MC_World *w,MC_Mob *m)
{
  unsigned type=m->type;float x=m->x,y=m->y,z=m->z;
  memset(m,0,sizeof(*m));w->mob_target=255;
  mob_killed(w,type,x,y,z);
}

void MC_ProjectilesTick(MC_World *w,float dt)
{
  if(!(dt>0)||w->bag)return;
  dt=fminf(dt,.1f);w->shoot_cooldown=fmaxf(0,w->shoot_cooldown-dt);
  for(unsigned i=0;i<MC_PROJECTILES;i++) {
    MC_Projectile *p=&w->projectiles[i];if(!p->active)continue;
    p->age+=dt;if(p->age>8){memset(p,0,sizeof(*p));continue;}
    unsigned steps=(unsigned)ceilf(dt/.02f);if(!steps)steps=1;float h=dt/steps;
    int gone=0;
    for(unsigned step=0;step<steps && !gone;step++) {
      float ox=p->x,oy=p->y,oz=p->z;p->vy-=8*h;
      float nx=ox+p->vx*h,ny=oy+p->vy*h,nz=oz+p->vz*h;
      /* Sample the segment so an arrow cannot tunnel through a one-cell wall. */
      unsigned samples=(unsigned)ceilf(sqrtf((nx-ox)*(nx-ox)+(ny-oy)*(ny-oy)+(nz-oz)*(nz-oz))/.12f);if(samples<1)samples=1;
      for(unsigned s=1;s<=samples;s++) {
        float t=(float)s/samples,qx=ox+(nx-ox)*t,qy=oy+(ny-oy)*t,qz=oz+(nz-oz)*t;
        if(MC_IsSolid(MC_Get(w,(int)floorf(qx),(int)floorf(qy),(int)floorf(qz)))){gone=1;w->notice="ARROW HIT BLOCK";break;}
        for(unsigned m=0;m<MC_MOBS;m++)if(w->mobs[m].health) {
          const MC_Mob *mob=&w->mobs[m];float radius=mob->type?.55f:.45f;
          if(segment_distance2(ox,oy,oz,qx,qy,qz,mob->x,mob->y+.72f,mob->z)<=radius*radius) {
            MC_Mob *hit=&w->mobs[m];hit->health=hit->health>5?(uint8_t)(hit->health-5):0;hit->hurt=.35f;
            if(!hit->health)arrow_kill_mob(w,hit);else w->notice=hit->type?"ANIMAL HIT":"NIGHT STALKER HIT";
            gone=1;break;
          }
        }
      }
      if(!gone){p->x=nx;p->y=ny;p->z=nz;}
    }
    if(gone)memset(p,0,sizeof(*p));
  }
}

static void consume_selected_wheat(MC_World *w)
{
  if(w->creative)return;
  MC_Stack *s=&w->slots[w->slot];
  if(s->count && s->item==MC_WHEAT) {
    if(!--s->count)memset(s,0,sizeof(*s));
    MC_SyncInventory(w);
  }
}

int MC_MobFeed(MC_World *w,float x,float y)
{
  if(w->bag || (w->creative && w->hotbar[w->slot]!=MC_WHEAT) ||
     (!w->creative && (w->hotbar[w->slot]!=MC_WHEAT || !w->slots[w->slot].count)))return 0;
  float distance;int target=MC_PickMob(w,x,y,3.5f,&distance);
  if(target<0 || !w->mobs[target].type)return 0;
  if(w->mob_fed[target]) {w->notice="ANIMAL ALREADY FED";return 1;}
  consume_selected_wheat(w);w->mob_fed[target]=1;w->mob_fed_age[target]=0;
  w->mobs[target].heading=atan2f(w->x-w->mobs[target].x,-(w->z-w->mobs[target].z));
  for(unsigned i=0;i<MC_MOBS;i++) {
    if((int)i==target || !w->mobs[i].health || !w->mobs[i].type || !w->mob_fed[i])continue;
    float dx=w->mobs[i].x-w->mobs[target].x,dy=w->mobs[i].y-w->mobs[target].y,dz=w->mobs[i].z-w->mobs[target].z;
    if(dx*dx+dy*dy+dz*dz>2.6f*2.6f)continue;
    if(w->breed_cooldown>0) {w->notice="ANIMALS RESTING";return 1;}
    unsigned count=0;for(unsigned k=0;k<MC_MOBS;k++)if(w->mobs[k].health&&w->mobs[k].type)count++;
    if(count>=4) {w->notice="ANIMAL LIMIT 4";return 1;}
    for(unsigned k=0;k<MC_MOBS;k++)if(!w->mobs[k].health) {
      float px=(w->mobs[i].x+w->mobs[target].x)*.5f,pz=(w->mobs[i].z+w->mobs[target].z)*.5f,py=fminf(w->mobs[i].y,w->mobs[target].y);
      if(!MC_BodyClear(w,px,py,pz)) {w->notice="NEED OPEN GROUND";return 1;}
      unsigned r=next_random(w);w->mobs[k]=(MC_Mob){px,py,pz,0,0,0,8,1,1,((r>>8)&255)*(6.2831853f/256)};
      w->mob_fed[i]=w->mob_fed[target]=0;w->mob_fed_age[i]=w->mob_fed_age[target]=0;w->breed_cooldown=12;
      w->notice="ANIMAL BORN";return 1;
    }
  }
  w->notice="ANIMAL FED  FIND A PARTNER";return 1;
}

void MC_Hurt(MC_World *w,unsigned damage)
{
  if(w->creative||w->hurt_timer>0||!damage)return;
  unsigned armor=MC_ArmorPoints(w);
  if(armor) {
    unsigned reduced=(damage*(10u-armor)+9u)/10u;
    if(!reduced)reduced=1;
    damage=reduced;
    for(unsigned i=0;i<MC_ARMOR_SLOTS;i++)if(w->armor[i].count&&w->armor[i].wear) {
      if(--w->armor[i].wear==0){memset(&w->armor[i],0,sizeof(MC_Stack));w->notice="ARMOR BROKE";}
    }
  }
  w->hurt_timer=1;
  if(damage>=w->health){MC_Respawn(w);w->health=20;w->air=10;w->hunger=20;w->hunger_clock=w->starvation_clock=0;w->notice="RESPAWNED  INVENTORY KEPT";}
  else w->health=(uint8_t)(w->health-damage);
}

static void move_mob(MC_World *w,MC_Mob *m,float vx,float vz,float dt)
{
  /* Jump only onto a reachable one-block ledge. Check both the vertical
     takeoff space and the landing volume, so low roofs and tall walls stay
     barriers. Gravity and collision still advance in 20 ms substeps. */
  float speed=sqrtf(vx*vx+vz*vz);
  if(!m->type && m->on_ground && speed>.01f &&
     !MC_BodyClear(w,m->x+vx*.15f,m->y,m->z+vz*.15f)) {
    float lx=m->x+vx/speed*.8f,lz=m->z+vz/speed*.8f;
    if(MC_IsSolid(MC_Get(w,(int)floorf(lx),(int)floorf(m->y+.05f),(int)floorf(lz))) &&
       MC_BodyClear(w,m->x,m->y+.5f,m->z) && MC_BodyClear(w,m->x,m->y+1.05f,m->z) &&
       MC_BodyClear(w,lx,m->y+1.05f,lz)) {m->vy=6.6f;m->on_ground=0;}
  }
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
  dt=fminf(dt,.1f);w->attack_cooldown=fmaxf(0,w->attack_cooldown-dt);w->attack_flash=fmaxf(0,w->attack_flash-dt);w->breed_cooldown=fmaxf(0,w->breed_cooldown-dt);
  if(w->creative){memset(w->mobs,0,sizeof(w->mobs));w->mob_target=255;w->spawn_clock=w->mob_clock=0;return;}
  int night=sinf(w->time*(6.2831853f/600))<=-.38f;
  w->spawn_clock+=dt;w->mob_clock+=dt;
  if(w->spawn_clock>=4) {
    w->spawn_clock=0;unsigned r=next_random(w);
    if((r&3)==0) {
      float angle=((r>>4)&255)*(6.2831853f/256),radius=9+((r>>12)&7);
      int x=(int)(w->x+cosf(angle)*radius),z=(int)(w->z+sinf(angle)*radius);
      for(int y=(int)w->y+3;y>(int)w->y-5&&y>0;y--)
        if(MC_MobSpawn(w,x,y,z) || (!night && MC_PassiveSpawn(w,x,y,z)))break;
    }
  }
  if(w->mob_clock<.1f)return;
  dt=fminf(w->mob_clock,.2f);w->mob_clock=0;
  for(unsigned i=0;i<MC_MOBS;i++) {
    MC_Mob *m=&w->mobs[i];if(!m->health){w->mob_fed[i]=0;w->mob_fed_age[i]=0;continue;}
    /* Dawn clears only exposed hostiles. Sheltered cave enemies persist;
       artificial lighting prevents new spawns but never deletes live mobs. */
    if(!m->type && !night && sky_visible(w,(int)floorf(m->x),(int)floorf(m->y+1.6f),(int)floorf(m->z))) {
      memset(m,0,sizeof(*m));w->mob_fed[i]=0;w->mob_fed_age[i]=0;
      if(w->mob_target==i)w->mob_target=255;
      continue;
    }
    if(w->mob_fed[i] && (w->mob_fed_age[i]+=dt)>20){w->mob_fed[i]=0;w->mob_fed_age[i]=0;}
    m->hurt=fmaxf(0,m->hurt-dt);m->attack=fmaxf(0,m->attack-dt);
    float dx=w->x-m->x,dz=w->z-m->z,dist=sqrtf(dx*dx+dz*dz);
    if(dist>28||fabsf(w->y-m->y)>7||!MC_BodyClear(w,m->x,m->y,m->z)){memset(m,0,sizeof(*m));continue;}
    if(m->type) {
      int lure=!w->creative && w->hotbar[w->slot]==MC_WHEAT && w->slots[w->slot].count;
      if(((lure && dist<12) || dist<7) && dist>.01f)m->heading=atan2f(dx,-dz);
      else if(m->attack<=0){m->heading+=(((int)(next_random(w)&1023)-511)/511.f)*1.3f;m->attack=2+(next_random(w)&31)*.12f;}
      float speed=lure&&dist<12?.72f:dist<7?1.35f:dist<12?.48f:0;
      move_mob(w,m,-sinf(m->heading)*speed,cosf(m->heading)*speed,dt);
      if(m->y<1||m->y>MC_MAP_Y+2)memset(m,0,sizeof(*m));
      continue;
    }
    float speed=dist<12&&dist>1.15f?.82f:0;
    move_mob(w,m,dist>.01f?dx/dist*speed:0,dist>.01f?dz/dist*speed:0,dt);
    if(m->y<1||m->y>MC_MAP_Y+2){memset(m,0,sizeof(*m));continue;}
    if(dist<1.5f&&fabsf(w->y-m->y)<1.2f&&m->attack<=0&&clear_to_player(w,m,dist)) {
      unsigned health=w->health;m->attack=1.25f;MC_Hurt(w,2);if(health<=2&&w->health==20)return;
    }
  }
}
