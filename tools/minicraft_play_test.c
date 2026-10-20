#include "minicraft.h"
#include "mc_ui.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
static void scene(MC_World *w) {
 memset(w->blocks,0,MC_MAP_BYTES);
 for(int z=0;z<128;z++)for(int x=0;x<128;x++){w->blocks[z*128+x]=MC_BEDROCK;MC_Set(w,x,1,z,MC_DIRT);}
 MC_NewPlayer(w,0);w->x=10.5f;w->y=2;w->z=10.5f;w->yaw=w->pitch=0;w->vy=0;w->on_ground=1;
}
static void mine(MC_World *w,int frames){for(int i=0;i<frames;i++)MC_PlayTick(w,.1f,1,400,240);}
static void tick(MC_World *w,int frames){for(int i=0;i<frames;i++)MC_SystemsTick(w,.1f);}
static unsigned find(MC_World *w,unsigned item){for(unsigned i=0;i<MC_SLOTS;i++)if(w->slots[i].item==item&&w->slots[i].count)return i;assert(0);return 0;}
static void select_item(MC_World *w,unsigned item){unsigned i=find(w,item);if(i>=8){MC_MoveStack(&w->slots[i],&w->slots[7],0);i=7;}w->slot=(uint8_t)i;MC_SyncInventory(w);}
static void full(MC_World *w){for(unsigned i=0;i<MC_SLOTS;i++)w->slots[i]=(MC_Stack){MC_DIRT,64,0};MC_SyncInventory(w);}
int main(void) {
 MC_Hold h;MC_HoldStart(&h,100,100,1000);assert(!MC_HoldReady(&h,1449));assert(MC_HoldReady(&h,1450));
 MC_HoldMove(&h,105,104,1500);assert(MC_HoldReady(&h,1500));MC_HoldMove(&h,125,104,1510);assert(!MC_HoldReady(&h,1600));assert(MC_HoldReady(&h,1960));
 MC_HoldStart(&h,0,0,0xffffff00u);assert(MC_HoldReady(&h,0x100));assert(MC_UIRole(620,310)==MC_TOUCH_LOOK);
 MC_World w={.blocks=malloc(MC_MAP_BYTES)};assert(w.blocks);scene(&w);
 /* Nearby sand falls on a bounded scheduler, while supported sand stays put. */
 MC_Set(&w,4,5,4,MC_SAND);tick(&w,2);
 assert(MC_Get(&w,4,5,4)==MC_AIR&&MC_Get(&w,4,4,4)==MC_SAND);
 MC_Set(&w,5,5,5,MC_SAND);MC_Set(&w,5,4,5,MC_STONE);tick(&w,40);
 assert(MC_Get(&w,5,5,5)==MC_SAND);
 scene(&w);
 /* Deferred legacy tools retain wear when a full inventory gains space. */
 full(&w);w.reserve[MC_WOOD_PICK]=1;w.reserve_wear[0]=17;tick(&w,1);assert(w.reserve[MC_WOOD_PICK]==1);
 w.slots[9]=(MC_Stack){0};tick(&w,1);assert(!w.reserve[MC_WOOD_PICK]&&w.slots[9].item==MC_WOOD_PICK&&w.slots[9].wear==17);scene(&w);
 /* End-to-end bootstrap from a tree to a stone pick. */
 MC_Set(&w,10,3,13,MC_WOOD);MC_Hit hit;assert(MC_Pick(&w,400,240,&hit)&&hit.z==13);
 MC_PlayTick(&w,.1f,1,400,240);assert(w.mining>0);MC_PlayTick(&w,.1f,0,400,240);assert(w.mining==0);
 mine(&w,20);assert(MC_Get(&w,10,3,13)==MC_AIR&&w.items[MC_WOOD]==1);
 assert(MC_Craft(&w,0)&&w.items[MC_PLANKS]==4);assert(!MC_Craft(&w,3));
 assert(MC_Craft(&w,1)&&w.items[MC_STICK]==4);assert(MC_Give(&w,MC_WOOD,3));
 for(int i=0;i<3;i++){assert(MC_Craft(&w,0));}
 assert(MC_Craft(&w,2));assert(!MC_Craft(&w,3));
 MC_Set(&w,11,2,10,MC_TABLE);assert(MC_Craft(&w,3));select_item(&w,MC_WOOD_PICK);
 MC_Set(&w,10,3,13,MC_STONE);mine(&w,25);assert(w.items[MC_COBBLE]==1&&w.slots[w.slot].wear==59);
 MC_Set(&w,10,3,13,MC_IRON_ORE);mine(&w,25);assert(!w.items[MC_IRON_ORE]);
 for(int i=0;i<2;i++){MC_Set(&w,10,3,13,MC_STONE);mine(&w,25);}assert(MC_Craft(&w,4));select_item(&w,MC_STONE_PICK);
 MC_Set(&w,10,3,13,MC_IRON_ORE);mine(&w,20);assert(w.items[MC_IRON_ORE]==1);
 MC_Set(&w,10,3,13,MC_COAL_ORE);mine(&w,20);assert(w.items[MC_COAL]==1);
 assert(!MC_Craft(&w,8));assert(strstr(MC_RecipeStatus(&w,8),"FURNACE"));
 /* Finite stacks, atomic give/craft, full-bag mining protection. */
 scene(&w);assert(MC_Give(&w,MC_WOOD,130));assert(w.slots[0].count==64&&w.slots[1].count==64&&w.slots[2].count==2);
 assert(!MC_Give(&w,MC_AIR,1)&&!MC_Give(&w,MC_WATER,1));full(&w);
 assert(!MC_Give(&w,MC_WOOD,1));MC_Set(&w,10,3,13,MC_WOOD);mine(&w,20);
 assert(MC_Get(&w,10,3,13)==MC_AIR);assert(w.drops[0].item==MC_WOOD&&w.drops[0].count==1);
 w.slots[0]=(MC_Stack){0};MC_SyncInventory(&w);w.x=w.drops[0].x;w.y=w.drops[0].y-.65f;w.z=w.drops[0].z;tick(&w,2);assert(!w.drops[0].count&&w.items[MC_WOOD]==1);
 scene(&w);full(&w);w.drops[0]=(MC_Drop){10.5f,2.1f,10.5f,0,0,0,MC_WOOD,2};w.drops[1]=(MC_Drop){10.7f,2.1f,10.5f,0,0,0,MC_WOOD,3};tick(&w,1);
 assert(w.drops[0].count==5&&!w.drops[1].count);w.drops[0].age=300;tick(&w,1);assert(!w.drops[0].count);
 w.slots[0]=(MC_Stack){MC_WOOD,2,0};MC_SyncInventory(&w);assert(!MC_Craft(&w,0));assert(w.items[MC_WOOD]==2);
 /* Durability follows the individual tool through swaps and chest transfers. */
 scene(&w);assert(MC_Give(&w,MC_WOOD_PICK,2));w.slots[0].wear=1;MC_Set(&w,10,3,13,MC_STONE);mine(&w,25);
 assert(!w.slots[0].count&&w.slots[1].wear==60&&w.items[MC_WOOD_PICK]==1);
 w.open_chest=0;w.selected_slot=1;assert(MC_ContainerTap(&w,4));assert(w.chest_blocks[0][4].wear==60&&!w.slots[1].count);
 assert(MC_ContainerTap(&w,4)&&w.slots[1].wear==60);MC_CloseBag(&w);
 MC_Stack a={MC_WOOD,11,0},b={0};assert(MC_MoveStack(&a,&b,1)&&a.count==5&&b.count==6);
 assert(MC_MoveStack(&a,&b,1)&&a.count==4&&b.count==7);assert(MC_MoveStack(&a,&b,0)&&!a.count&&b.count==11);
 /* Tools do not share pick tiers. */
 scene(&w);assert(MC_Give(&w,MC_SHOVEL,1));MC_Set(&w,10,3,13,MC_IRON_ORE);mine(&w,50);assert(!w.items[MC_IRON_ORE]);
 scene(&w);assert(MC_Give(&w,MC_AXE,1));MC_Set(&w,10,3,13,MC_WOOD);mine(&w,6);assert(w.items[MC_WOOD]==1);
 scene(&w);assert(MC_Give(&w,MC_AXE,1));w.pitch=-.3f;MC_Set(&w,10,2,13,MC_DOOR);mine(&w,4);
 assert(MC_Get(&w,10,2,13)==MC_AIR&&w.items[MC_DOOR]==1);
 /* Placement consumes exactly the selected physical stack. */
 scene(&w);assert(MC_Give(&w,MC_DIRT,65));w.slot=1;MC_Set(&w,10,3,13,MC_STONE);assert(MC_Place(&w));
 assert(w.slots[0].count==64&&!w.slots[1].count&&MC_Get(&w,10,3,12)==MC_DIRT);
 /* Non-cube building parts keep their shape for placement, selection and mining. */
 scene(&w);assert(MC_IsBlock(MC_FENCE)&&MC_IsBlock(MC_STAIR));
 assert(MC_Give(&w,MC_FENCE,1));MC_Set(&w,10,3,13,MC_STONE);assert(MC_Place(&w));
 assert(MC_Get(&w,10,3,12)==MC_FENCE);assert(MC_Pick(&w,400,240,&hit)&&hit.block==MC_FENCE);
 assert(MC_Give(&w,MC_AXE,1));select_item(&w,MC_AXE);mine(&w,8);assert(MC_Get(&w,10,3,12)==MC_AIR&&w.items[MC_FENCE]==1);
 MC_Set(&w,10,2,12,MC_FENCE);assert(!MC_BodyClear(&w,10.5f,1,11.6f));MC_Set(&w,10,2,12,MC_AIR);
 MC_Set(&w,11,2,10,MC_TABLE);assert(MC_Give(&w,MC_PLANKS,4)&&MC_Give(&w,MC_STICK,2));assert(MC_Craft(&w,19)&&w.items[MC_FENCE]>=3);
 assert(MC_Give(&w,MC_COBBLE,6));assert(MC_Craft(&w,20)&&w.items[MC_STAIR]>=4);
 scene(&w);assert(MC_Give(&w,MC_STAIR,1));MC_Set(&w,10,3,13,MC_STONE);w.yaw=0;assert(MC_Place(&w));
 assert(MC_Get(&w,10,3,12)==MC_STAIR_SOUTH);assert(MC_Pick(&w,400,240,&hit)&&hit.block==MC_STAIR_SOUTH);
 assert(MC_Give(&w,MC_STONE_PICK,1));select_item(&w,MC_STONE_PICK);mine(&w,30);assert(MC_Get(&w,10,3,12)==MC_AIR&&w.items[MC_STAIR]>=1);
 MC_Set(&w,10,2,12,MC_STAIR_EAST);assert(!MC_BodyClear(&w,10.5f,1,11.6f));
 /* Chest registration, opening and nonempty destruction protection. */
 scene(&w);assert(MC_Give(&w,MC_CHEST,1));MC_Set(&w,10,3,13,MC_STONE);assert(MC_Place(&w));assert(w.chest_map[0][0]==11);
 assert(MC_OpenTarget(&w)&&w.bag&&w.open_chest==0);assert(MC_Give(&w,MC_WOOD,10));w.selected_slot=find(&w,MC_WOOD);
 assert(MC_ContainerTap(&w,0));MC_CloseBag(&w);mine(&w,20);assert(MC_Get(&w,10,3,12)==MC_CHEST);
 w.open_chest=0;w.selected_slot=0;assert(MC_ContainerTap(&w,0));MC_CloseBag(&w);mine(&w,20);assert(MC_Get(&w,10,3,12)==MC_AIR);
 /* Persistent furnace: inputs/fuel, elapsed work, exhausted fuel, full output. */
 scene(&w);MC_Set(&w,10,3,13,MC_FURNACE);assert(MC_OpenTarget(&w)&&w.open_furnace==0);
 assert(MC_Give(&w,MC_IRON_ORE,10));assert(MC_Give(&w,MC_COAL,1));assert(MC_Give(&w,MC_STICK,1));
 w.selected_slot=find(&w,MC_STICK);assert(!MC_ContainerTap(&w,0));w.selected_slot=find(&w,MC_IRON_ORE);assert(MC_ContainerTap(&w,0));
 w.selected_slot=find(&w,MC_COAL);assert(MC_ContainerTap(&w,1));MC_Furnace *f=&w.furnaces[0];tick(&w,25);assert(!f->output.count&&f->progress>2.4f);
 tick(&w,26);assert(f->output.item==MC_IRON&&f->output.count==1);tick(&w,360);
 assert(f->output.count==8&&f->input.count+f->output.count==10);unsigned out=f->output.count;tick(&w,100);assert(f->output.count==out);
 assert(MC_ContainerTap(&w,2)&&w.items[MC_IRON]==out&&!f->output.count);
 f->output=(MC_Stack){MC_IRON,64,0};f->burn=10;tick(&w,60);assert(f->output.count==64&&f->progress==0);
 /* Renewable trees, blocked growth retry and no trunk inside player. */
 scene(&w);MC_Set(&w,20,2,20,MC_SAPLING);w.growers[0]=(MC_Grower){21,3,21,119.9f};tick(&w,2);assert(MC_Get(&w,20,2,20)==MC_WOOD&&!w.growers[0].x);
 MC_Set(&w,30,2,30,MC_SAPLING);MC_Set(&w,30,4,30,MC_STONE);w.growers[0]=(MC_Grower){31,3,31,119.9f};tick(&w,2);assert(MC_Get(&w,30,2,30)==MC_SAPLING&&w.growers[0].age<111);
 MC_Set(&w,10,2,10,MC_SAPLING);w.growers[1]=(MC_Grower){11,3,11,119.9f};tick(&w,2);assert(MC_Get(&w,10,2,10)==MC_SAPLING);
 /* Farming: shovel dirt, plant seeds, grow on the low-frequency timer, harvest. */
 scene(&w);MC_Set(&w,10,3,13,MC_DIRT);assert(MC_Give(&w,MC_SHOVEL,1));select_item(&w,MC_SHOVEL);assert(MC_Use(&w));assert(MC_Get(&w,10,3,13)==MC_FARMLAND);
 assert(MC_Give(&w,MC_WHEAT_SEEDS,1));select_item(&w,MC_WHEAT_SEEDS);assert(MC_Use(&w));assert(MC_Get(&w,10,4,13)==MC_WHEAT_CROP);
 tick(&w,751);assert(MC_Get(&w,10,4,13)==MC_WHEAT_MATURE);assert(w.growers[0].x==0);
 w.pitch=.25f;mine(&w,6);assert(MC_Get(&w,10,4,13)==MC_FARMLAND&&w.items[MC_WHEAT]>=1&&w.items[MC_WHEAT_SEEDS]>=1);
 /* Bed night skip, safe respawn and survival food/oxygen/death. */
 scene(&w);MC_Set(&w,10,3,13,MC_BED);w.pitch=-.15f;w.time=435;assert(MC_Use(&w)&&w.time==90&&w.spawn[2]==14);w.time=150;assert(MC_Use(&w)&&w.time==150);
 MC_Respawn(&w);assert(w.x>9&&w.x<15&&w.z>10&&w.z<16);
 scene(&w);assert(MC_Give(&w,MC_APPLE,3));w.health=12;w.hunger=8;
 assert(MC_Use(&w)&&w.health==17&&w.hunger==14&&w.items[MC_APPLE]==2);
 w.health=20;assert(MC_Use(&w)&&w.health==20&&w.hunger==20&&w.items[MC_APPLE]==1);
 w.hunger=1;for(int i=0;i<320;i++)MC_PlayTick(&w,.1f,0,400,240);assert(w.hunger==0&&w.health<20);
 MC_Set(&w,10,3,10,MC_WATER);for(int i=0;i<140;i++)MC_PlayTick(&w,.1f,0,400,240);assert(w.health<17);
 MC_Set(&w,10,3,10,MC_AIR);MC_PlayTick(&w,.1f,0,400,240);assert(w.air==10);w.health=2;w.fall_speed=18;w.hurt_timer=0;MC_PlayTick(&w,.1f,0,400,240);assert(w.health==20&&w.items[MC_APPLE]==1);
 /* Redstone: bounded propagation, lamp lighting, and a half-second button pulse. */
 scene(&w);MC_Set(&w,10,3,10,MC_LEVER_ON);MC_Set(&w,11,3,10,MC_DUST);MC_Set(&w,12,3,10,MC_DUST);MC_Set(&w,13,3,10,MC_LAMP);
 MC_RedstoneRefresh(&w);assert(MC_Get(&w,11,3,10)==MC_DUST_ON&&MC_Get(&w,12,3,10)==MC_DUST_ON&&MC_Get(&w,13,3,10)==MC_LAMP_ON);
 MC_Set(&w,10,3,10,MC_LEVER);MC_RedstoneRefresh(&w);assert(MC_Get(&w,11,3,10)==MC_DUST&&MC_Get(&w,13,3,10)==MC_LAMP);
 MC_Set(&w,10,3,11,MC_BUTTON_ON);w.redstone_button_pos[0][0]=11;w.redstone_button_pos[0][1]=4;w.redstone_button_pos[0][2]=12;w.redstone_button_timer[0]=.5f;
 MC_Set(&w,11,3,11,MC_DUST);MC_Set(&w,12,3,11,MC_LAMP);MC_RedstoneRefresh(&w);assert(MC_Get(&w,11,3,11)==MC_DUST_ON&&MC_Get(&w,12,3,11)==MC_LAMP_ON);
 for(int i=0;i<6;i++) { MC_SystemsTick(&w,.1f); }
 assert(MC_Get(&w,10,3,11)==MC_BUTTON);
 /* Touch layout: all 18 recipes reachable, real slot moves, split and mode. */
 scene(&w);w.bag=1;assert(MC_Give(&w,MC_WOOD,10));MC_UIBagTap(&w,30,115);MC_UIBagTap(&w,78,183);assert(w.slots[9].count==10&&!w.slots[0].count);
 MC_UIBagTap(&w,200,420);assert(w.split);MC_UIBagTap(&w,78,183);MC_UIBagTap(&w,30,115);assert(w.slots[0].count==5&&w.slots[9].count==5);
 for(unsigned i=1;i<=6;i++){MC_UIBagTap(&w,450,85);assert(w.recipe_page==i%7);}assert(MC_RecipeCount()==25);
 assert(MC_RecipeResult(0)==MC_PLANKS&&MC_RecipeIngredient(0,0)==MC_WOOD&&MC_RecipeIngredientCount(0,0)==1);
 /* The former attack button area is now scene input; weapon taps are handled
    by voxel_game.c so the right side only keeps the essential controls. */
 assert(MC_RecipeIngredient(0,1)==0&&MC_UIRole(720,170)==MC_TOUCH_LOOK&&
        MC_UIRole(720,240)==MC_TOUCH_SNEAK&&MC_UIRole(720,300)==MC_TOUCH_PLACE);
 MC_UIBagTap(&w,40,420);assert(w.creative);MC_UIBagTap(&w,750,415);assert(w.slot==7);
 MC_UIBagTap(&w,24+3*48+5,108+3*68+5);assert(w.hotbar[7]==MC_SAPLING);
 MC_UIBagTap(&w,40,420);assert(!w.creative&&w.items[MC_WOOD]==10);MC_UIBagTap(&w,320,420);assert(w.change_world);
 MC_UIBagTap(&w,620,30);assert(!w.bag&&w.open_chest==255&&w.open_furnace==255&&w.moving_slot==255);
 /* Canary catches UI writes beyond framebuffer on every menu. */
 uint16_t *fb=calloc(800*480+64,sizeof(*fb));assert(fb);for(int k=0;k<64;k++)fb[800*480+k]=0xa55a;
 memset(fb,0,800*480*sizeof(*fb));w.bag=0;w.creative=0;w.slot=0;w.slots[0]=(MC_Stack){MC_SWORD,1,0};MC_SyncInventory(&w);
 MC_UIDraw(fb,&w,0,15,67,"TEST",1,0,0);unsigned held_pixels=0;
 for(int yy=270;yy<400;yy++)for(int xx=570;xx<685;xx++)if(fb[yy*800+xx])held_pixels++;
 assert(held_pixels>100);
 for(int k=0;k<6;k++){w.bag=k>0;MC_SetMode(&w,k==5);w.open_chest=k==2?0:255;w.open_furnace=k==3?0:255;MC_UIDraw(fb,&w,0,15,67,"TEST",1,0,0);}
 for(int k=0;k<64;k++)assert(fb[800*480+k]==0xa55a);
 free(fb);free(w.blocks);puts("minicraft slots / crafting / containers / furnace / growth / survival / UI: OK");return 0;
}
