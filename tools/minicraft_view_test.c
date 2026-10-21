#include "mc_view.h"
#include "mc_ui.h"
#include "mc_save.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned renders,reuses;
static void flat(MC_World *w)
{
  memset(w->blocks,0,MC_MAP_BYTES);MC_NewPlayer(w,0);
  for(int z=4;z<25;z++)for(int x=4;x<25;x++)MC_Set(w,x,1,z,MC_STONE);
  w->x=w->z=10.5f;w->y=2.01f;w->yaw=w->pitch=0;w->bag=1;MC_Notify(w,NULL);
}
static void check(MC_View *v,const MC_World *w,uint16_t *reference,int rendered)
{
  int actual=MC_ViewUpdate(v,w);assert(actual==rendered);
  if(actual)renders++;else reuses++;
  MC_Render(w,reference,MC_VIEW_W,MC_VIEW_H);
  assert(!memcmp(v->pixels,reference,sizeof(v->pixels)));
}
static void lcd(const MC_World *w,const uint16_t *scene,uint16_t *fb)
{
  for(unsigned y=0;y<480;y++)for(unsigned x=0;x<800;x++)fb[y*800+x]=scene[(y/5)*MC_VIEW_W+x/5];
  MC_UIDraw(fb,w,0,15,67,"TEST",1,0,0);
}
int main(void)
{
  MC_World w={.blocks=calloc(1,MC_MAP_BYTES)};MC_View *v=calloc(1,sizeof(*v)),*second=calloc(1,sizeof(*second));
  uint16_t *reference=malloc(sizeof(v->pixels)),*fb=malloc(800*480*2),*old=malloc(800*480*2);
  uint8_t *a=malloc(MC_SAVE_SIZE),*b=malloc(MC_SAVE_SIZE);assert(w.blocks&&v&&second&&reference&&fb&&old&&a&&b);
  flat(&w);check(v,&w,reference,1);check(v,&w,reference,0);
  /* Inventory/UI-only changes reuse the scene but draw a fresh LCD overlay. */
  lcd(&w,v->pixels,old);assert(MC_Give(&w,MC_PLANKS,24));w.selected_slot=0;w.moving_slot=0;
  check(v,&w,reference,0);lcd(&w,v->pixels,fb);assert(memcmp(fb,old,800*480*2));
  lcd(&w,reference,old);assert(!memcmp(fb,old,800*480*2));
  w.recipe_page=1;w.slots[0].count=12;MC_SyncInventory(&w);check(v,&w,reference,0);
  MC_Notify(&w,"BAG FULL");check(v,&w,reference,0);lcd(&w,v->pixels,old);
  MC_SystemsTick(&w,3);check(v,&w,reference,0);lcd(&w,v->pixels,fb);assert(memcmp(fb,old,800*480*2));
  MC_NotifySave(&w,MC_SAVE_WRITE_FAILED);check(v,&w,reference,0);
  w.open_chest=0;w.chest_blocks[0][0]=(MC_Stack){MC_IRON,3,0};check(v,&w,reference,0);
  w.open_chest=255;w.open_furnace=0;w.furnaces[0].progress=2;w.furnaces[0].burn=5;check(v,&w,reference,0);
  flat(&w);check(v,&w,reference,1);
  w.x+=.25f;check(v,&w,reference,1);w.y+=.125f;check(v,&w,reference,1);w.z+=.25f;check(v,&w,reference,1);
  w.yaw+=.1f;check(v,&w,reference,1);w.pitch+=.1f;check(v,&w,reference,1);w.time=435;check(v,&w,reference,1);
  MC_Set(&w,10,2,13,MC_STONE);check(v,&w,reference,1);MC_Set(&w,11,2,12,MC_TORCH);check(v,&w,reference,1);
  MC_Set(&w,(int)w.x,(int)(w.y+1.62f),(int)w.z,MC_WATER);check(v,&w,reference,1);
  w.target_valid=1;w.target=(MC_Hit){.x=10,.y=2,.z=13,.block=MC_STONE};check(v,&w,reference,1);
  w.mining=.6f;check(v,&w,reference,1);w.target.y++;check(v,&w,reference,1);
  w.target.x++;check(v,&w,reference,1);w.target.z++;check(v,&w,reference,1);w.target.block=MC_DOOR;check(v,&w,reference,1);
  w.target_valid=0;check(v,&w,reference,1);w.mining=.1f;w.target.x++;check(v,&w,reference,0);
  flat(&w);check(v,&w,reference,1);
  w.mobs[0].x=99;w.drops[0].age=9;w.projectiles[0].vz=30;check(v,&w,reference,0);
  w.mobs[0]=(MC_Mob){10.5f,2.01f,14,0,0,0,12,1,0,0};check(v,&w,reference,1);
  w.mobs[0].health=7;w.mobs[0].vy=2;w.mobs[0].attack=1;w.mobs[0].heading=2;check(v,&w,reference,0);
  w.mobs[0].hurt=.35f;check(v,&w,reference,1);w.mobs[0].hurt=.1f;check(v,&w,reference,0);
  w.mobs[0].hurt=0;check(v,&w,reference,1);w.mobs[0].type=1;check(v,&w,reference,1);
  w.mobs[0].x+=.3f;check(v,&w,reference,1);w.mobs[0].y+=.3f;check(v,&w,reference,1);w.mobs[0].z+=.3f;check(v,&w,reference,1);
  w.mobs[0].health=0;check(v,&w,reference,1);
  w.drops[0]=(MC_Drop){10.5f,2.5f,12,0,0,0,MC_APPLE,1};check(v,&w,reference,1);
  w.drops[0].age=12;w.drops[0].spin=1;w.drops[0].vy=-1;w.drops[0].count=2;check(v,&w,reference,0);
  w.drops[0].x+=.2f;check(v,&w,reference,1);w.drops[0].y+=.2f;check(v,&w,reference,1);w.drops[0].z+=.2f;check(v,&w,reference,1);
  w.drops[0].item=MC_COAL;check(v,&w,reference,1);w.drops[0].count=0;check(v,&w,reference,1);
  w.projectiles[0]=(MC_Projectile){11,3,15,0,0,18,0,1};check(v,&w,reference,1);
  w.projectiles[0].age=3;check(v,&w,reference,0);
  w.projectiles[0].x+=.2f;check(v,&w,reference,1);w.projectiles[0].y+=.2f;check(v,&w,reference,1);w.projectiles[0].z+=.2f;check(v,&w,reference,1);
  w.projectiles[0].vx+=2;check(v,&w,reference,1);w.projectiles[0].vy+=2;check(v,&w,reference,1);w.projectiles[0].vz+=2;check(v,&w,reference,1);
  w.projectiles[0].active=0;check(v,&w,reference,1);
  /* Identity, revision wrap, explicit invalidation and separate view owners. */
  w.scene_serial++;check(v,&w,reference,1);w.seed++;check(v,&w,reference,1);
  w.map_revision=0xffffffffu;check(v,&w,reference,1);MC_Set(&w,10,2,13,MC_PLANKS);assert(!w.map_revision);check(v,&w,reference,1);
  MC_World alias=w;check(v,&alias,reference,1);check(v,&w,reference,1);
  uint8_t *copy=malloc(MC_MAP_BYTES);assert(copy);memcpy(copy,w.blocks,MC_MAP_BYTES);uint8_t *original=w.blocks;w.blocks=copy;
  check(v,&w,reference,1);w.blocks=original;check(v,&w,reference,1);free(copy);
  v->pixels[0]^=0xffff;MC_ViewInvalidate(v);check(v,&w,reference,1);
  check(second,&w,reference,1);check(v,&w,reference,0);check(second,&w,reference,0);
  w.bag=0;check(v,&w,reference,1);check(v,&w,reference,1);w.bag=1;check(v,&w,reference,1);check(v,&w,reference,0);
  MC_SaveEncode(&w,4,a);check(v,&w,reference,0);MC_SaveEncode(&w,4,b);assert(!memcmp(a,b,MC_SAVE_SIZE));
  printf("minicraft view: %u renders / %u reuses match raw pixels; camera, edits, lights, entities, UI, world identity, reopen and save invariants: OK (%lu bytes beyond existing scene buffer)\n",renders,reuses,(unsigned long)(sizeof(*v)-sizeof(v->pixels)));
  free(w.blocks);free(v);free(second);free(reference);free(fb);free(old);free(a);free(b);return 0;
}
