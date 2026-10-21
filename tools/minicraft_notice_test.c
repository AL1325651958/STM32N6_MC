#include "minicraft.h"
#include "mc_ui.h"
#include "mc_save.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void reset(MC_World *w)
{
  memset(w->blocks,0,MC_MAP_BYTES);MC_NewPlayer(w,0);
  for(int z=0;z<MC_MAP_Z;z++)for(int x=0;x<MC_MAP_X;x++) {
    w->blocks[z*MC_MAP_X+x]=MC_BEDROCK;MC_Set(w,x,1,z,MC_STONE);
  }
  w->x=w->z=10.5f;w->y=2.01f;w->yaw=w->pitch=0;MC_Notify(w,NULL);
}
static void draw(const MC_World *w,uint16_t *fb)
{
  for(unsigned i=0;i<800*480;i++)fb[i]=0x4a69;
  MC_UIDraw(fb,w,0,15,67,"TEST",1,0,0);
}
static void changed_only(const uint16_t *a,const uint16_t *b,int right,int y0)
{
  unsigned changed=0;
  for(int y=0;y<480;y++)for(int x=0;x<800;x++)if(a[y*800+x]!=b[y*800+x]) {
    changed++;assert(x>=10&&x<right&&y>=y0&&y<y0+24);
  }
  assert(changed);
}
int main(void)
{
  MC_World w={.blocks=calloc(1,MC_MAP_BYTES)},restored={.blocks=calloc(1,MC_MAP_BYTES)};
  uint8_t *a=malloc(MC_SAVE_SIZE),*b=malloc(MC_SAVE_SIZE);
  uint16_t *storage=malloc((800*480+2)*2),*reference=malloc(800*480*2);
  assert(w.blocks&&restored.blocks&&a&&b&&storage&&reference);uint16_t *fb=storage+1;
  storage[0]=storage[800*480+1]=0xa55a;reset(&w);
  MC_Notify(&w,"BAG FULL");assert(w.notice_time==3&&!strcmp(w.notice,"BAG FULL"));
  MC_SystemsTick(&w,1.5f);assert(w.notice_time==1.5f&&w.notice);
  MC_Notify(&w,"BAG FULL");assert(w.notice_time==3); /* same pointer is a new event */
  MC_SystemsTick(&w,0);MC_SystemsTick(&w,-1);MC_SystemsTick(&w,NAN);assert(w.notice_time==3);
  w.bag=1;MC_SystemsTick(&w,3);assert(!w.notice&&!w.notice_time);
  MC_Notify(&w,"NEED ARROWS");MC_SystemsTick(&w,30);assert(!w.notice&&!w.notice_time);
  MC_Notify(&w,"");assert(!w.notice&&!w.notice_time);MC_Notify(&w,NULL);assert(!w.notice);
  MC_Notify(&w,"BAG FULL");MC_NotifySave(&w,MC_SAVE_WRITE_FAILED);MC_SystemsTick(&w,3);
  assert(!w.notice&&w.save_feedback==MC_SAVE_WRITE_FAILED&&w.save_feedback_time==3);
  MC_NewPlayer(&w,0);assert(w.notice_time==3&&strcmp(w.notice,"BAG FULL"));
  reset(&w);MC_SaveEncode(&w,4,a);MC_Notify(&w,"BAG FULL");w.notice_time=.25f;MC_SaveEncode(&w,4,b);
  assert(!memcmp(a,b,MC_SAVE_SIZE));MC_Notify(&restored,"NEED ARROWS");
  assert(MC_SaveDecode(&restored,b,MC_SAVE_SIZE)&&restored.notice_time==3&&!strcmp(restored.notice,"WORLD AND INVENTORY LOADED"));

  reset(&w);draw(&w,reference);MC_Notify(&w,"BAG FULL");draw(&w,fb);changed_only(reference,fb,120,74);
  MC_SystemsTick(&w,3);draw(&w,fb);assert(!memcmp(reference,fb,800*480*2));
  char long_zh[601];for(unsigned i=0;i<100;i++)memcpy(long_zh+i*6,"背包",6);long_zh[600]=0;
  MC_Notify(&w,long_zh);draw(&w,fb);changed_only(reference,fb,580,74);
  assert(storage[0]==0xa55a&&storage[800*480+1]==0xa55a);
  w.bag=1;MC_Notify(&w,NULL);draw(&w,reference);MC_Notify(&w,long_zh);draw(&w,fb);changed_only(reference,fb,790,450);
  MC_SystemsTick(&w,3);draw(&w,fb);assert(!memcmp(reference,fb,800*480*2));
  assert(storage[0]==0xa55a&&storage[800*480+1]==0xa55a);

  /* The final useful action still succeeds, but its ordinary success text
     must not overwrite the broken-tool feedback from that same action. */
  reset(&w);assert(MC_Give(&w,MC_BOW,1)&&MC_Give(&w,MC_ARROW,1));w.slots[0].wear=1;MC_SyncInventory(&w);
  assert(MC_Shoot(&w,400,240)&&!w.items[MC_BOW]&&!w.items[MC_ARROW]&&w.projectiles[0].active);
  assert(!strcmp(w.notice,"BOW BROKE")&&w.notice_time==3);
  reset(&w);assert(MC_Give(&w,MC_SHOVEL,1));w.slots[0].wear=1;MC_SyncInventory(&w);w.pitch=-.7f;
  MC_Set(&w,10,1,12,MC_GRASS);assert(MC_UseAt(&w,400,240)&&MC_Get(&w,10,1,12)==MC_FARMLAND&&!w.items[MC_SHOVEL]);
  assert(!strcmp(w.notice,"SHOVEL BROKE")&&w.notice_time==3);
  free(w.blocks);free(restored.blocks);free(a);free(b);free(storage);free(reference);
  puts("minicraft action notices: expiry / repeated events / bag and long frames / independent save feedback / transient saves / bounded Chinese / broken tools: OK");return 0;
}
