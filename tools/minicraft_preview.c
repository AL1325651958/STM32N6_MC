#include "minicraft.h"
#include "mc_save.h"
#include "mc_ui.h"
#include "mc_view.h"
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
  MC_View *view=calloc(1,sizeof(*view));uint16_t *fb=malloc(800*480*2);
  if(!w.blocks||!save||!view||!fb) return 1;
  MC_Generate(&w,0x20261020);
  char path[1024];
  const char *variants[]={"preview","night","inventory","mining","chest","furnace","creative","geometry","farming","combat","ruins",
    "action_attack","action_shoot","action_feed","action_place","action_use",
    "crop_dry","crop_dark","crop_growing","crop_ready","action_eat","main_menu","create_world","slab_merge","cave_threat","cave_lit",
    "save_busy","save_ok","save_no_sd","save_locked","save_write_failed","save_verify_failed","creative_page1","creative_page2",
    "tool_interact","sneak_build","tool_chest_open","door_closed","door_open","lever_off","lever_on","slab_steps","delete_world","worlds_delete",
    "arrow_door_closed","arrow_door_open","arrow_slab_gap","cave_cache","cave_cache_loot",
    "hud_meters","empty_hand","armor_storage","armor_equip","new_world_view","bed_safe_respawn",
    "notice_short","notice_expired","notice_broken_bow","notice_inventory","inventory_cached_selection","furnace_cached_progress",
    "furnace_empty","furnace_invalid","furnace_blocked","furnace_needs_fuel","furnace_ready","furnace_smelting","furnace_done",
    "furnace_cold","furnace_burning","delete_error"};
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
      MC_Set(&w,13,3,11,MC_DOOR_TOP);MC_Set(&w,14,3,11,MC_DOOR_TOP_OPEN);
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
    if(k>=26 && k<=31) {
      static const unsigned results[]={MC_SAVE_BUSY,MC_SAVE_OK,MC_SAVE_NO_SD,MC_SAVE_LOCKED,MC_SAVE_WRITE_FAILED,MC_SAVE_VERIFY_FAILED};
      MC_Generate(&w,0x20261020);MC_Give(&w,MC_TORCH,8);
      MC_NotifySave(&w,results[k-26]);w.notice=NULL;
    }
    if(k==32 || k==33) {
      MC_Generate(&w,0x20261020);MC_SetMode(&w,1);w.bag=1;w.slot=7;
      if(k==33)MC_UIBagTap(&w,100,90);
      w.notice="SELECT HOTBAR SLOT THEN CATALOG ITEM";
    }
    if(k>=34 && k<=36) {
      memset(w.blocks,0,MC_MAP_BYTES);MC_NewPlayer(&w,0);
      for(int z=0;z<MC_MAP_Z;z++)for(int x=0;x<MC_MAP_X;x++)MC_Set(&w,x,1,z,MC_GRASS);
      MC_Set(&w,12,2,12,MC_CHEST);w.chest_map[0][0]=13;w.chest_map[0][1]=3;w.chest_map[0][2]=13;
      w.chest_blocks[0][0]=(MC_Stack){MC_WOOD,32,0};w.chest_blocks[0][1]=(MC_Stack){MC_COAL,8,0};
      w.x=12.5f;w.y=2.01f;w.z=8.5f;w.yaw=0;w.pitch=-.28f;w.sneak=k==35;
      MC_Give(&w,k==35?MC_DIRT:MC_SWORD,k==35?16:1);w.notice=NULL;
      w.target_valid=MC_Pick(&w,400,240,&w.target);assert(w.target_valid&&w.target.block==MC_CHEST);
      if(k==36)assert(MC_SceneTap(&w,400,240)&&w.bag&&w.open_chest==0);
    }
    if(k>=37 && k<=41) {
      memset(w.blocks,0,MC_MAP_BYTES);MC_NewPlayer(&w,0);
      for(int z=0;z<MC_MAP_Z;z++)for(int x=0;x<MC_MAP_X;x++)MC_Set(&w,x,1,z,MC_GRASS);
      w.x=12.5f;w.y=2.01f;w.z=8.5f;w.yaw=0;w.pitch=-.04f;
      if(k==37 || k==38) {
        for(int y=2;y<=4;y++){MC_Set(&w,11,y,13,MC_PLANKS);MC_Set(&w,13,y,13,MC_PLANKS);}
        MC_Set(&w,12,4,13,MC_PLANKS);MC_Set(&w,12,2,13,MC_DOOR);MC_Set(&w,12,3,13,MC_DOOR_TOP);
        MC_Give(&w,MC_AXE,1);if(k==38)assert(MC_SceneTap(&w,400,240));
        assert(MC_BodyClear(&w,12.5f,2.01f,13.1f)==(k==38));w.notice=k==38?"DOOR OPEN":"DOOR CLOSED";
      } else if(k==39 || k==40) {
        w.z=10.5f;w.pitch=-.38f;MC_Set(&w,12,2,13,MC_LEVER);MC_Set(&w,13,2,13,MC_DUST);MC_Set(&w,14,2,13,MC_LAMP);
        MC_Give(&w,MC_LEVER,1);if(k==40)assert(MC_SceneTap(&w,400,240));w.notice=k==40?"LEVER ON":"LEVER OFF";
      } else {
        w.pitch=-.2f;MC_Set(&w,12,2,12,MC_SLAB);MC_Set(&w,12,2,13,MC_STAIR_SOUTH);MC_Set(&w,12,2,14,MC_STONE);
        MC_Set(&w,10,2,12,MC_DOUBLE_SLAB);MC_Set(&w,14,2,12,MC_SLAB_TOP);MC_Give(&w,MC_SLAB,16);
        w.notice="LOWER HALF  UPPER HALF  TWO SLABS MERGED";
      }
      w.target_valid=MC_Pick(&w,400,240,&w.target);
    }
    if(k>=44 && k<=46) {
      memset(w.blocks,0,MC_MAP_BYTES);MC_NewPlayer(&w,0);
      for(int z=0;z<MC_MAP_Z;z++)for(int x=0;x<MC_MAP_X;x++)MC_Set(&w,x,1,z,MC_GRASS);
      w.x=14.2f;w.y=2.01f;w.z=9.5f;w.yaw=-.36f;w.pitch=-.12f;w.time=90;
      if(k<46) {
        for(int y=2;y<=4;y++){MC_Set(&w,11,y,13,MC_PLANKS);MC_Set(&w,13,y,13,MC_PLANKS);}
        MC_Set(&w,12,4,13,MC_PLANKS);
        MC_Set(&w,12,2,13,k==44?MC_DOOR:MC_DOOR_OPEN);
        MC_Set(&w,12,3,13,k==44?MC_DOOR_TOP:MC_DOOR_TOP_OPEN);
      } else MC_Set(&w,12,2,13,MC_SLAB);
      w.projectiles[0]=(MC_Projectile){12.5f,k==46?2.9f:3.5f,11.5f,0,0,18,0,1};
      MC_ProjectilesTick(&w,.1f);assert(w.projectiles[0].active==(k!=44));
      MC_Give(&w,MC_BOW,1);w.notice=NULL;
    }
    if(k==47 || k==48) {
      MC_Generate(&w,0x20261020);unsigned slot=0;
      while(slot<MC_CHESTS && w.chest_blocks[slot][0].item!=MC_IRON)slot++;
      assert(slot<MC_CHESTS);
      int cx=w.chest_map[slot][0]-2,floor=w.chest_map[slot][1]-2,cz=w.chest_map[slot][2]-1;
      w.x=cx+.5f;w.y=floor+1.01f;w.z=cz-2.5f;w.yaw=0;w.pitch=-.06f;w.time=90;
      if(k==48) {
        w.z=cz+.5f;w.yaw=1.570796327f;w.pitch=-.8f;
        assert(MC_SceneTap(&w,400,240)&&w.bag&&w.open_chest==slot);
      }
      w.notice=NULL;
    }
    if(k>=49 && k<=52) {
      MC_Generate(&w,0x20261020);
      if(k==49) {
        w.health=13;w.hunger=9;w.experience=MC_XpForLevel(5)+5;w.level=5;
        MC_Give(&w,MC_STONE_PICK,1);w.slots[0].wear=3;MC_SyncInventory(&w);
      }
      if(k>=51) {
        memset(w.blocks,0,MC_MAP_BYTES);MC_NewPlayer(&w,0);
        for(int z=0;z<MC_MAP_Z;z++)for(int x=0;x<MC_MAP_X;x++)MC_Set(&w,x,1,z,MC_GRASS);
        w.x=10.5f;w.y=2.01f;w.z=10.5f;w.yaw=0;w.pitch=-.35f;
        for(unsigned a=0;a<MC_ARMOR_SLOTS;a++){MC_Give(&w,MC_IRON_HELMET+a,1);w.slots[a].wear=(uint16_t)(17+a*21);}
        if(k==51) {
          MC_Set(&w,10,2,13,MC_CHEST);w.chest_map[0][0]=11;w.chest_map[0][1]=3;w.chest_map[0][2]=14;
          assert(MC_SceneTap(&w,400,240)&&w.bag&&w.open_chest==0);
          MC_UIBagTap(&w,30,115);MC_UIBagTap(&w,435,115);
          assert(w.chest_blocks[0][0].item==MC_IRON_HELMET&&!w.armor[0].count);
          MC_UIBagTap(&w,80,115);
        } else {
          w.bag=1;MC_UIBagTap(&w,30,115);MC_UIBagTap(&w,440,410);
          MC_UIBagTap(&w,80,115);assert(w.armor[0].count&&!w.armor[1].count);
        }
      }
      w.notice=NULL;
    }
    if(k==53){MC_Generate(&w,0x20261020);w.notice=NULL;}
    if(k==54) {
      memset(w.blocks,0,MC_MAP_BYTES);MC_NewPlayer(&w,0);
      for(int z=0;z<MC_MAP_Z;z++)for(int x=0;x<MC_MAP_X;x++)MC_Set(&w,x,1,z,MC_GRASS);
      MC_Set(&w,12,2,12,MC_BED);
      for(int z=10;z<=14;z++)for(int x=10;x<=14;x++) {
        MC_Set(&w,x,4,z,MC_PLANKS);
        if(x==10||x==14||z==14)for(int y=2;y<4;y++)MC_Set(&w,x,y,z,MC_PLANKS);
      }
      w.spawn[0]=13;w.spawn[1]=3;w.spawn[2]=13;MC_Respawn(&w);
      assert(w.y<2.02f&&MC_BodyClear(&w,w.x,w.y,w.z));
      w.yaw=.78539816f;w.pitch=-.55f;w.notice=NULL;
    }
    if(k>=55 && k<=58) {
      MC_Generate(&w,0x20261020);
      if(k==55 || k==56) {
        MC_Notify(&w,"NO BLOCKS");
        if(k==56){MC_SystemsTick(&w,3.1f);assert(!w.notice);}
      } else if(k==57) {
        assert(MC_Give(&w,MC_BOW,1)&&MC_Give(&w,MC_ARROW,1));w.slots[0].wear=1;MC_SyncInventory(&w);
        assert(MC_Shoot(&w,400,240)&&!strcmp(w.notice,"BOW BROKE"));
      } else {
        MC_Set(&w,(int)w.x+1,(int)w.y,(int)w.z,MC_TABLE);w.bag=1;w.recipe_page=2;
        assert(!MC_Craft(&w,9)&&!strcmp(w.notice,"NEED MORE MATERIALS"));
      }
    }
    if(k==59) {
      MC_Generate(&w,0x20261020);w.bag=1;MC_Notify(&w,NULL);
      MC_Give(&w,MC_PLANKS,24);MC_Give(&w,MC_STONE_PICK,1);MC_Give(&w,MC_BREAD,3);w.slots[1].wear=17;
      assert(MC_ViewUpdate(view,&w));MC_UIBagTap(&w,80,115);
      assert(!MC_ViewUpdate(view,&w)&&w.selected_slot==1);
    }
    if(k==60) {
      memset(w.blocks,0,MC_MAP_BYTES);MC_NewPlayer(&w,0);
      for(int z=0;z<MC_MAP_Z;z++)for(int x=0;x<MC_MAP_X;x++)MC_Set(&w,x,1,z,MC_GRASS);
      w.x=10.5f;w.y=2.01f;w.z=10.5f;w.yaw=0;w.pitch=-.2f;w.bag=1;w.open_furnace=0;MC_Notify(&w,NULL);
      MC_Set(&w,10,2,13,MC_FURNACE);w.furnaces[0]=(MC_Furnace){11,3,14,{MC_IRON_ORE,8,0},{MC_COAL,4,0},{MC_IRON,2,0},20,1};
      assert(MC_ViewUpdate(view,&w));
      for(unsigned tick=0;tick<20;tick++){MC_SystemsTick(&w,.1f);assert(!MC_ViewUpdate(view,&w));}
      assert(w.furnaces[0].progress>2.9f&&w.furnaces[0].burn<18.1f);
    }
    if(k>=61 && k<=69) {
      memset(w.blocks,0,MC_MAP_BYTES);MC_NewPlayer(&w,0);
      for(int z=0;z<MC_MAP_Z;z++)for(int x=0;x<MC_MAP_X;x++)MC_Set(&w,x,1,z,MC_COBBLE);
      w.x=10.5f;w.y=2.01f;w.z=8.5f;w.yaw=0;w.pitch=-.32f;w.time=435;w.notice=NULL;
      MC_Set(&w,10,2,13,MC_FURNACE);MC_Furnace *f=&w.furnaces[0];*f=(MC_Furnace){.x=11,.y=3,.z=14};
      if(k<=67) {
        unsigned state=k-61;w.bag=1;w.open_furnace=0;
        if(state>=MC_FURNACE_INVALID && state<=MC_FURNACE_SMELTING)f->input=(MC_Stack){state==MC_FURNACE_INVALID?MC_DIRT:MC_IRON_ORE,3,0};
        if(state==MC_FURNACE_BLOCKED)f->output=(MC_Stack){MC_GLASS,1,0};
        if(state==MC_FURNACE_READY || state==MC_FURNACE_SMELTING)f->fuel=(MC_Stack){MC_COAL,2,0};
        if(state==MC_FURNACE_SMELTING){f->burn=23;f->progress=2.7f;}
        if(state==MC_FURNACE_DONE)f->output=(MC_Stack){MC_IRON,3,0};
        assert(MC_FurnaceState(f)==state);
      } else if(k==69)f->burn=23;
    }
    MC_ViewUpdate(view,&w);
    for(unsigned y=0;y<480;y++)for(unsigned x=0;x<800;x++)fb[y*800+x]=view->pixels[y/5*MC_VIEW_W+x/5];
    MC_UIDraw(fb,&w,MC_GRASS,0,0,"NEW WORLD  SD READY",1,0,0);
    if(k==21 || k==22) {const uint8_t banks[2]={1,0};MC_MenuDraw(fb,banks,k==22,1,20261021,0,"EXISTING WORLDS ARE PRESERVED");}
    if(k==42 || k==43) {const uint8_t banks[2]={1,2};MC_MenuDraw(fb,banks,k==42?2:0,1,20261021,0,"CHECK WORLD NUMBER BEFORE DELETING");}
    if(k==70) {
      char error[112];const uint8_t banks[2]={1,2};
      snprintf(error,sizeof(error),MC_Localize("DELETE FAILED %s CODE %u"),"MC0.SAV",(unsigned)0x90);
      MC_MenuDraw(fb,banks,2,0,20261021,0,error);
    }
    snprintf(path,sizeof(path),"%s/%s.ppm",argv[1],variants[k]);
    FILE *f=fopen(path,"wb");if(!f)return 1;
    fputs("P6\n800 480\n255\n",f);
    for(int i=0;i<800*480;i++){uint16_t c=fb[i];fputc(((c>>11)&31)*255/31,f);fputc(((c>>5)&63)*255/63,f);fputc((c&31)*255/31,f);}
    fclose(f);
  }
  MC_Generate(&w,0x20261020);
  MC_SaveEncode(&w,1,save);snprintf(path,sizeof(path),"%s/initial-world-v6.SAV",argv[1]);
  FILE *f=fopen(path,"wb");if(!f)return 1;
  if(fwrite(save,1,MC_SAVE_SIZE,f)!=MC_SAVE_SIZE)return 1;
  fclose(f);
  printf("%u interface/gameplay previews and %u-byte example save generated at %s\n",(unsigned)(sizeof(variants)/sizeof(variants[0])),(unsigned)MC_SAVE_SIZE,argv[1]);
  free(fb);free(view);free(save);free(w.blocks);return 0;
}
