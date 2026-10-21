#include "minicraft.h"
#include "mc_save.h"
#include "mc_ui.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Uses the same world generator, renderer, HUD and save encoder as the firmware. */
int main(int argc,char **argv)
{
  if(argc!=2) {fputs("Usage: minicraft_preview OUTPUT_DIRECTORY\n",stderr);return 1;}
  MC_World w={.blocks=malloc(MC_MAP_BYTES)};
  uint8_t *save=malloc(MC_SAVE_SIZE);
  uint16_t *view=malloc(MC_VIEW_W*MC_VIEW_H*2),*fb=malloc(800*480*2);
  if(!w.blocks||!save||!view||!fb) return 1;
  MC_Generate(&w,0x20261020);
  char path[1024];
  const char *variants[]={"preview","night","inventory","mining","chest","furnace","creative","geometry","farming","combat","ruins",
    "action_attack","action_shoot","action_feed","action_place","action_use",
    "crop_dry","crop_dark","crop_growing","crop_ready","action_eat","main_menu","create_world","slab_merge","cave_threat","cave_lit",
    "save_busy","save_ok","save_no_sd","save_locked","save_write_failed","save_verify_failed"};
  for(unsigned k=0;k<sizeof(variants)/sizeof(variants[0]);k++) {
    MC_CloseBag(&w);w.time=k==1?435:90;w.bag=k==2;w.target_valid=0;
    if(k==2) {for(unsigned i=1;i<MC_ITEM_COUNT;i++)if(i!=MC_BEDROCK&&i!=MC_WATER&&i!=MC_SLAB_TOP)MC_Give(&w,i,MC_ToolIndex(i)>=0?1:2);w.notice="32 SLOTS  CRAFTING  CONTAINERS";}
    else w.notice="TAP TO ACT  HOLD TO MINE  BAG TO CRAFT";
    if(k==3) {w.pitch=-.5f;w.target_valid=MC_Pick(&w,400,240,&w.target);w.mining=.65f;}
    if(k==4) {w.bag=1;w.open_chest=0;w.chest_blocks[0][0]=(MC_Stack){MC_WOOD,64,0};w.chest_blocks[0][5]=(MC_Stack){MC_AXE,1,17};w.notice="SELECT INVENTORY THEN CHEST SLOT";}
    if(k==5) {w.bag=1;w.open_furnace=0;w.furnaces[0]=(MC_Furnace){1,2,1,{MC_IRON_ORE,8,0},{MC_COAL,2,0},{MC_IRON,3,0},23,2.8f};w.notice="SELECT ITEM THEN INPUT OR FUEL";}
    if(k==6) {w.bag=1;MC_SetMode(&w,1);w.notice="SELECT HOTBAR SLOT THEN CATALOG ITEM";}
    if(k==7) {
      memset(w.blocks,0,MC_MAP_BYTES);MC_NewPlayer(&w,0);
      for(int z=0;z<MC_MAP_Z;z++)for(int x=0;x<MC_MAP_X;x++) {
        MC_Set(&w,x,1,z,MC_GRASS);
      }
      MC_Set(&w,8,2,11,MC_PLANKS);MC_Set(&w,9,2,11,MC_CHEST);
      MC_Set(&w,10,2,11,MC_TORCH);MC_Set(&w,11,2,11,MC_SAPLING);MC_Set(&w,12,2,11,MC_BED);
      MC_Set(&w,13,2,11,MC_DOOR);MC_Set(&w,14,2,11,MC_DOOR_OPEN);
      MC_Set(&w,15,2,11,MC_SLAB);MC_Set(&w,16,2,11,MC_SLAB_TOP);
      for(int x=7;x<=10;x++)MC_Set(&w,x,2,13,MC_FENCE);
      MC_Set(&w,12,2,13,MC_STAIR);MC_Set(&w,13,2,13,MC_STAIR_EAST);
      MC_Set(&w,14,2,13,MC_STAIR_SOUTH);MC_Set(&w,15,2,13,MC_STAIR_WEST);
      w.x=12.5f;w.y=2;w.z=7.2f;w.yaw=0;w.pitch=-.24f;w.time=90;
      w.notice="CHEST TORCH SAPLING BED DOOR SLABS FENCE STAIRS";
    }
    if(k==8 || k>=16) {
      memset(w.blocks,0,MC_MAP_BYTES);MC_NewPlayer(&w,0);
      for(int z=0;z<MC_MAP_Z;z++)for(int x=0;x<MC_MAP_X;x++)MC_Set(&w,x,1,z,MC_GRASS);
      for(int x=8;x<=14;x++)MC_Set(&w,x,1,11,MC_FARMLAND);
      MC_Set(&w,9,2,11,MC_WHEAT_CROP);MC_Set(&w,11,2,11,MC_WHEAT_CROP);
      MC_Set(&w,13,2,11,MC_WHEAT_MATURE);
      if(k!=16)for(int x=8;x<=14;x++)MC_Set(&w,x,1,12,MC_WATER);
      w.growers[0]=(MC_Grower){12,3,12,30};
      if(k==19)MC_Set(&w,11,2,11,MC_WHEAT_MATURE);
      w.x=11.5f;w.y=2.01f;w.z=7.2f;w.yaw=0;w.pitch=-.30f;w.time=k==17?435:90;
      MC_Give(&w,MC_WHEAT_SEEDS,2);
      w.target_valid=MC_Pick(&w,400,240,&w.target);
      assert(w.target_valid && w.target.x==11 && w.target.z==11);
      assert(w.target.block==(k==19?MC_WHEAT_MATURE:MC_WHEAT_CROP));
      w.notice="FARMING  WATER + LIGHT + TIME";
    }
    if(k==9){
      w.time=435;w.x=10.5f;w.y=2.01f;w.z=10.5f;w.yaw=0;w.pitch=0;
      MC_Set(&w,10,1,10,MC_GRASS);MC_Set(&w,10,1,11,MC_GRASS);MC_Set(&w,10,1,12,MC_GRASS);
      w.mobs[0]=(MC_Mob){10.5f,2.01f,14.5f,0,0,0,MC_MOB_HEALTH,1,0,0};
      w.mobs[1]=(MC_Mob){12.5f,2.01f,14.0f,0,0,0,8,1,1,0};
      MC_Give(&w,MC_SWORD,1);w.mob_target=0;w.notice="TAP SCREEN TO ATTACK  ANIMAL DROPS RAW MEAT";
    }
    if(k==10){
      MC_Generate(&w,0x20261020);
      int cx=(int)w.chest_map[0][0]-1,h=(int)w.chest_map[0][1]-3,cz=(int)w.chest_map[0][2]-2;
      w.x=cx+.5f;w.y=h+2.01f;w.z=cz-5.5f;w.yaw=0;w.pitch=-.08f;w.time=90;
      w.notice="STONE RUIN  TORCHES  SUPPLY CHEST";
    }
    if(k>=11 && k<=15) {
      static const unsigned action_items[]={MC_SWORD,MC_BOW,MC_WHEAT,MC_DIRT,MC_APPLE};
      w.slot=0;w.slots[0]=(MC_Stack){action_items[k-11],1,0};MC_SyncInventory(&w);
    }
    if(k==20) {
      w.slot=0;w.slots[0]=(MC_Stack){MC_BREAD,1,0};w.health=12;w.hunger=8;MC_SyncInventory(&w);
      w.notice="TAP SCREEN TO EAT";
    }
    if(k==23) {
      memset(w.blocks,0,MC_MAP_BYTES);MC_NewPlayer(&w,0);
      for(int z=0;z<MC_MAP_Z;z++)for(int x=0;x<MC_MAP_X;x++)MC_Set(&w,x,1,z,MC_GRASS);
      MC_Set(&w,10,2,12,MC_SLAB);MC_Set(&w,12,2,12,MC_SLAB_TOP);MC_Set(&w,14,2,12,MC_DOUBLE_SLAB);
      w.x=12.5f;w.y=2;w.z=7.2f;w.yaw=0;w.pitch=-.18f;MC_Give(&w,MC_SLAB,8);
      w.notice="LOWER HALF  UPPER HALF  TWO SLABS MERGED";
    }
    if(k==24 || k==25) {
      memset(w.blocks,0,MC_MAP_BYTES);MC_NewPlayer(&w,0);
      for(int z=0;z<MC_MAP_Z;z++)for(int x=0;x<MC_MAP_X;x++)MC_Set(&w,x,1,z,MC_STONE);
      for(int z=9;z<=22;z++)for(int x=8;x<=17;x++) {
        MC_Set(&w,x,5,z,MC_STONE);
        if(x==8 || x==17 || z==22)for(int y=2;y<5;y++)MC_Set(&w,x,y,z,MC_STONE);
      }
      MC_Set(&w,10,3,22,MC_IRON_ORE);MC_Set(&w,14,3,22,MC_COAL_ORE);
      w.x=12.5f;w.y=2.01f;w.z=6.5f;w.yaw=0;w.pitch=0;w.time=90;
      if(k==25)MC_Set(&w,11,2,16,MC_TORCH);
      assert(MC_MobSpawn(&w,12,1,16)==(k==24));
      MC_Give(&w,k==24?MC_SWORD:MC_TORCH,k==24?1:8);
      w.notice=k==24?"DARK CAVES REMAIN DANGEROUS IN DAYLIGHT":"TORCHES AND POWERED LAMPS PREVENT SPAWNS";
    }
    if(k>=26) {
      static const unsigned results[]={MC_SAVE_BUSY,MC_SAVE_OK,MC_SAVE_NO_SD,MC_SAVE_LOCKED,MC_SAVE_WRITE_FAILED,MC_SAVE_VERIFY_FAILED};
      MC_Generate(&w,0x20261020);MC_Give(&w,MC_TORCH,8);
      MC_NotifySave(&w,results[k-26]);w.notice=NULL;
    }
    MC_Render(&w,view,MC_VIEW_W,MC_VIEW_H);
    for(unsigned y=0;y<480;y++)for(unsigned x=0;x<800;x++)fb[y*800+x]=view[y/5*MC_VIEW_W+x/5];
    MC_UIDraw(fb,&w,MC_GRASS,0,0,"NEW WORLD  SD READY",1,0,0);
    if(k==21 || k==22) {const uint8_t banks[2]={1,0};MC_MenuDraw(fb,banks,k==22,1,20261021,0,"EXISTING WORLDS ARE PRESERVED");}
    snprintf(path,sizeof(path),"%s/%s.ppm",argv[1],variants[k]);
    FILE *f=fopen(path,"wb");if(!f)return 1;
    fputs("P6\n800 480\n255\n",f);
    for(int i=0;i<800*480;i++){uint16_t c=fb[i];fputc(((c>>11)&31)*255/31,f);fputc(((c>>5)&63)*255/63,f);fputc((c&31)*255/31,f);}
    fclose(f);
  }
  MC_Generate(&w,0x20261020);
  MC_SaveEncode(&w,1,save);snprintf(path,sizeof(path),"%s/initial-world-v5.SAV",argv[1]);
  FILE *f=fopen(path,"wb");if(!f)return 1;
  if(fwrite(save,1,MC_SAVE_SIZE,f)!=MC_SAVE_SIZE)return 1;
  fclose(f);
  printf("%u interface/gameplay previews and %u-byte example save generated at %s\n",(unsigned)(sizeof(variants)/sizeof(variants[0])),(unsigned)MC_SAVE_SIZE,argv[1]);
  free(fb);free(view);free(save);free(w.blocks);return 0;
}
