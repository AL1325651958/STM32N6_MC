#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "minicraft.h"
#include "mc_save.h"
#include "mc_ui.h"

static void arena(MC_World *w)
{
  memset(w->blocks,0,MC_MAP_BYTES);
  for(int z=0;z<MC_MAP_Z;z++)for(int x=0;x<MC_MAP_X;x++){
    w->blocks[z*MC_MAP_X+x]=MC_BEDROCK;MC_Set(w,x,1,z,MC_GRASS);
  }
  w->x=10.5f;w->y=2.01f;w->z=10.5f;w->yaw=0;w->pitch=0;w->time=435;
  MC_NewPlayer(w,0);w->time=435;
}

int main(void)
{
  MC_World w={.blocks=calloc(1,MC_MAP_BYTES)};assert(w.blocks);arena(&w);
  assert(MC_DarkAt(&w,25,1,10));w.time=90;assert(!MC_DarkAt(&w,25,1,10));w.time=435;
  MC_Set(&w,25,1,10,MC_AIR);assert(!MC_MobSpawn(&w,25,1,10));
  MC_Set(&w,25,1,10,MC_GRASS);assert(MC_MobSpawn(&w,25,1,10));
  assert(w.mobs[0].health==MC_MOB_HEALTH&&w.mobs[0].on_ground);
  assert(!MC_MobSpawn(&w,25,1,10));
  assert(!MC_MobSpawn(&w,11,1,10)&&!MC_MobSpawn(&w,30,1,10)); /* Safe radius and range cap. */
  MC_Set(&w,23,3,10,MC_STONE);assert(!MC_MobSpawn(&w,23,1,10));MC_Set(&w,23,3,10,MC_AIR);
  for(unsigned i=1;i<MC_MOBS;i++)w.mobs[i]=(MC_Mob){40+(float)i*2,2,40,0,0,0,12,1,0,0};
  assert(!MC_MobSpawn(&w,23,1,10));memset(w.mobs+1,0,sizeof(w.mobs)-sizeof(w.mobs[0]));
  MC_Set(&w,25,1,10,MC_TORCH);assert(!MC_DarkAt(&w,25,1,10));MC_Set(&w,25,1,10,MC_GRASS);
  /* Passive animals only spawn in daylight on grass and use the same fixed
     mob slots as hostile night stalkers. */
  memset(w.mobs,0,sizeof(w.mobs));w.time=90;
  assert(MC_PassiveSpawn(&w,25,1,10));
  assert(w.mobs[0].health==8&&w.mobs[0].type==1&&w.mobs[0].heading>=0);
  memset(w.mobs,0,sizeof(w.mobs));w.time=435;
  assert(!MC_PassiveSpawn(&w,25,1,10));w.time=435;
  w.mobs[0]=(MC_Mob){10.5f,2.01f,13.5f,0,0,0,MC_MOB_HEALTH,1,0,0};
  float d;assert(MC_PickMob(&w,400,240,4,&d)==0&&d>2&&d<3);
  /* The former attack-button area and the centre of the scene are both
     ordinary look/tap input now; a held weapon attack is dispatched from
     the scene contact instead of a separate button. */
  assert(MC_UIRole(720,170)==MC_TOUCH_LOOK&&MC_UIRole(400,240)==MC_TOUCH_LOOK);
  assert(MC_Attack(&w,400,240));assert(w.mobs[0].health==MC_MOB_HEALTH-1&&w.attack_cooldown>0);
  assert(!MC_Attack(&w,400,240));w.attack_cooldown=0;
  assert(MC_Give(&w,MC_SWORD,1));assert(MC_Attack(&w,400,240));assert(w.mobs[0].health==MC_MOB_HEALTH-7);
  w.experience=MC_XpForLevel(5);w.level=5;w.attack_cooldown=0;
  w.mobs[0].health=MC_MOB_HEALTH;assert(MC_Attack(&w,400,240));assert(w.mobs[0].health==MC_MOB_HEALTH-7);
  w.experience=0;w.level=0;
  MC_Set(&w,10,3,12,MC_STONE);w.attack_cooldown=0;assert(MC_PickMob(&w,400,240,4,&d)<0);
  assert(!MC_Attack(&w,400,240));MC_Set(&w,10,3,12,MC_AIR);
  /* Wheat attracts passive animals and two fed animals produce one child.
     This uses the scene tap path and keeps the fixed four-animal budget. */
  memset(w.mobs,0,sizeof(w.mobs));w.yaw=0;w.pitch=0;w.time=90;
  w.mobs[0]=(MC_Mob){10.5f,2.01f,13.5f,0,0,0,8,1,1,0};
  w.mobs[1]=(MC_Mob){11.6f,2.01f,13.5f,0,0,0,8,1,1,0};
  assert(MC_Give(&w,MC_WHEAT,2));
  MC_SelectItem(&w,MC_WHEAT);
  assert(MC_MobFeed(&w,400,400)&&w.mob_fed[0]&&w.items[MC_WHEAT]==1);
  w.yaw=atan2f(w.mobs[1].x-w.x,w.mobs[1].z-w.z);
  assert(MC_MobFeed(&w,400,400)&&!w.mob_fed[0]&&!w.mob_fed[1]&&w.breed_cooldown>0);
  assert(w.items[MC_WHEAT]==0&&w.mobs[2].health==8&&w.mobs[2].type==1);
  for(int i=0;i<130;i++)MC_MobsTick(&w,.1f);
  assert(w.breed_cooldown==0&&w.mobs[2].health==8);
  /* Feeding expires if no partner is nearby. */
  memset(w.mobs,0,sizeof(w.mobs));w.mob_fed[0]=0;w.mob_fed_age[0]=0;w.yaw=0;
  w.mobs[0]=(MC_Mob){10.5f,2.01f,13.5f,0,0,0,8,1,1,0};
  assert(MC_Give(&w,MC_WHEAT,1));MC_SelectItem(&w,MC_WHEAT);
  assert(MC_MobFeed(&w,400,400)&&w.mob_fed[0]);
  for(int i=0;i<210;i++)MC_MobsTick(&w,.1f);
  assert(!w.mob_fed[0]);
  w.mobs[0].x=10.5f;w.mobs[0].z=13.5f;w.mobs[0].y=2.01f;
  MC_SelectItem(&w,MC_SWORD);
  unsigned wear=w.slots[w.slot].wear;w.attack_cooldown=0;
  assert(MC_Attack(&w,400,400)&&w.mobs[0].health==2&&w.slots[w.slot].wear==wear-1);
  /* Animals take normal weapon damage and drop raw meat. */
  memset(w.mobs,0,sizeof(w.mobs));
  w.mobs[0]=(MC_Mob){10.5f,2.01f,13.5f,0,0,0,8,1,1,0};
  w.attack_cooldown=0;assert(MC_Attack(&w,400,400));w.attack_cooldown=0;
  assert(MC_Attack(&w,400,400)&&!w.mobs[0].health&&w.items[MC_RAW_MEAT]==1);
  assert(w.experience==3&&MC_XpLevel(&w)==0);
  /* Hostiles are a night-only threat and leave when dawn arrives.  The
     night predicate is sin(time*2pi/600)<=-.38, so 435 is night and 150 is
     daylight. */
  w.time=435;w.mobs[0]=(MC_Mob){10.5f,2.01f,13.5f,0,0,0,MC_MOB_HEALTH,1,0,0};
  MC_MobsTick(&w,.1f);assert(w.mobs[0].health==MC_MOB_HEALTH);
  w.time=150;w.mobs[0]=(MC_Mob){10.5f,2.01f,13.5f,0,0,0,MC_MOB_HEALTH,1,0,0};
  MC_MobsTick(&w,.1f);assert(!w.mobs[0].health);
  memset(w.slots,0,sizeof(w.slots));MC_SyncInventory(&w);assert(MC_Give(&w,MC_RAW_MEAT,1));
  w.health=10;assert(MC_Use(&w)&&w.health==14&&w.items[MC_RAW_MEAT]==0);
  w.time=435; /* Keep the hostile alive while exercising its attack tick. */
  w.mobs[0]=(MC_Mob){10.5f,2.01f,13.5f,0,0,0,MC_MOB_HEALTH,1,0,0};
  w.mobs[0].z=11.8f;unsigned health=w.health;MC_MobsTick(&w,.1f);assert(w.health<health);
  health=w.health;w.bag=1;MC_MobsTick(&w,.1f);assert(w.health==health);
  w.bag=0;w.creative=1;MC_MobsTick(&w,.1f);assert(!w.mobs[0].health&&w.spawn_clock==0);
  arena(&w);w.mobs[0]=(MC_Mob){10.5f,5,16.5f,0,0,0,12,0,0,0};
  for(int i=0;i<35;i++)MC_MobsTick(&w,.1f);
  assert(w.mobs[0].health&&fabsf(w.mobs[0].y-2)<.01f&&w.mobs[0].on_ground);
  arena(&w);w.mobs[0]=(MC_Mob){10.5f,2.01f,14.5f,0,0,0,12,1,1,0};
  for(int y=2;y<5;y++)for(int x=8;x<14;x++)MC_Set(&w,x,y,12,MC_STONE);
  for(int i=0;i<35;i++)MC_MobsTick(&w,.1f);
  assert(w.mobs[0].z>=13.27f&&w.health==20);
  /* A hidden entity must not alter the rendered wall; verify framebuffer bounds. */
  uint16_t *frame=malloc((MC_VIEW_W*MC_VIEW_H+2)*sizeof(uint16_t));
  uint16_t *reference=malloc(MC_VIEW_W*MC_VIEW_H*sizeof(uint16_t));assert(frame&&reference);
  frame[0]=frame[MC_VIEW_W*MC_VIEW_H+1]=0xa55a;
  MC_Render(&w,frame+1,MC_VIEW_W,MC_VIEW_H);memset(w.mobs,0,sizeof(w.mobs));
  MC_Render(&w,reference,MC_VIEW_W,MC_VIEW_H);
  assert(!memcmp(frame+1,reference,MC_VIEW_W*MC_VIEW_H*sizeof(uint16_t)));
  assert(frame[0]==0xa55a&&frame[MC_VIEW_W*MC_VIEW_H+1]==0xa55a);free(frame);free(reference);

  uint8_t *save=malloc(MC_SAVE_SIZE);MC_World restored={.blocks=calloc(1,MC_MAP_BYTES)};assert(save&&restored.blocks);
  arena(&w);w.mobs[2]=(MC_Mob){30.5f,5.25f,40.5f,-.5f,.7f,.2f,9,1,1,.72f};w.mob_rng=17;
  MC_SaveEncode(&w,3,save);assert(MC_SaveValidate(save,MC_SAVE_SIZE)==3);assert(MC_SaveDecode(&restored,save,MC_SAVE_SIZE));
  assert(w.mobs[2].x==restored.mobs[2].x&&w.mobs[2].health==restored.mobs[2].health&&restored.mobs[2].type==1&&fabsf(restored.mobs[2].heading-.72f)<.001f&&restored.mob_rng==17);
  free(restored.blocks);free(save);free(w.blocks);puts("minicraft mobs, combat, v4 persistence: OK");return 0;
}
