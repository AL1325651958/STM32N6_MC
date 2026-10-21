#include "minicraft.h"
#include "mc_ui.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
static void scene(MC_World *w) {
 memset(w->blocks,0,MC_MAP_BYTES);
 for(int z=0;z<MC_MAP_Z;z++)for(int x=0;x<MC_MAP_X;x++){w->blocks[z*MC_MAP_X+x]=MC_BEDROCK;MC_Set(w,x,1,z,MC_DIRT);}
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
 assert(!strcmp(MC_Localize("BAG"),"背包")&&!strcmp(MC_Localize("STONE SLAB"),"石台阶"));
 MC_World w={.blocks=malloc(MC_MAP_BYTES)};assert(w.blocks);scene(&w);
 /* A release is a tap only before the hold threshold and without a drag.
    Long holds never place/eat first, regardless of the selected item. */
 MC_HoldStart(&h,400,240,100);assert(MC_HoldTap(&h,549));assert(!MC_HoldTap(&h,550));
 MC_HoldMove(&h,430,240,560);assert(!MC_HoldTap(&h,570));
 MC_HoldStart(&h,400,240,0xfffffff0u);assert(MC_HoldTap(&h,30));
 const unsigned mining_items[]={MC_DIRT,MC_APPLE,MC_BREAD,MC_COOKED_MEAT,MC_STICK,MC_WHEAT_SEEDS,MC_SWORD};
 for(unsigned i=0;i<sizeof(mining_items)/sizeof(mining_items[0]);i++) {
   scene(&w);assert(MC_Give(&w,mining_items[i],2));
   MC_Set(&w,10,3,13,MC_WOOD);MC_HoldStart(&h,400,240,100);
   assert(!MC_HoldTap(&h,550));mine(&w,40);
   assert(MC_Get(&w,10,3,13)==MC_AIR&&w.items[mining_items[i]]==2);
 }
 scene(&w);assert(MC_Give(&w,MC_DIRT,2));MC_Set(&w,10,3,13,MC_STONE);
 MC_HoldStart(&h,400,240,100);assert(MC_HoldTap(&h,200));
 assert(MC_SceneTap(&w,400,240)&&w.items[MC_DIRT]==1&&MC_Get(&w,10,3,12)==MC_DIRT);
 scene(&w);
 assert(MC_MenuTap(400,200,0)==MC_MENU_WORLD1&&MC_MenuTap(400,300,0)==MC_MENU_WORLD2);
 assert(MC_MenuTap(400,200,1)==MC_MENU_SEED&&MC_MenuTap(400,260,1)==MC_MENU_MODE);
 assert(MC_MenuTap(400,330,1)==MC_MENU_CREATE&&MC_MenuTap(400,400,1)==MC_MENU_BACK);
 assert(MC_MenuTap(199,330,1)==MC_MENU_NONE&&MC_MenuTap(600,330,1)==MC_MENU_NONE);

 /* Nearby sand falls on a bounded scheduler, while supported sand stays put. */
 MC_Set(&w,4,5,4,MC_SAND);tick(&w,2);
 assert(MC_Get(&w,4,5,4)==MC_AIR&&MC_Get(&w,4,4,4)==MC_SAND);
 MC_Set(&w,5,5,5,MC_SAND);MC_Set(&w,5,4,5,MC_STONE);tick(&w,40);
 assert(MC_Get(&w,5,5,5)==MC_SAND);
 /* Water falls and spreads only within the bounded fluid scheduler. */
 scene(&w);w.fluid_cursor=7u*15u+7u;MC_Set(&w,10,6,10,MC_WATER);tick(&w,3);
 assert(MC_Get(&w,10,5,10)==MC_WATER&&MC_Get(&w,10,6,10)==MC_AIR);
 MC_Set(&w,10,2,10,MC_STONE);MC_Set(&w,10,3,10,MC_WATER);MC_Set(&w,11,2,10,MC_STONE);
 /* Remove the earlier falling sample so the bounded cursor reaches the
    supported source cell on this short deterministic test window. */
 MC_Set(&w,10,5,10,MC_AIR);MC_Set(&w,10,4,10,MC_AIR);
 w.fluid_cursor=7u*15u+7u;tick(&w,3);
 assert(MC_Get(&w,11,3,10)==MC_WATER);
 MC_Set(&w,12,3,10,MC_STONE);MC_Set(&w,11,3,10,MC_AIR);MC_Set(&w,11,2,10,MC_AIR);
 w.fluid_cursor=7u*15u+7u;tick(&w,3);
 assert(MC_Get(&w,11,3,10)==MC_AIR);
 scene(&w);
 /* Deferred legacy tools retain wear when a full inventory gains space. */
 full(&w);w.reserve[MC_WOOD_PICK]=1;w.reserve_wear[0]=17;tick(&w,1);assert(w.reserve[MC_WOOD_PICK]==1);
 w.slots[9]=(MC_Stack){0};tick(&w,1);assert(!w.reserve[MC_WOOD_PICK]&&w.slots[9].item==MC_WOOD_PICK&&w.slots[9].wear==17);scene(&w);
 /* End-to-end bootstrap from a tree to a stone pick. */
 MC_Set(&w,10,3,13,MC_WOOD);MC_Hit hit;assert(MC_Pick(&w,400,240,&hit)&&hit.z==13);
 MC_PlayTick(&w,.1f,1,400,240);assert(w.mining>0);MC_PlayTick(&w,.1f,0,400,240);assert(w.mining==0);
 mine(&w,20);assert(MC_Get(&w,10,3,13)==MC_AIR&&w.items[MC_WOOD]==1&&w.experience>=1);
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
 /* Slabs merge only into their empty half, consume one physical item,
    and never fill space occupied by the player. */
 scene(&w);assert(MC_Give(&w,MC_SLAB,3));MC_Set(&w,10,3,13,MC_SLAB);w.pitch=-.04f;
 assert(MC_Pick(&w,400,240,&hit)&&hit.block==MC_SLAB&&hit.ny==1);
 assert(MC_Place(&w)&&MC_Get(&w,10,3,13)==MC_DOUBLE_SLAB&&w.items[MC_SLAB]==2);
 scene(&w);assert(MC_Give(&w,MC_SLAB,2));MC_Set(&w,10,4,13,MC_SLAB_TOP);w.pitch=.3f;
 assert(MC_Pick(&w,400,240,&hit)&&hit.block==MC_SLAB_TOP&&hit.ny==-1);
 assert(MC_Place(&w)&&MC_Get(&w,10,4,13)==MC_DOUBLE_SLAB&&w.items[MC_SLAB]==1);
 scene(&w);assert(MC_Give(&w,MC_SLAB,2));MC_Set(&w,10,3,13,MC_SLAB);MC_Set(&w,10,3,14,MC_STONE);
 w.pitch=.05f;assert(MC_Place(&w)&&MC_Get(&w,10,3,13)==MC_DOUBLE_SLAB);
 scene(&w);assert(MC_Give(&w,MC_SLAB,2));MC_Set(&w,10,2,10,MC_SLAB);w.y=2.5f;w.pitch=-1.5f;
 assert(!MC_Place(&w)&&MC_Get(&w,10,2,10)==MC_SLAB&&w.items[MC_SLAB]==2);
 /* Off-centre placement and half-height choice use the same normalized
    touch ray, not a second centre-screen pick. */
 scene(&w);assert(MC_Give(&w,MC_SLAB,2));MC_Set(&w,10,3,13,MC_STONE);MC_Set(&w,11,3,13,MC_STONE);
 assert(MC_Pick(&w,600,280,&hit)&&hit.x==11);
 assert(MC_PlaceAt(&w,600,280)&&MC_Get(&w,11,3,12)==MC_SLAB&&MC_Get(&w,10,3,12)==MC_AIR);
 /* The full-height result uses ordinary cube collision and drops two slabs. */
 scene(&w);assert(MC_Give(&w,MC_STONE_PICK,1));MC_Set(&w,10,3,13,MC_DOUBLE_SLAB);
 assert(!MC_Give(&w,MC_DOUBLE_SLAB,1));
 MC_Set(&w,10,2,12,MC_SLAB);assert(MC_BodyClear(&w,10.5f,2.51f,12.5f));
 MC_Set(&w,10,2,12,MC_DOUBLE_SLAB);assert(!MC_BodyClear(&w,10.5f,2.51f,12.5f));
 MC_Set(&w,10,2,12,MC_AIR);mine(&w,25);
 assert(MC_Get(&w,10,3,13)==MC_AIR&&w.items[MC_SLAB]==2);
 scene(&w);full(&w);w.slots[0]=(MC_Stack){MC_STONE_PICK,1,132};MC_SyncInventory(&w);
 for(unsigned i=0;i<MC_DROPS;i++)w.drops[i]=(MC_Drop){50,3,50,0,0,0,MC_DIRT,64};
 w.drops[0]=(MC_Drop){10.5f,3.55f,13.5f,0,0,0,MC_SLAB,63};
 MC_Set(&w,10,3,13,MC_DOUBLE_SLAB);mine(&w,25);
 assert(MC_Get(&w,10,3,13)==MC_DOUBLE_SLAB&&w.drops[0].count==63);
 w.slots[1]=(MC_Stack){0};MC_SyncInventory(&w);mine(&w,25);
 assert(MC_Get(&w,10,3,13)==MC_AIR&&w.items[MC_SLAB]==2&&w.drops[0].count==63);
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
 /* An adjacent chest feeds a furnace and collects its finished output. */
 scene(&w);MC_Set(&w,10,3,10,MC_FURNACE);w.furnaces[0]=(MC_Furnace){11,4,11,{0},{0},{0},0,0};
 MC_Set(&w,11,3,10,MC_CHEST);w.chest_map[0][0]=12;w.chest_map[0][1]=4;w.chest_map[0][2]=11;
 w.chest_blocks[0][0]=(MC_Stack){MC_IRON_ORE,3,0};w.chest_blocks[0][1]=(MC_Stack){MC_COAL,1,0};
 tick(&w,3);f=&w.furnaces[0];assert(f->input.item==MC_IRON_ORE&&f->input.count==3&&f->burn>0&&w.chest_blocks[0][1].count==0);
 tick(&w,200);assert(w.chest_blocks[0][0].item==MC_IRON&&w.chest_blocks[0][0].count==3);
 /* Renewable trees, blocked growth retry and no trunk inside player. */
 scene(&w);MC_Set(&w,20,2,20,MC_SAPLING);w.growers[0]=(MC_Grower){21,3,21,119.9f};tick(&w,2);assert((MC_Get(&w,20,2,20)==MC_WOOD||MC_Get(&w,20,2,20)==MC_BIRCH_WOOD||MC_Get(&w,20,2,20)==MC_DARK_WOOD)&&!w.growers[0].x);
 MC_Set(&w,30,2,30,MC_SAPLING);MC_Set(&w,30,4,30,MC_STONE);w.growers[0]=(MC_Grower){31,3,31,119.9f};tick(&w,2);assert(MC_Get(&w,30,2,30)==MC_SAPLING&&w.growers[0].age<111);
 MC_Set(&w,10,2,10,MC_SAPLING);w.growers[1]=(MC_Grower){11,3,11,119.9f};tick(&w,2);assert(MC_Get(&w,10,2,10)==MC_SAPLING);
 /* Wood variants keep their own blocks and icons, can be converted to
    matching planks, and still satisfy ordinary wood recipes/fuel rules. */
 scene(&w);assert(MC_Give(&w,MC_BIRCH_WOOD,1)&&MC_Give(&w,MC_DARK_WOOD,1));
 assert(MC_Craft(&w,31)&&MC_Craft(&w,32));
 assert(w.items[MC_BIRCH_PLANKS]==4&&w.items[MC_DARK_PLANKS]==4);
 assert(MC_Craft(&w,1)&&w.items[MC_STICK]>=4);
 MC_Set(&w,10,3,10,MC_FURNACE);w.furnaces[0]=(MC_Furnace){11,4,11,{MC_IRON_ORE,1,0},{MC_BIRCH_WOOD,1,0},{0},0,0};
 tick(&w,1);assert(w.furnaces[0].burn>0);tick(&w,55);assert(w.furnaces[0].output.item==MC_IRON);
 /* Farming: shovel dirt, plant seeds, grow on the low-frequency timer, harvest. */
 scene(&w);assert(MC_Give(&w,MC_SHOVEL,1));select_item(&w,MC_SHOVEL);
 w.mobs[0]=(MC_Mob){10.5f,2.01f,13.5f,0,0,0,MC_MOB_HEALTH,1,0,0};
 assert(!MC_SceneTap(&w,400,240)&&w.mobs[0].health==MC_MOB_HEALTH-3);
 memset(w.mobs,0,sizeof(w.mobs));MC_Set(&w,10,3,13,MC_STONE);
 assert(!MC_SceneTap(&w,400,240));mine(&w,50);assert(MC_Get(&w,10,3,13)==MC_AIR);
 scene(&w);MC_Set(&w,10,3,13,MC_DIRT);assert(MC_Give(&w,MC_SHOVEL,1));select_item(&w,MC_SHOVEL);assert(MC_Use(&w));assert(MC_Get(&w,10,3,13)==MC_FARMLAND);
 scene(&w);MC_Set(&w,10,3,13,MC_DIRT);assert(MC_Give(&w,MC_SHOVEL,1));select_item(&w,MC_SHOVEL);
 assert(MC_SceneTap(&w,400,240)&&MC_Get(&w,10,3,13)==MC_FARMLAND);
 assert(MC_Give(&w,MC_WHEAT_SEEDS,1));select_item(&w,MC_WHEAT_SEEDS);assert(MC_Use(&w));assert(MC_Get(&w,10,4,13)==MC_WHEAT_CROP);
 /* Dry fields never advance. Wrong-height and five-block water are ignored;
    a diagonal source within four blocks irrigates the same soil layer. */
 tick(&w,760);assert(MC_Get(&w,10,4,13)==MC_WHEAT_CROP&&w.growers[0].age==0);
 assert(MC_CropGrowthNeeds(&w,10,4,13)==MC_CROP_NEEDS_WATER);
 MC_Set(&w,15,2,13,MC_STONE);MC_Set(&w,15,3,13,MC_WATER);
 MC_Set(&w,14,2,17,MC_WATER);
 assert(MC_CropGrowthNeeds(&w,10,4,13)==MC_CROP_NEEDS_WATER);
 MC_Set(&w,15,3,13,MC_AIR);MC_Set(&w,14,2,17,MC_STONE);MC_Set(&w,14,3,17,MC_WATER);
 assert(!MC_CropGrowthNeeds(&w,10,4,13));tick(&w,200);
 float crop_age=w.growers[0].age;assert(crop_age>=19&&crop_age<=21);
 MC_Set(&w,14,3,17,MC_AIR);tick(&w,100);assert(w.growers[0].age==crop_age);
 MC_Set(&w,14,3,17,MC_WATER);w.time=435;tick(&w,100);
 assert(w.growers[0].age==crop_age&&MC_CropGrowthNeeds(&w,10,4,13)==MC_CROP_NEEDS_LIGHT);
 w.time=90;MC_Set(&w,10,6,13,MC_STONE);tick(&w,100);assert(w.growers[0].age==crop_age);
 MC_Set(&w,10,6,13,MC_GLASS);assert(!MC_CropGrowthNeeds(&w,10,4,13));
 MC_Set(&w,10,5,13,MC_STONE);assert(MC_CropGrowthNeeds(&w,10,4,13)&MC_CROP_NEEDS_SPACE);
 tick(&w,100);assert(w.growers[0].age==crop_age);MC_Set(&w,10,5,13,MC_AIR);
 MC_Set(&w,10,3,13,MC_DIRT);tick(&w,100);assert(w.growers[0].age==crop_age);
 assert(MC_CropGrowthNeeds(&w,10,4,13)&MC_CROP_NEEDS_SOIL);MC_Set(&w,10,3,13,MC_FARMLAND);
 w.time=435;MC_Set(&w,12,4,13,MC_LAMP);assert(MC_CropGrowthNeeds(&w,10,4,13)&MC_CROP_NEEDS_LIGHT);
 MC_Set(&w,12,4,13,MC_LAMP_ON);assert(!MC_CropGrowthNeeds(&w,10,4,13));
 MC_Set(&w,12,4,13,MC_TORCH);assert(!MC_CropGrowthNeeds(&w,10,4,13));
 tick(&w,560);assert(MC_Get(&w,10,4,13)==MC_WHEAT_MATURE);assert(w.growers[0].x==0);
 assert(MC_Give(&w,MC_WHEAT_SEEDS,1));select_item(&w,MC_WHEAT_SEEDS);
 w.pitch=.25f;assert(!MC_SceneTap(&w,400,240));mine(&w,6);
 assert(MC_Get(&w,10,4,13)==MC_AIR&&MC_Get(&w,10,3,13)==MC_FARMLAND&&w.items[MC_WHEAT]>=1&&w.items[MC_WHEAT_SEEDS]>=1);
 w.pitch=0;select_item(&w,MC_WHEAT_SEEDS);assert(MC_SceneTap(&w,400,240));
 assert(MC_Get(&w,10,4,13)==MC_WHEAT_CROP&&w.growers[0].y==5);
 tick(&w,760);assert(MC_Get(&w,10,4,13)==MC_WHEAT_MATURE);
 /* Bed night skip, safe respawn and survival food/oxygen/death. */
 scene(&w);MC_Set(&w,10,3,13,MC_BED);w.pitch=-.15f;w.time=435;assert(MC_Use(&w)&&w.time==90&&w.spawn[2]==14);w.time=150;assert(MC_Use(&w)&&w.time==150);
 MC_Respawn(&w);assert(w.x>9&&w.x<15&&w.z>10&&w.z<16);
 scene(&w);assert(MC_Give(&w,MC_APPLE,3));w.health=12;w.hunger=8;
 assert(MC_Use(&w)&&w.health==17&&w.hunger==14&&w.items[MC_APPLE]==2);
 w.health=20;assert(MC_Use(&w)&&w.health==20&&w.hunger==20&&w.items[MC_APPLE]==1);
 w.hunger=1;for(int i=0;i<320;i++)MC_PlayTick(&w,.1f,0,400,240);assert(w.hunger==0&&w.health<20);
 MC_Set(&w,10,3,10,MC_WATER);for(int i=0;i<140;i++)MC_PlayTick(&w,.1f,0,400,240);assert(w.health<17);
 MC_Set(&w,10,3,10,MC_AIR);MC_PlayTick(&w,.1f,0,400,240);assert(w.air==10);w.health=2;w.fall_speed=18;w.hurt_timer=0;MC_PlayTick(&w,.1f,0,400,240);assert(w.health==20&&w.items[MC_APPLE]==1);
 /* Bow combat: a tap fires, consumes one arrow, damages a mob, and stops at
    the bounded projectile pool. A block hit removes the arrow as well. */
 scene(&w);assert(MC_Give(&w,MC_BOW,1)&&MC_Give(&w,MC_ARROW,5));select_item(&w,MC_BOW);
 MC_Set(&w,10,3,13,MC_STONE);w.shoot_cooldown=0;assert(MC_Shoot(&w,400,240));
 assert(w.items[MC_ARROW]==4&&w.slots[w.slot].item==MC_BOW&&w.slots[w.slot].wear<MC_ToolLife(MC_BOW));
 for(int i=0;i<8;i++)MC_ProjectilesTick(&w,.1f);
 for(unsigned i=0;i<MC_PROJECTILES;i++)assert(!w.projectiles[i].active);
 scene(&w);assert(MC_Give(&w,MC_BOW,1)&&MC_Give(&w,MC_ARROW,2));select_item(&w,MC_BOW);
 w.mobs[0]=(MC_Mob){10.5f,2.01f,14.0f,0,0,0,MC_MOB_HEALTH,1,0,0};w.shoot_cooldown=0;assert(MC_Shoot(&w,400,384));
 for(int i=0;i<20 && w.mobs[0].health==MC_MOB_HEALTH;i++)MC_ProjectilesTick(&w,.1f);
 assert(w.mobs[0].health<MC_MOB_HEALTH);
 scene(&w);assert(MC_Give(&w,MC_BOW,1)&&MC_Give(&w,MC_ARROW,3));select_item(&w,MC_BOW);
 w.shoot_cooldown=0;MC_PlayTick(&w,.1f,1,400,240);assert(w.items[MC_ARROW]==2);
 for(int i=0;i<5;i++){MC_ProjectilesTick(&w,.1f);MC_PlayTick(&w,.1f,1,400,240);}
 assert(w.items[MC_ARROW]==1); /* A held bow repeats without a mob target. */
 scene(&w);assert(MC_Give(&w,MC_BOW,1)&&MC_Give(&w,MC_ARROW,MC_PROJECTILES+1));select_item(&w,MC_BOW);w.shoot_cooldown=0;
 for(unsigned i=0;i<MC_PROJECTILES;i++)w.projectiles[i].active=1;
 assert(!MC_Shoot(&w,400,240));
 /* Redstone: bounded propagation, lamp lighting, and a half-second button pulse. */
 scene(&w);MC_Set(&w,10,3,10,MC_LEVER_ON);MC_Set(&w,11,3,10,MC_DUST);MC_Set(&w,12,3,10,MC_DUST);MC_Set(&w,13,3,10,MC_LAMP);
 MC_RedstoneRefresh(&w);assert(MC_Get(&w,11,3,10)==MC_DUST_ON&&MC_Get(&w,12,3,10)==MC_DUST_ON&&MC_Get(&w,13,3,10)==MC_LAMP_ON);
 MC_Set(&w,10,3,10,MC_LEVER);MC_RedstoneRefresh(&w);assert(MC_Get(&w,11,3,10)==MC_DUST&&MC_Get(&w,13,3,10)==MC_LAMP);
 MC_Set(&w,10,3,11,MC_BUTTON_ON);w.redstone_button_pos[0][0]=11;w.redstone_button_pos[0][1]=4;w.redstone_button_pos[0][2]=12;w.redstone_button_timer[0]=.5f;
 MC_Set(&w,11,3,11,MC_DUST);MC_Set(&w,12,3,11,MC_LAMP);MC_RedstoneRefresh(&w);assert(MC_Get(&w,11,3,11)==MC_DUST_ON&&MC_Get(&w,12,3,11)==MC_LAMP_ON);
 for(int i=0;i<6;i++) { MC_SystemsTick(&w,.1f); }
 assert(MC_Get(&w,10,3,11)==MC_BUTTON);
 /* Cached circuits must refresh on edits, crossing the scan window, and
    button expiry, while an unchanged powered circuit remains powered. */
 scene(&w);MC_Set(&w,10,3,11,MC_LEVER_ON);MC_Set(&w,11,3,11,MC_DUST);MC_Set(&w,12,3,11,MC_LAMP);
 MC_RedstoneRefresh(&w);assert(MC_Get(&w,12,3,11)==MC_LAMP_ON);
 tick(&w,20);assert(MC_Get(&w,12,3,11)==MC_LAMP_ON);
 MC_Set(&w,10,3,11,MC_LEVER);tick(&w,3);assert(MC_Get(&w,12,3,11)==MC_LAMP);
 /* A loaded button has no runtime pulse: it must not act as a source
    during the first refresh, otherwise the cache would retain ghost power. */
 MC_Set(&w,10,3,11,MC_BUTTON_ON);MC_Set(&w,11,3,11,MC_DUST_ON);MC_Set(&w,12,3,11,MC_LAMP_ON);
 MC_RedstoneRefresh(&w);assert(MC_Get(&w,10,3,11)==MC_BUTTON);
 assert(MC_Get(&w,11,3,11)==MC_DUST&&MC_Get(&w,12,3,11)==MC_LAMP);
 /* A powered map state outside the old scan window is cleared on arrival. */
 MC_Set(&w,50,3,50,MC_LAMP_ON);MC_RedstoneRefresh(&w);
 assert(MC_Get(&w,50,3,50)==MC_LAMP_ON);w.x=w.z=50.5f;
 tick(&w,3);assert(MC_Get(&w,50,3,50)==MC_LAMP);
 /* Revision wrap must not accidentally match a cached world. */
 w.map_revision=0xffffffffu;MC_Set(&w,50,3,50,MC_LAMP_ON);
 tick(&w,3);assert(MC_Get(&w,50,3,50)==MC_LAMP);
 /* Iron armor is non-stackable, equips from the bag, reduces damage, and
    returns to an empty bag slot when removed. */
 scene(&w);assert(MC_Give(&w,MC_IRON_HELMET,2));assert(w.slots[0].count==1&&w.slots[1].count==1);
 w.bag=1;MC_UIBagTap(&w,30,115);assert(w.armor[0].item==MC_IRON_HELMET&&!w.slots[0].count);
 w.health=20;w.hurt_timer=0;unsigned before=w.armor[0].wear;MC_Hurt(&w,10);
 assert(w.health==11&&w.armor[0].wear==before-1);
 MC_UIBagTap(&w,430,400);assert(!w.armor[0].count&&w.slots[w.selected_slot].item==MC_IRON_HELMET);
 scene(&w);assert(MC_Give(&w,MC_IRON_CHESTPLATE,1));w.bag=1;MC_UIBagTap(&w,30,115);
 assert(w.armor[1].item==MC_IRON_CHESTPLATE);w.armor[1].wear=1;w.health=20;w.hurt_timer=0;MC_Hurt(&w,2);
 assert(!w.armor[1].count&&strstr(w.notice,"ARMOR BROKE"));
 /* Touch layout: all 31 recipes reachable, real slot moves, split and mode. */
 scene(&w);w.bag=1;assert(MC_Give(&w,MC_WOOD,10));MC_UIBagTap(&w,30,115);MC_UIBagTap(&w,78,183);assert(w.slots[9].count==10&&!w.slots[0].count);
 MC_UIBagTap(&w,200,420);assert(w.split);MC_UIBagTap(&w,78,183);MC_UIBagTap(&w,30,115);assert(w.slots[0].count==5&&w.slots[9].count==5);
 for(unsigned i=1;i<=8;i++){MC_UIBagTap(&w,450,85);assert(w.recipe_page==i%9);}assert(MC_RecipeCount()==34);
 assert(MC_RecipeResult(0)==MC_PLANKS&&MC_RecipeIngredient(0,0)==MC_WOOD&&MC_RecipeIngredientCount(0,0)==1);
 assert(MC_RecipeResult(33)==MC_BREAD&&MC_RecipeIngredient(33,0)==MC_WHEAT&&MC_RecipeIngredientCount(33,0)==3);
 scene(&w);assert(MC_Give(&w,MC_WHEAT,3));assert(MC_Craft(&w,33)&&w.items[MC_BREAD]==1);
 assert(MC_FoodHealth(MC_BREAD)==5&&MC_FoodHunger(MC_BREAD)==8);
 assert(MC_Give(&w,MC_RAW_MEAT,1));MC_Set(&w,10,3,10,MC_FURNACE);
 w.furnaces[0]=(MC_Furnace){11,4,11,{MC_RAW_MEAT,1,0},{MC_COAL,1,0},{0},0,0};tick(&w,56);
 assert(w.furnaces[0].output.item==MC_COOKED_MEAT&&MC_FoodHunger(MC_COOKED_MEAT)==10);
 assert(MC_Give(&w,MC_WOOD,10));
 assert(MC_IsBlock(MC_BIRCH_WOOD)&&MC_IsBlock(MC_DARK_PLANKS));
 /* The former attack button area is now scene input; weapon taps are handled
    by voxel_game.c so the right side only keeps the essential controls. */
 assert(MC_RecipeIngredient(0,1)==0&&MC_UIRole(720,170)==MC_TOUCH_LOOK&&
        MC_UIRole(720,240)==MC_TOUCH_SNEAK&&MC_UIRole(720,300)==MC_TOUCH_LOOK);
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
 uint16_t action_pixels[7][100*8];const unsigned action_items[]={MC_SWORD,MC_BOW,MC_WHEAT,MC_DIRT,MC_APPLE,MC_BREAD,MC_COOKED_MEAT};
 for(unsigned a=0;a<7;a++) {
   w.slots[0]=(MC_Stack){action_items[a],1,0};MC_SyncInventory(&w);
   memset(fb,0,800*480*sizeof(*fb));MC_UIDraw(fb,&w,0,15,67,"TEST",1,0,0);
   for(int yy=0;yy<8;yy++)memcpy(&action_pixels[a][yy*100],&fb[(288+yy)*800+690],100*sizeof(*fb));
 }
 for(unsigned a=1;a<7;a++)assert(memcmp(action_pixels[0],action_pixels[a],sizeof(action_pixels[0]))!=0);
 /* The crop hint responds to irrigation without another action button. */
 scene(&w);MC_Set(&w,10,3,13,MC_FARMLAND);MC_Set(&w,10,4,13,MC_WHEAT_CROP);w.pitch=.25f;
 w.growers[0]=(MC_Grower){11,5,14,30};w.target_valid=MC_Pick(&w,400,240,&w.target);
 assert(w.target_valid&&w.target.block==MC_WHEAT_CROP);
 memset(fb,0,800*480*sizeof(*fb));MC_UIDraw(fb,&w,0,15,67,"TEST",1,0,0);
 uint16_t dry_hint[220*8];
 for(int yy=0;yy<8;yy++)memcpy(&dry_hint[yy*220],&fb[(309+yy)*800+290],220*sizeof(*fb));
 MC_Set(&w,14,3,13,MC_WATER);
 memset(fb,0,800*480*sizeof(*fb));MC_UIDraw(fb,&w,0,15,67,"TEST",1,0,0);
 int hint_changed=0;
 for(int yy=0;yy<8;yy++)if(memcmp(&dry_hint[yy*220],&fb[(309+yy)*800+290],220*sizeof(*fb)))hint_changed=1;
 assert(hint_changed);
 for(int k=0;k<6;k++){w.bag=k>0;MC_SetMode(&w,k==5);w.open_chest=k==2?0:255;w.open_furnace=k==3?0:255;MC_UIDraw(fb,&w,0,15,67,"TEST",1,0,0);}
 /* Only coordinates/FPS/frame cost occupy the compact top line.
    Former status/log rows remain transparent. */
 for(unsigned i=0;i<800*480;i++)fb[i]=0x4a69;
 w.bag=0;MC_UIDraw(fb,&w,0,5,999,"DIAGNOSTIC TEXT",0,0,0);
 for(int y=39;y<70;y++)for(int x=10;x<588;x++)assert(fb[y*800+x]==0x4a69);
 uint32_t metrics_crc=0;
 for(int y=8;y<38;y++)metrics_crc^=MC_Crc32(&fb[y*800+10],578*2);
 for(unsigned i=0;i<800*480;i++)fb[i]=0x4a69;
 MC_UIDraw(fb,&w,0,5,999,"OTHER LOG CONTENT",1,0,0);
 uint32_t same_metrics_crc=0;
 for(int y=8;y<38;y++)same_metrics_crc^=MC_Crc32(&fb[y*800+10],578*2);
 assert(metrics_crc==same_metrics_crc);
 for(unsigned i=0;i<800*480;i++)fb[i]=0x4a69;
 MC_UIDraw(fb,&w,0,30,33,"OTHER LOG CONTENT",1,0,0);
 uint32_t new_metrics_crc=0;
 for(int y=8;y<38;y++)new_metrics_crc^=MC_Crc32(&fb[y*800+10],578*2);
 assert(metrics_crc!=new_metrics_crc);

 /* Saving feedback changes only the existing button, in the world and bag.
    The old log area remains transparent for every success/error result. */
 uint16_t *idle_frame=malloc(800*480*sizeof(*idle_frame));assert(idle_frame);
 for(unsigned bag=0;bag<2;bag++) {
   uint32_t button_crc[MC_SAVE_VERIFY_FAILED+1];w.bag=(uint8_t)bag;
   for(unsigned result=MC_SAVE_IDLE;result<=MC_SAVE_VERIFY_FAILED;result++) {
     MC_NotifySave(&w,result);for(unsigned i=0;i<800*480;i++)fb[i]=0x4a69;
     MC_UIDraw(fb,&w,0,15,67,"INVISIBLE LOG",1,0,0);
     if(result==MC_SAVE_IDLE)memcpy(idle_frame,fb,800*480*sizeof(*fb));
     uint32_t crc=0;
     for(int y=0;y<480;y++)for(int x=0;x<800;x++)
       if(x<700 || x>=790 || y<8 || y>=62)assert(fb[y*800+x]==idle_frame[y*800+x]);
     for(int y=8;y<62;y++)crc=(crc<<1|crc>>31)^MC_Crc32(&fb[y*800+700],90*2);
     button_crc[result]=crc;
     for(unsigned previous=0;previous<result;previous++)assert(crc!=button_crc[previous]);
   }
 }
 free(idle_frame);
 MC_NotifySave(&w,MC_SAVE_OK);MC_SystemsTick(&w,2.75f);assert(w.save_feedback==MC_SAVE_OK);
 MC_SystemsTick(&w,.3f);assert(w.save_feedback==MC_SAVE_IDLE);
 MC_NotifySave(&w,MC_SAVE_WRITE_FAILED);MC_SystemsTick(&w,5.5f);assert(w.save_feedback==MC_SAVE_WRITE_FAILED);
 MC_SystemsTick(&w,1);assert(w.save_feedback==MC_SAVE_IDLE);
 MC_NotifySave(&w,MC_SAVE_BUSY);MC_SystemsTick(&w,10);assert(w.save_feedback==MC_SAVE_BUSY);
 MC_NotifySave(&w,255);assert(w.save_feedback==MC_SAVE_IDLE&&!w.save_feedback_time);

 const uint8_t banks[2]={1,0};
 MC_MenuDraw(fb,banks,0,0,20261021,0,"TEST");
 MC_MenuDraw(fb,banks,1,1,20261021,1,"TEST");
 for(int k=0;k<64;k++)assert(fb[800*480+k]==0xa55a);
 free(fb);free(w.blocks);puts("minicraft slots / crafting / containers / furnace / growth / survival / UI: OK");return 0;
}
