#include "minicraft.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
static const char *names[MC_ITEM_COUNT]={"AIR","GRASS","DIRT","STONE","SAND","LOG","LEAVES","BEDROCK","WATER","COBBLE","PLANKS","WORKBENCH","FURNACE","COAL ORE","IRON ORE","GLASS","BRICKS","TORCH","STICKS","COAL","IRON","WOOD PICK","STONE PICK","IRON PICK","APPLE","AXE","SHOVEL","SWORD","CHEST","BED","SAPLING","DOOR","DOOR OPEN","STONE SLAB","SLAB TOP","FARMLAND","WHEAT CROP","WHEAT MATURE","WHEAT SEEDS","WHEAT","FENCE","STONE STAIRS","STONE STAIRS EAST","STONE STAIRS SOUTH","STONE STAIRS WEST","RAW MEAT","REDSTONE DUST","REDSTONE DUST ON","REDSTONE LAMP","REDSTONE LAMP ON","LEVER","LEVER ON","BUTTON","BUTTON ON","BOW","ARROW","IRON HELMET","IRON CHESTPLATE","IRON LEGGINGS","IRON BOOTS","BIRCH LOG","BIRCH PLANKS","DARK LOG","DARK PLANKS","BREAD","COOKED MEAT","DOUBLE STONE SLAB"};
/* Designators prevent an appended item from shifting unrelated drop colors. */
static const uint16_t colors[MC_ITEM_COUNT]={
  [MC_GRASS]=0x6cc6,[MC_DIRT]=0x8b07,[MC_STONE]=0x8410,[MC_SAND]=0xd66f,
  [MC_WOOD]=0x8b45,[MC_LEAVES]=0x3b85,[MC_BEDROCK]=0x3186,[MC_WATER]=0x3397,
  [MC_COBBLE]=0x7390,[MC_PLANKS]=0xb48a,[MC_TABLE]=0x9347,[MC_FURNACE]=0x630c,
  [MC_COAL_ORE]=0x4208,[MC_IRON_ORE]=0xb3ac,[MC_GLASS]=0x9f7f,[MC_BRICK]=0xb2a8,
  [MC_TORCH]=0xff20,[MC_STICK]=0x9b05,[MC_COAL]=0x2104,[MC_IRON]=0xc638,
  [MC_WOOD_PICK]=0xa366,[MC_STONE_PICK]=0x94b2,[MC_IRON_PICK]=0xdefb,[MC_APPLE]=0xe924,
  [MC_AXE]=0x9b05,[MC_SHOVEL]=0x9b05,[MC_SWORD]=0xdefb,[MC_CHEST]=0x9347,
  [MC_BED]=0x6cc6,[MC_SAPLING]=0x3b85,[MC_DOOR]=0xa365,[MC_DOOR_OPEN]=0x9b05,
  [MC_SLAB]=0x8c71,[MC_SLAB_TOP]=0x8c71,[MC_FARMLAND]=0x6a64,[MC_WHEAT_CROP]=0x4d8b,
  [MC_WHEAT_MATURE]=0xf6a0,[MC_WHEAT_SEEDS]=0x7e24,[MC_WHEAT]=0xe5a4,[MC_FENCE]=0xa365,
  [MC_STAIR]=0x8410,[MC_STAIR_EAST]=0x8410,[MC_STAIR_SOUTH]=0x8410,[MC_STAIR_WEST]=0x8410,
  [MC_RAW_MEAT]=0xc9b0,[MC_DUST]=0xf800,[MC_DUST_ON]=0xf800,[MC_LAMP]=0x8410,
  [MC_LAMP_ON]=0xffe0,[MC_LEVER]=0x9b05,[MC_LEVER_ON]=0xffc0,[MC_BUTTON]=0xb48a,
  [MC_BUTTON_ON]=0xffe0,[MC_BOW]=0x8a45,[MC_ARROW]=0xe7be,
  [MC_IRON_HELMET]=0xa514,[MC_IRON_CHESTPLATE]=0xa514,[MC_IRON_LEGGINGS]=0xa514,[MC_IRON_BOOTS]=0xa514,
  [MC_BIRCH_WOOD]=0xd6b6,[MC_BIRCH_PLANKS]=0xc5a2,[MC_DARK_WOOD]=0x5a04,[MC_DARK_PLANKS]=0x7b05,
  [MC_BREAD]=0xdca6,[MC_COOKED_MEAT]=0xa346,[MC_DOUBLE_SLAB]=0x8c71
};
const char *MC_ItemName(unsigned i){return i<MC_ITEM_COUNT?names[i]:"EMPTY";}uint16_t MC_ItemColor(unsigned i){return i<MC_ITEM_COUNT?colors[i]:0;}
void MC_HoldStart(MC_Hold*h,int x,int y,uint32_t n){*h=(MC_Hold){x,y,x,y,n,0};}void MC_HoldMove(MC_Hold*h,int x,int y,uint32_t n){int dx=x-h->anchor_x,dy=y-h->anchor_y;h->x=x;h->y=y;if(dx*dx+dy*dy>144){h->moved=1;h->anchor_x=x;h->anchor_y=y;h->since=n;}}int MC_HoldReady(const MC_Hold*h,uint32_t n){return(uint32_t)(n-h->since)>=450;}
int MC_HoldTap(const MC_Hold*h,uint32_t n){return !h->moved && !MC_HoldReady(h,n);}
int MC_IsBlock(unsigned i) {
  return (i>MC_AIR && i<MC_BLOCK_COUNT) || i==MC_DOUBLE_SLAB || i==MC_CHEST || i==MC_BED || i==MC_SAPLING || i==MC_DOOR || i==MC_DOOR_OPEN || i==MC_SLAB || i==MC_SLAB_TOP || i==MC_FARMLAND || i==MC_WHEAT_CROP || i==MC_WHEAT_MATURE || (i>=MC_FENCE && i<=MC_STAIR_WEST) || (i>=MC_DUST && i<=MC_BUTTON_ON) || (i>=MC_BIRCH_WOOD && i<=MC_DARK_PLANKS);
}
static int map_only(unsigned i){return i==MC_DOUBLE_SLAB||i==MC_FARMLAND||i==MC_WHEAT_CROP||i==MC_WHEAT_MATURE||(i>=MC_STAIR_EAST&&i<=MC_STAIR_WEST)||i==MC_DUST_ON||i==MC_LAMP_ON||i==MC_LEVER_ON||i==MC_BUTTON_ON;}
static uint8_t redstone_off(uint8_t b)
{
  if(b==MC_DUST_ON)return MC_DUST;
  if(b==MC_LAMP_ON)return MC_LAMP;
  if(b==MC_LEVER_ON)return MC_LEVER;
  if(b==MC_BUTTON_ON)return MC_BUTTON;
  return b;
}
static int redstone_kind(uint8_t b){return b>=MC_DUST && b<=MC_BUTTON_ON;}
static int redstone_button_find(const MC_World *w,int x,int y,int z)
{
  for(unsigned i=0;i<MC_REDSTONE_BUTTONS;i++)
    if(w->redstone_button_timer[i]>0 && w->redstone_button_pos[i][0]==(uint16_t)(x+1) &&
       w->redstone_button_pos[i][1]==(uint16_t)(y+1) && w->redstone_button_pos[i][2]==(uint16_t)(z+1)) return (int)i;
  return -1;
}
static void redstone_button_press(MC_World *w,int x,int y,int z)
{
  int i=redstone_button_find(w,x,y,z);
  if(i<0) for(unsigned n=0;n<MC_REDSTONE_BUTTONS;n++) if(w->redstone_button_timer[n]<=0){i=(int)n;break;}
  if(i>=0){w->redstone_button_pos[i][0]=(uint16_t)(x+1);w->redstone_button_pos[i][1]=(uint16_t)(y+1);w->redstone_button_pos[i][2]=(uint16_t)(z+1);w->redstone_button_timer[i]=.5f;MC_Set(w,x,y,z,MC_BUTTON_ON);}
}
/* Recompute a small redstone neighborhood with a fixed queue budget.  Sources
   are levers and active buttons; dust carries power horizontally and lamps
   adjacent to powered dust turn on.  Runtime state is deliberately excluded
   from saves, so a loaded world can rebuild this network cheaply. */
void MC_RedstoneRefresh(MC_World *w)
{
  int cx=(int)w->x,cy=(int)w->y,cz=(int)w->z;
  /* Same blocks and same scan window imply the same circuit. Button expiry
     edits its map state before refresh, and all gameplay edits use MC_Set. */
  if(w->redstone_valid && w->redstone_revision==w->map_revision &&
     w->redstone_center[0]==cx && w->redstone_center[1]==cy && w->redstone_center[2]==cz)return;
  int x0=cx-16,x1=cx+16,y0=cy-8,y1=cy+8,z0=cz-16,z1=cz+16;
  if(x0<1)x0=1;
  if(z0<1)z0=1;
  if(y0<1)y0=1;
  if(x1>=MC_MAP_X-1)x1=MC_MAP_X-2;
  if(z1>=MC_MAP_Z-1)z1=MC_MAP_Z-2;
  if(y1>=MC_MAP_Y)y1=MC_MAP_Y-1;
  int qx[128],qy[128],qz[128],head=0,tail=0;
  for(int y=y0;y<=y1;y++)for(int z=z0;z<=z1;z++)for(int x=x0;x<=x1;x++){
    uint8_t b=w->blocks[((unsigned)y*MC_MAP_Z+(unsigned)z)*MC_MAP_X+(unsigned)x];
    if(b==MC_DUST_ON||b==MC_LAMP_ON)MC_Set(w,x,y,z,redstone_off(b));
    else if(b==MC_BUTTON_ON && redstone_button_find(w,x,y,z)<0){b=MC_BUTTON;MC_Set(w,x,y,z,b);}
    if((b==MC_LEVER_ON || b==MC_BUTTON_ON) && tail<128){qx[tail]=x;qy[tail]=y;qz[tail++]=z;}
  }
  while(head<tail){
    int x=qx[head],y=qy[head],z=qz[head];head++;
    for(int dz=-1;dz<=1;dz++)for(int dx=-1;dx<=1;dx++){
      if((dx&&dz)||(!dx&&!dz))continue;
      int nx=x+dx,nz=z+dz;uint8_t b=MC_Get(w,nx,y,nz);
      if(b==MC_DUST){MC_Set(w,nx,y,nz,MC_DUST_ON);if(tail<128){qx[tail]=nx;qy[tail]=y;qz[tail++]=nz;}}
      else if(b==MC_LAMP)MC_Set(w,nx,y,nz,MC_LAMP_ON);
      else if(b==MC_BUTTON_ON || b==MC_LEVER_ON){/* a source can feed dust on its other side */}
    }
  }
  w->redstone_revision=w->map_revision;
  w->redstone_center[0]=(int16_t)cx;w->redstone_center[1]=(int16_t)cy;w->redstone_center[2]=(int16_t)cz;
  w->redstone_valid=1;
}
int MC_ToolIndex(unsigned i) {
  if(i>=MC_WOOD_PICK && i<=MC_IRON_PICK)return (int)i-MC_WOOD_PICK;
  if(i>=MC_AXE && i<=MC_SWORD)return (int)i-MC_AXE+3;
  if(i==MC_BOW)return 6;
  return -1;
}
unsigned MC_ToolLife(unsigned i) {
  static const uint16_t life[MC_TOOLS]={60,132,250,90,90,90,160};
  int t=MC_ToolIndex(i);return t<0?0:life[t];
}
int MC_ArmorIndex(unsigned item) {
  if(item>=MC_IRON_HELMET && item<=MC_IRON_BOOTS)return (int)item-(int)MC_IRON_HELMET;
  return -1;
}
unsigned MC_ArmorLife(unsigned item) {
  static const uint16_t life[MC_ARMOR_SLOTS]={80,120,100,80};
  int slot=MC_ArmorIndex(item);return slot<0?0:life[slot];
}
unsigned MC_ArmorPoints(const MC_World *w) {
  static const uint8_t points[MC_ARMOR_SLOTS]={1,3,2,1};
  unsigned total=0;for(unsigned i=0;i<MC_ARMOR_SLOTS;i++)if(w->armor[i].count)total+=points[i];
  return total;
}
unsigned MC_XpForLevel(unsigned level) {
  if(level>MC_MAX_LEVEL)level=MC_MAX_LEVEL;
  return 5u*level*level+10u*level;
}
unsigned MC_XpLevel(const MC_World *w) {
  unsigned xp=w?w->experience:0,level=0;
  unsigned cap=MC_XpForLevel(MC_MAX_LEVEL);
  if(xp>cap)xp=cap;
  while(level<MC_MAX_LEVEL && xp>=MC_XpForLevel(level+1))level++;
  return level;
}
unsigned MC_XpProgress(const MC_World *w) {
  unsigned level=MC_XpLevel(w),base=MC_XpForLevel(level);
  return w&&w->experience>base?w->experience-base:0;
}
unsigned MC_XpToNext(const MC_World *w) {
  unsigned level=MC_XpLevel(w);
  return level>=MC_MAX_LEVEL?0:MC_XpForLevel(level+1)-MC_XpForLevel(level);
}
unsigned MC_AddExperience(MC_World *w,unsigned amount) {
  if(!w||w->creative||!amount)return 0;
  unsigned old=w->experience,old_level=MC_XpLevel(w),cap=MC_XpForLevel(MC_MAX_LEVEL);
  if(old>=cap)return 0;
  if(amount>cap-old)amount=cap-old;
  w->experience=old+amount;
  unsigned new_level=MC_XpLevel(w);
  w->level=(uint8_t)new_level;
  if(new_level>old_level) {
    w->level_flash=.9f;
    w->notice="LEVEL UP";
  }
  return amount;
}
unsigned MC_AttackDamage(const MC_World *w,unsigned weapon) {
  unsigned damage=weapon==MC_SWORD?6:weapon==MC_AXE?5:weapon==MC_IRON_PICK?4:MC_ToolIndex(weapon)>=0?3:1;
  unsigned level=MC_XpLevel(w);
  if(level>=5)damage++;
  if(level>=15)damage++;
  return damage;
}
static unsigned capacity(const MC_Stack *slots,unsigned len,unsigned id) {
  unsigned room=0,limit=(MC_ToolIndex(id)>=0||MC_ArmorIndex(id)>=0)?1:64;
  for(unsigned i=0;i<len;i++) {
    if(!slots[i].count)room+=limit;
    else if(slots[i].item==id && limit==64)room+=64-slots[i].count;
  }
  return room;
}
static int add_stack(MC_Stack *slots,unsigned len,MC_Stack src) {
  if(capacity(slots,len,src.item)<src.count)return 0;
  for(unsigned i=0;i<len && src.count;i++) {
    if(slots[i].count && slots[i].item==src.item && MC_ToolIndex(src.item)<0) {
      unsigned n=64-slots[i].count;if(n>src.count)n=src.count;
      slots[i].count+=n;src.count-=n;
    }
  }
  for(unsigned i=0;i<len && src.count;i++)if(!slots[i].count) {
    unsigned n=(MC_ToolIndex(src.item)>=0||MC_ArmorIndex(src.item)>=0)?1:src.count>64?64:src.count;
    slots[i]=src;slots[i].count=(uint8_t)n;src.count-=n;
  }
  return 1;
}
void MC_SyncInventory(MC_World *w) {
  memset(w->items,0,sizeof(w->items));memset(w->durability,0,sizeof(w->durability));
  for(unsigned i=0;i<MC_SLOTS;i++) {
    MC_Stack *s=&w->slots[i];
    if(s->count && s->item<MC_ITEM_COUNT) {
      w->items[s->item]+=s->count;int t=MC_ToolIndex(s->item);
      if(t>=0 && !w->durability[t])w->durability[t]=s->wear;
    }
  }
  for(unsigned i=0;i<MC_HOTBAR;i++)w->hotbar[i]=w->creative?w->creative_bar[i]:w->slots[i].item;
  if(!w->creative && !w->items[w->equipped])w->equipped=0;
}
int MC_Give(MC_World *w,unsigned id,unsigned n) {
  if(!id || id>=MC_ITEM_COUNT || id==MC_BEDROCK || id==MC_WATER || map_only(id) || !n || n>255)return 0;
  MC_Stack src={(uint8_t)id,(uint8_t)n,(uint16_t)MC_ToolLife(id)};
  if(MC_ArmorIndex(id)>=0)src.wear=(uint16_t)MC_ArmorLife(id);
  if(!add_stack(w->slots,MC_SLOTS,src))return 0;
  MC_SyncInventory(w);return 1;
}
int MC_EquipArmor(MC_World *w,unsigned inventory_slot) {
  if(inventory_slot>=MC_SLOTS)return 0;
  MC_Stack *src=&w->slots[inventory_slot];int slot=MC_ArmorIndex(src->item);
  if(slot<0||src->count!=1)return 0;
  MC_Stack old=w->armor[slot];w->armor[slot]=*src;
  if(old.count)*src=old;else memset(src,0,sizeof(*src));
  MC_SyncInventory(w);w->notice="ARMOR EQUIPPED";return 1;
}
int MC_UnequipArmor(MC_World *w,unsigned armor_slot) {
  if(armor_slot>=MC_ARMOR_SLOTS||!w->armor[armor_slot].count)return 0;
  unsigned dst=MC_SLOTS;
  if(w->selected_slot<MC_SLOTS&&!w->slots[w->selected_slot].count)dst=w->selected_slot;
  if(dst==MC_SLOTS)for(unsigned i=0;i<MC_SLOTS;i++)if(!w->slots[i].count){dst=i;break;}
  if(dst==MC_SLOTS){w->notice="BAG FULL  ARMOR STAYS ON";return 0;}
  w->slots[dst]=w->armor[armor_slot];memset(&w->armor[armor_slot],0,sizeof(MC_Stack));
  w->selected_slot=(uint8_t)dst;MC_SyncInventory(w);w->notice="ARMOR REMOVED";return 1;
}
static void remove_item(MC_World *w,unsigned id,unsigned n) {
  for(unsigned i=0;i<MC_SLOTS && n;i++)if(w->slots[i].item==id) {
    unsigned a=w->slots[i].count;if(a>n)a=n;
    w->slots[i].count-=a;n-=a;
    if(!w->slots[i].count)memset(&w->slots[i],0,sizeof(MC_Stack));
  }
}
int MC_MoveStack(MC_Stack *a,MC_Stack *b,int split) {
  if(a==b || !a->count)return 0;
  if(!b->count) {
    unsigned n=split?(a->count+1)/2:a->count;
    *b=*a;b->count=(uint8_t)n;a->count-=n;
  } else if(a->item==b->item && !a->wear && !b->wear) {
    unsigned room=64-b->count,n=split?1:a->count;
    if(n>room)n=room;
    if(!n)return 0;
    a->count-=n;b->count+=n;
  } else if(!split) {MC_Stack tmp=*b;*b=*a;*a=tmp;}
  else return 0;
  if(!a->count)memset(a,0,sizeof(*a));
  return 1;
}
void MC_SetMode(MC_World *w,int creative) {
  w->creative=(uint8_t)!!creative;
  w->bag_page=0;
  if(w->creative){w->hunger=20;w->hunger_clock=w->starvation_clock=0;}
  MC_SyncInventory(w);w->moving_slot=255;
}
void MC_CloseBag(MC_World *w) {
  w->bag=0;w->open_chest=w->open_furnace=255;w->moving_slot=255;
  w->recipe_requires=0;
}
void MC_NewPlayer(MC_World *w,int legacy) {
  w->save_feedback=MC_SAVE_IDLE;w->save_feedback_time=0;
  memset(w->items,0,sizeof(w->items));memset(w->slots,0,sizeof(w->slots));
  memset(w->armor,0,sizeof(w->armor));
  memset(w->chest_blocks,0,sizeof(w->chest_blocks));memset(w->chest_map,0,sizeof(w->chest_map));
  memset(w->furnaces,0,sizeof(w->furnaces));memset(w->growers,0,sizeof(w->growers));
  memset(w->reserve,0,sizeof(w->reserve));memset(w->spawn,0,sizeof(w->spawn));memset(w->durability,0,sizeof(w->durability));
  memset(w->reserve_wear,0,sizeof(w->reserve_wear));
  const uint8_t bar[8]={MC_WOOD,MC_DIRT,MC_COBBLE,MC_PLANKS,MC_TABLE,MC_TORCH,MC_GLASS,MC_BOW};
  memcpy(w->creative_bar,bar,8);w->slot=0;w->creative=(uint8_t)legacy;
  w->health=20;w->air=10;w->hunger=20;w->equipped=0;w->time=90;
  w->experience=0;w->level=0;w->level_flash=0;
  w->hunger_clock=w->starvation_clock=0;
  w->fall_speed=w->breath=w->hurt_timer=0;w->target_valid=0;w->mining=0;
  w->bag=w->recipe_page=w->change_world=w->bag_page=w->recipe_requires=w->split=0;
  w->selected_slot=0;w->open_chest=w->open_furnace=w->moving_slot=255;
  w->sneak=0;
  memset(w->mobs,0,sizeof(w->mobs));memset(w->drops,0,sizeof(w->drops));memset(w->projectiles,0,sizeof(w->projectiles));w->mob_rng=w->seed^0x9e3779b9u;if(!w->mob_rng)w->mob_rng=1;
  w->mob_target=255;w->mob_clock=w->spawn_clock=w->attack_cooldown=w->attack_flash=w->shoot_cooldown=0;
  w->gravity_clock=0;w->gravity_cursor=0;
  w->fluid_clock=0;w->fluid_cursor=0;
  w->automation_clock=0;w->crop_clock=0;
  w->map_revision=w->redstone_revision=0;w->redstone_valid=0;
  memset(w->redstone_center,0,sizeof(w->redstone_center));
  w->redstone_clock=0;memset(w->redstone_button_pos,0,sizeof(w->redstone_button_pos));memset(w->redstone_button_timer,0,sizeof(w->redstone_button_timer));
  memset(w->mob_fed,0,sizeof(w->mob_fed));memset(w->mob_fed_age,0,sizeof(w->mob_fed_age));w->breed_cooldown=0;
  MC_SyncInventory(w);
  w->notice=legacy?"OLD WORLD  CREATIVE MODE":"HOLD BLOCK TO MINE  BAG TO CRAFT";
}
void MC_SelectItem(MC_World *w,unsigned item) {
  if(!item || item>=MC_ITEM_COUNT || item==MC_BEDROCK || item==MC_WATER || item==MC_SLAB_TOP || map_only(item))return;
  if(w->creative) {w->creative_bar[w->slot]=(uint8_t)item;MC_SyncInventory(w);}
  else for(unsigned i=0;i<MC_HOTBAR;i++)if(w->slots[i].item==item && w->slots[i].count) {w->slot=(uint8_t)i;break;}
  if(MC_ToolIndex(item)>=0 && (w->creative || w->items[item]))w->equipped=(uint8_t)item;
}
typedef struct{uint8_t out,n,a,na,b,nb,station;}R;
static const R recipes[]={{MC_PLANKS,4,MC_WOOD,1,0,0,0},{MC_STICK,4,MC_PLANKS,2,0,0,0},{MC_TABLE,1,MC_PLANKS,4,0,0,0},{MC_WOOD_PICK,1,MC_PLANKS,3,MC_STICK,2,MC_TABLE},{MC_STONE_PICK,1,MC_COBBLE,3,MC_STICK,2,MC_TABLE},{MC_FURNACE,1,MC_COBBLE,8,0,0,MC_TABLE},{MC_COAL,1,MC_WOOD,1,MC_PLANKS,1,MC_FURNACE},{MC_TORCH,4,MC_COAL,1,MC_STICK,1,0},{MC_IRON,1,MC_IRON_ORE,1,MC_COAL,1,MC_FURNACE},{MC_IRON_PICK,1,MC_IRON,3,MC_STICK,2,MC_TABLE},{MC_GLASS,4,MC_SAND,4,MC_COAL,1,MC_FURNACE},{MC_BRICK,4,MC_COBBLE,4,MC_COAL,1,MC_FURNACE},{MC_AXE,1,MC_PLANKS,3,MC_STICK,2,MC_TABLE},{MC_SHOVEL,1,MC_PLANKS,1,MC_STICK,2,MC_TABLE},{MC_SWORD,1,MC_PLANKS,2,MC_STICK,1,MC_TABLE},{MC_CHEST,1,MC_PLANKS,8,0,0,MC_TABLE},{MC_BED,1,MC_PLANKS,3,MC_SAPLING,3,MC_TABLE},{MC_DOOR,1,MC_PLANKS,6,0,0,MC_TABLE},{MC_SLAB,6,MC_COBBLE,3,0,0,MC_TABLE},{MC_FENCE,3,MC_PLANKS,4,MC_STICK,2,MC_TABLE},{MC_STAIR,4,MC_COBBLE,6,0,0,MC_TABLE},{MC_DUST,4,MC_COAL,1,MC_IRON,1,MC_TABLE},{MC_LAMP,1,MC_DUST,4,MC_GLASS,4,MC_TABLE},{MC_LEVER,1,MC_STICK,1,MC_COBBLE,1,MC_TABLE},{MC_BUTTON,1,MC_PLANKS,1,0,0,MC_TABLE},{MC_BOW,1,MC_PLANKS,3,MC_STICK,3,MC_TABLE},{MC_ARROW,4,MC_STICK,1,MC_IRON,1,MC_TABLE},{MC_IRON_HELMET,1,MC_IRON,5,0,0,MC_TABLE},{MC_IRON_CHESTPLATE,1,MC_IRON,8,0,0,MC_TABLE},{MC_IRON_LEGGINGS,1,MC_IRON,7,0,0,MC_TABLE},{MC_IRON_BOOTS,1,MC_IRON,4,0,0,MC_TABLE},{MC_BIRCH_PLANKS,4,MC_BIRCH_WOOD,1,0,0,0},{MC_DARK_PLANKS,4,MC_DARK_WOOD,1,0,0,0},{MC_BREAD,1,MC_WHEAT,3,0,0,0}};
static const char*cost[]={"1 LOG - 4 PLANKS","2 PLANKS - 4 STICKS","4 PLANKS","3 PLANKS 2 STICKS","3 COBBLE 2 STICKS","8 COBBLE","1 LOG 1 PLANK","1 COAL 1 STICK","1 ORE 1 COAL","3 IRON 2 STICKS","4 SAND 1 COAL","4 COBBLE 1 COAL","3 PLANKS 2 STICKS","1 PLANK 2 STICKS","2 PLANKS 1 STICK","8 PLANKS","3 PLANKS 3 SAPLINGS","6 PLANKS","3 COBBLE - 6 STONE SLABS","4 PLANKS 2 STICKS - 3 FENCES","6 COBBLE - 4 STONE STAIRS","1 COAL 1 IRON - 4 DUST","4 DUST 4 GLASS","1 STICK 1 COBBLE","1 PLANK","3 PLANKS 3 STICKS","1 STICK 1 IRON - 4 ARROWS","5 IRON","8 IRON","7 IRON","4 IRON","1 BIRCH LOG - 4 BIRCH PLANKS","1 DARK LOG - 4 DARK PLANKS","3 WHEAT - 1 BREAD"};
unsigned MC_RecipeCount(void){return sizeof(recipes)/sizeof(recipes[0]);}const char*MC_RecipeName(unsigned r){return r<MC_RecipeCount()?MC_ItemName(recipes[r].out):"";}const char*MC_RecipeCost(unsigned r){return r<MC_RecipeCount()?cost[r]:"";}
unsigned MC_RecipeResult(unsigned r){return r<MC_RecipeCount()?recipes[r].out:0;}
unsigned MC_RecipeIngredient(unsigned r,unsigned p){if(r>=MC_RecipeCount()||p>1)return 0;return p?recipes[r].b:recipes[r].a;}
unsigned MC_RecipeIngredientCount(unsigned r,unsigned p){if(r>=MC_RecipeCount()||p>1)return 0;return p?recipes[r].nb:recipes[r].na;}
static int near(const MC_World*w,uint8_t b){for(int y=(int)w->y-2;y<=(int)w->y+2;y++)for(int z=(int)w->z-3;z<=(int)w->z+3;z++)for(int x=(int)w->x-3;x<=(int)w->x+3;x++)if(MC_Get(w,x,y,z)==b)return 1;return 0;}
static unsigned material_count(const MC_World *w,unsigned id){
  if(id==MC_PLANKS)return w->items[MC_PLANKS]+w->items[MC_BIRCH_PLANKS]+w->items[MC_DARK_PLANKS];
  if(id==MC_WOOD)return w->items[MC_WOOD]+w->items[MC_BIRCH_WOOD]+w->items[MC_DARK_WOOD];
  return w->items[id];
}
static void remove_material(MC_World *w,unsigned id,unsigned n){
  unsigned ids[3]={id,MC_BIRCH_PLANKS,MC_DARK_PLANKS};
  if(id==MC_WOOD){ids[1]=MC_BIRCH_WOOD;ids[2]=MC_DARK_WOOD;}
  else if(id!=MC_PLANKS){remove_item(w,id,n);return;}
  for(unsigned i=0;i<3&&n;i++){unsigned take=w->items[ids[i]]<n?w->items[ids[i]]:n;if(take){remove_item(w,ids[i],take);n-=take;}}
}
const char*MC_RecipeStatus(const MC_World*w,unsigned r){if(r>=MC_RecipeCount())return "";R c=recipes[r];if(c.station==MC_FURNACE)return "USE FURNACE INPUT AND FUEL";if(!w->creative&&c.station&&!near(w,c.station))return "NEEDS WORKBENCH";return w->creative||(material_count(w,c.a)>=c.na&&(!c.nb||material_count(w,c.b)>=c.nb))?"READY":"NEED MATERIALS";}
int MC_Craft(MC_World *w,unsigned r) {
  if(r>=MC_RecipeCount())return 0;
  R c=recipes[r];
  /* Smelting has its own persistent input/fuel/output and timer. */
  if(c.station==MC_FURNACE){w->notice="OPEN FURNACE  ADD INPUT AND FUEL";return 0;}
  if(!w->creative && c.station && !near(w,c.station)){w->notice="PLACE WORKBENCH NEARBY";return 0;}
  if(!w->creative && (material_count(w,c.a)<c.na || (c.nb && material_count(w,c.b)<c.nb))){w->notice="NEED MORE MATERIALS";return 0;}
  MC_Stack before[MC_SLOTS];memcpy(before,w->slots,sizeof(before));
  if(!w->creative){remove_material(w,c.a,c.na);if(c.nb)remove_material(w,c.b,c.nb);}
  if(!MC_Give(w,c.out,c.n)) {
    memcpy(w->slots,before,sizeof(before));MC_SyncInventory(w);w->notice="BAG FULL  NO MATERIALS USED";return 0;
  }
  if(MC_ToolIndex(c.out)>=0){w->equipped=c.out;for(unsigned i=0;i<MC_HOTBAR;i++)if(w->slots[i].item==c.out){w->slot=i;break;}}
  else MC_SelectItem(w,c.out);
  MC_AddExperience(w,1);
  w->notice="CRAFTED  CHECK INVENTORY";return 1;
}
static int chest_at(const MC_World *w,int x,int y,int z) {
  for(unsigned i=0;i<MC_CHESTS;i++)if(w->chest_map[i][0]==x+1 && w->chest_map[i][1]==y+1 && w->chest_map[i][2]==z+1)return (int)i;
  return -1;
}
static int furnace_at(const MC_World *w,int x,int y,int z) {
  for(unsigned i=0;i<MC_FURNACES;i++)if(w->furnaces[i].x==x+1 && w->furnaces[i].y==y+1 && w->furnaces[i].z==z+1)return (int)i;
  return -1;
}
int MC_DropSpawn(MC_World *w,unsigned item,unsigned count,float x,float y,float z) {
  if(!item || item>=MC_ITEM_COUNT || !count)return 0;
  /* Preflight the bounded pool: a failed multi-item drop must not partially
     mutate stacks while mining leaves the source block intact. */
  unsigned room=0;
  for(unsigned i=0;i<MC_DROPS;i++) {
    const MC_Drop *d=&w->drops[i];
    if(!d->count)room+=64;
    else if(d->item==item && fabsf(d->x-x)<.7f && fabsf(d->z-z)<.7f && fabsf(d->y-y)<.9f)room+=64-d->count;
  }
  if(room<count)return 0;
  for(unsigned i=0;i<MC_DROPS && count;i++) {
    MC_Drop *d=&w->drops[i];
    if(d->item==item && d->count && fabsf(d->x-x)<.7f && fabsf(d->z-z)<.7f && fabsf(d->y-y)<.9f) {
      unsigned room=64-d->count,n=count<room?count:room;d->count+=(uint8_t)n;count-=n;
    }
  }
  for(unsigned i=0;i<MC_DROPS && count;i++)if(!w->drops[i].count) {
    unsigned n=count>64?64:count;
    w->drops[i]=(MC_Drop){x,y,z,1.6f,0,(float)(i&3)*1.57f,(uint8_t)item,(uint8_t)n};count-=n;
  }
  return count==0;
}
static int fuel_time(unsigned item){return item==MC_COAL?40:(item==MC_WOOD||item==MC_BIRCH_WOOD||item==MC_DARK_WOOD)?15:(item==MC_PLANKS||item==MC_BIRCH_PLANKS||item==MC_DARK_PLANKS)?10:0;}
static unsigned smelt(unsigned item){return item==MC_IRON_ORE?MC_IRON:item==MC_SAND?MC_GLASS:item==MC_COBBLE?MC_BRICK:item==MC_WOOD?MC_COAL:item==MC_RAW_MEAT?MC_COOKED_MEAT:0;}
/* A chest touching a furnace becomes a small, deterministic automation link.
   It feeds one input/fuel stack when the corresponding furnace slot has room
   and receives finished output.  The scan is limited to six furnaces and six
   neighbouring cells, and runs at 4 Hz, so it adds no whole-world work. */
static void automation_tick(MC_World *w,float dt)
{
  w->automation_clock+=dt;
  if(w->automation_clock<.25f)return;
  w->automation_clock=fmodf(w->automation_clock,.25f);
  static const int dirs[6][3]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
  for(unsigned fi=0;fi<MC_FURNACES;fi++) {
    MC_Furnace *f=&w->furnaces[fi];
    if(!f->x)continue;
    int fx=(int)f->x-1,fy=(int)f->y-1,fz=(int)f->z-1;
    for(unsigned di=0;di<6;di++) {
      int ci=chest_at(w,fx+dirs[di][0],fy+dirs[di][1],fz+dirs[di][2]);
      if(ci<0)continue;
      MC_Stack *chest=w->chest_blocks[ci];
      if(f->input.count<64) {
        for(unsigned si=0;si<MC_CHEST_SLOTS;si++) {
          MC_Stack *s=&chest[si];unsigned out=smelt(s->item);
          if(!s->count||!out)continue;
          if(f->input.count && f->input.item!=s->item)continue;
          unsigned n=64-f->input.count;if(n>s->count)n=s->count;
          if(!f->input.count)f->input=(MC_Stack){s->item,0,0};
          f->input.count+=(uint8_t)n;s->count-=(uint8_t)n;if(!s->count)memset(s,0,sizeof(*s));
          break;
        }
      }
      if(f->fuel.count<64) {
        for(unsigned si=0;si<MC_CHEST_SLOTS;si++) {
          MC_Stack *s=&chest[si];if(!s->count||!fuel_time(s->item))continue;
          if(f->fuel.count && f->fuel.item!=s->item)continue;
          unsigned n=64-f->fuel.count;if(n>s->count)n=s->count;
          if(!f->fuel.count)f->fuel=(MC_Stack){s->item,0,0};
          f->fuel.count+=(uint8_t)n;s->count-=(uint8_t)n;if(!s->count)memset(s,0,sizeof(*s));
          break;
        }
      }
      if(f->output.count) {
        for(unsigned si=0;si<MC_CHEST_SLOTS;si++) {
          MC_Stack *s=&chest[si];
          if(s->count && (s->item!=f->output.item || s->count>=64))continue;
          unsigned n=f->output.count;
          if(s->count && n>(unsigned)(64u-s->count))n=(unsigned)(64u-s->count);
          if(!n)continue;
          if(!s->count)*s=(MC_Stack){f->output.item,0,0};
          s->count+=(uint8_t)n;f->output.count-=(uint8_t)n;
          if(!f->output.count)memset(&f->output,0,sizeof(f->output));
          break;
        }
      }
    }
  }
}
static int open_target_at(MC_World *w,float sx,float sy) {
  MC_Hit hit;
  if(!MC_Pick(w,sx,sy,&hit))return 0;
  int x=hit.x,y=hit.y,z=hit.z;
  uint8_t b=hit.block;
  w->moving_slot=255;
  if(b==MC_LEVER || b==MC_LEVER_ON) {
    MC_Set(w,x,y,z,b==MC_LEVER?MC_LEVER_ON:MC_LEVER);MC_RedstoneRefresh(w);
    w->notice=b==MC_LEVER?"LEVER ON":"LEVER OFF";w->target_valid=0;w->mining=0;return 1;
  }
  if(b==MC_BUTTON || b==MC_BUTTON_ON) {
    redstone_button_press(w,x,y,z);MC_RedstoneRefresh(w);
    w->notice="BUTTON PULSE";w->target_valid=0;w->mining=0;return 1;
  }
  if(b==MC_DOOR || b==MC_DOOR_OPEN) {
    if(b==MC_DOOR_OPEN) {
      MC_Set(w,x,y,z,MC_DOOR);
      if(!MC_BodyClear(w,w->x,w->y,w->z)) {
        MC_Set(w,x,y,z,MC_DOOR_OPEN);w->notice="MOVE CLEAR TO CLOSE";return 1;
      }
    } else MC_Set(w,x,y,z,MC_DOOR_OPEN);
    w->notice=b==MC_DOOR?"DOOR OPEN":"DOOR CLOSED";
    w->target_valid=0;w->mining=0;return 1;
  }
  if(b==MC_TABLE){w->bag=1;w->recipe_requires=b;w->open_chest=w->open_furnace=255;return 1;}
  if(b==MC_FURNACE) {
    int fi=furnace_at(w,x,y,z);
    if(fi<0)for(unsigned i=0;i<MC_FURNACES;i++)if(!w->furnaces[i].x){fi=(int)i;w->furnaces[i].x=x+1;w->furnaces[i].y=y+1;w->furnaces[i].z=z+1;break;}
    if(fi<0){w->notice="MAX 6 ACTIVE FURNACES";return 1;}
    w->open_furnace=(uint8_t)fi;w->open_chest=255;w->bag=1;w->notice="SELECT ITEM THEN INPUT OR FUEL";return 1;
  }
  if(b==MC_CHEST) {
    int ci=chest_at(w,x,y,z);if(ci<0)return 0;
    w->open_chest=(uint8_t)ci;w->open_furnace=255;w->bag=1;w->notice="SELECT INVENTORY THEN CHEST SLOT";return 1;
  }
  if(b==MC_BED) {
    for(unsigned i=0;i<MC_MOBS;i++)if(w->mobs[i].health&&!w->mobs[i].type) {
      float dx=w->mobs[i].x-w->x,dy=w->mobs[i].y-w->y,dz=w->mobs[i].z-w->z;
      if(dx*dx+dy*dy+dz*dz<64){w->notice="ENEMIES NEARBY  CANNOT SLEEP";return 1;}
    }
    w->spawn[0]=x+1;w->spawn[1]=y+1;w->spawn[2]=z+1;
    if(w->time>=300 || w->time<25){w->time=90;w->notice="SLEPT  SPAWN POINT SET";}
    else w->notice="SPAWN SET  SLEEP AT NIGHT";
    return 1;
  }
  return 0;
}
unsigned MC_FoodHealth(unsigned item) {
  return item==MC_APPLE||item==MC_BREAD?5:item==MC_RAW_MEAT?4:item==MC_COOKED_MEAT?8:0;
}
unsigned MC_FoodHunger(unsigned item) {
  return item==MC_APPLE?6:item==MC_RAW_MEAT?5:item==MC_BREAD?8:item==MC_COOKED_MEAT?10:0;
}
int MC_UseAt(MC_World *w,float sx,float sy) {
  if(!w->sneak && open_target_at(w,sx,sy))return 1;
  unsigned b=w->hotbar[w->slot];
  if(MC_FoodHunger(b) && (w->health<20 || w->hunger<20) && (w->creative || w->items[b])) {
    if(!w->creative){MC_Stack *s=&w->slots[w->slot];if(!--s->count)memset(s,0,sizeof(*s));}
    MC_SyncInventory(w);w->health=(uint8_t)fminf(20,w->health+MC_FoodHealth(b));
    w->hunger=(uint8_t)fminf(20,w->hunger+MC_FoodHunger(b));w->notice=b==MC_APPLE?"ATE APPLE":b==MC_RAW_MEAT?"ATE RAW MEAT":b==MC_BREAD?"ATE BREAD":"ATE COOKED MEAT";return 1;
  }
  if(b==MC_SHOVEL) {
    MC_Hit h;if(MC_Pick(w,sx,sy,&h) && (h.block==MC_GRASS||h.block==MC_DIRT) && MC_Get(w,h.x,h.y+1,h.z)==MC_AIR) {
      MC_Set(w,h.x,h.y,h.z,MC_FARMLAND);
      if(!w->creative) { MC_Stack *s=&w->slots[w->slot]; if(s->wear && !--s->wear){memset(s,0,sizeof(*s));w->equipped=0;w->notice="SHOVEL BROKE";} MC_SyncInventory(w); }
      w->notice="TILLED SOIL";return 1;
    }
    w->notice="TILL GRASS OR DIRT";return 0;
  }
  if(b==MC_WHEAT_SEEDS) {
    MC_Hit h;if(MC_Pick(w,sx,sy,&h) && h.block==MC_FARMLAND && MC_Get(w,h.x,h.y+1,h.z)==MC_AIR) {
      int gi=-1;for(unsigned i=0;i<MC_GROWERS;i++)if(!w->growers[i].x){gi=(int)i;break;}
      if(gi<0){w->notice="MAX 16 CROPS";return 0;}
      MC_Set(w,h.x,h.y+1,h.z,MC_WHEAT_CROP);w->growers[gi]=(MC_Grower){h.x+1,h.y+2,h.z+1,0};
      if(!w->creative){MC_Stack*s=&w->slots[w->slot];if(!--s->count)memset(s,0,sizeof(*s));MC_SyncInventory(w);}w->notice="WHEAT PLANTED";return 1;
    }
    w->notice="PLANT ON TILLED SOIL";return 0;
  }
  if(MC_ToolIndex(b)>=0){MC_SelectItem(w,b);return 0;}
  return MC_PlaceAt(w,sx,sy);
}
int MC_SceneTap(MC_World *w,float x,float y) {
  if(w->bag)return 0;
  unsigned held=w->hotbar[w->slot];
  if(MC_MobFeed(w,x,y))return 1;
  /* A visible entity takes priority over a block behind it. Otherwise tools
     can operate doors/containers without selecting an empty hand first.
     Sneaking deliberately bypasses block use for building against it. */
  if(MC_PickMob(w,x,y,3.5f,0)>=0) {
    if(held==MC_BOW)MC_Shoot(w,x,y);else MC_Attack(w,x,y);
    return 0;
  }
  if(!w->sneak && open_target_at(w,x,y))return 1;
  if(held==MC_BOW){MC_Shoot(w,x,y);return 0;}
  if(held==MC_SHOVEL) {
    return MC_UseAt(w,x,y);
  }
  if(MC_ToolIndex(held)>=0){MC_Attack(w,x,y);return 0;}
  /* Seeds may be kept in hand through planting, harvesting and replanting. */
  if(held==MC_WHEAT_SEEDS) {
    MC_Hit hit;
    if(MC_Pick(w,x,y,&hit) && (hit.block==MC_WHEAT_CROP || hit.block==MC_WHEAT_MATURE))return 0;
  }
  if(MC_UseAt(w,x,y))return 1;
  return 0;
}
int MC_Use(MC_World *w) { return MC_UseAt(w,400,240); }
int MC_Place(MC_World *w) { return MC_PlaceAt(w,400,240); }
int MC_PlaceAt(MC_World *w,float sx,float sy) {
  unsigned b=w->hotbar[w->slot];
  if(!MC_IsBlock(b) || map_only(b) || b==MC_BEDROCK || b==MC_WATER || b==MC_DOOR_OPEN || b==MC_SLAB_TOP)return 0;
  if(!w->creative && !w->items[b]){w->notice="NO BLOCKS";return 0;}
  int x,y,z,nx,ny,nz,ci=-1,fi=-1,gi=-1;
  MC_Hit hit;
  if(!MC_Pick(w,sx,sy,&hit))return 0;
  x=hit.x;y=hit.y;z=hit.z;nx=hit.nx;ny=hit.ny;nz=hit.nz;
  x+=nx;y+=ny;z+=nz;
  if((b==MC_DUST||b==MC_LEVER||b==MC_BUTTON) &&
     (ny<=0 || !MC_IsSolid(MC_Get(w,x,y-1,z)))) {w->notice="REDSTONE NEEDS SOLID FLOOR";return 0;}
  uint8_t placed=(uint8_t)b;
  if(b==MC_STAIR) {
    float dx=sinf(w->yaw),dz=cosf(w->yaw);
    if(fabsf(dx)>fabsf(dz))placed=dx>0?MC_STAIR_EAST:MC_STAIR_WEST;
    else placed=dz>0?MC_STAIR_SOUTH:MC_STAIR;
  }
  if(b==MC_SLAB) {
    float u=(sx-400)/576,v=(240-sy)/576,sp=sinf(w->pitch),cp=cosf(w->pitch);
    float syaw=sinf(w->yaw),cyaw=cosf(w->yaw);
    float dx=syaw*cp+u*cyaw-v*sp*syaw,dy=sp+v*cp,dz=cyaw*cp-u*syaw-v*sp*cyaw;
    float py=w->y+1.62f+dy*hit.distance/sqrtf(dx*dx+dy*dy+dz*dz);
    float local=py-hit.y;
    placed=(ny>0 || (ny==0 && local<.5f))?MC_SLAB:MC_SLAB_TOP;
  }
  if(b==MC_CHEST) {
    for(unsigned i=0;i<MC_CHESTS;i++)if(!w->chest_map[i][0]){ci=(int)i;break;}
    if(ci<0){w->notice="MAX 6 CHESTS";return 0;}
  }
  if(b==MC_FURNACE) {
    for(unsigned i=0;i<MC_FURNACES;i++)if(!w->furnaces[i].x){fi=(int)i;break;}
    if(fi<0){w->notice="MAX 6 FURNACES";return 0;}
  }
  if(b==MC_SAPLING) {
    uint8_t ground=MC_Get(w,x,y-1,z);
    if(ground!=MC_GRASS && ground!=MC_DIRT){w->notice="PLANT ON DIRT OR GRASS";return 0;}
    for(unsigned i=0;i<MC_GROWERS;i++)if(!w->growers[i].x){gi=(int)i;break;}
    if(gi<0){w->notice="MAX 16 GROWING TREES";return 0;}
  }
  if(!MC_EditAt(w,1,placed,sx,sy))return 0;
  if(ci>=0){w->chest_map[ci][0]=x+1;w->chest_map[ci][1]=y+1;w->chest_map[ci][2]=z+1;}
  if(fi>=0){w->furnaces[fi].x=x+1;w->furnaces[fi].y=y+1;w->furnaces[fi].z=z+1;}
  if(gi>=0)w->growers[gi]=(MC_Grower){x+1,y+1,z+1,0};
  if(!w->creative){MC_Stack *s=&w->slots[w->slot];if(!--s->count)memset(s,0,sizeof(*s));}
  MC_SyncInventory(w);w->notice=MC_ItemName(b);
  if(b==MC_DUST||b==MC_LAMP||b==MC_LEVER||b==MC_BUTTON)MC_RedstoneRefresh(w);
  return 1;
}
void MC_PlayTick(MC_World *w,float dt,int holding,float x,float y) {
  if(!(dt>0)||w->bag)return;
  dt=fminf(dt,.1f);w->time=fmodf(w->time+dt,600);
  w->hurt_timer=fmaxf(0,w->hurt_timer-dt);
  w->level_flash=fmaxf(0,w->level_flash-dt);
  if(!w->creative) {
    w->hunger_clock+=dt;
    while(w->hunger_clock>=30) {
      w->hunger_clock-=30;
      if(w->hunger)w->hunger--;
    }
    if(w->hunger)w->starvation_clock=0;
    else {
      w->starvation_clock+=dt;
      while(w->starvation_clock>=2) {
        w->starvation_clock-=2;
        MC_Hurt(w,1);
      }
    }
  }
  if(MC_Get(w,(int)w->x,(int)(w->y+1.62f),(int)w->z)==MC_WATER) {
    w->fall_speed=0;w->breath+=dt;
    if(w->breath>=1){w->breath-=1;if(w->air)w->air--;else MC_Hurt(w,2);}
  } else {w->air=10;w->breath=0;}
  if(w->on_ground){if(w->fall_speed>11)MC_Hurt(w,(unsigned)((w->fall_speed-10)*.8f));w->fall_speed=0;}
  else if(-w->vy>w->fall_speed)w->fall_speed=-w->vy;
  MC_MobsTick(w,dt);
  /* Holding the scene with a bow keeps firing on cooldown, whether the ray
     currently crosses a mob or is aimed into open space. */
  if(holding && w->hotbar[w->slot]==MC_BOW)MC_Shoot(w,x,y);
  MC_Hit h;int ok=MC_Pick(w,holding?x:400,holding?y:240,&h);
  float mob_distance;int mob=MC_PickMob(w,holding?x:400,holding?y:240,3.5f,&mob_distance);
  if(mob>=0 && (!ok || mob_distance<h.distance)) {
    w->target_valid=0;w->mining=0;w->mob_target=(uint8_t)mob;
    if(holding && w->hotbar[w->slot]!=MC_BOW && MC_ToolIndex(w->hotbar[w->slot])>=0)MC_Attack(w,x,y);
    return;
  }
  w->mob_target=255;
  if(!ok || !holding || !w->target_valid || h.x!=w->target.x || h.y!=w->target.y || h.z!=w->target.z)w->mining=0;
  w->target_valid=(uint8_t)ok;if(ok)w->target=h;
  if(!ok || !holding || h.y==0 || h.block==MC_BEDROCK || h.block==MC_WATER)return;
  unsigned tool=w->hotbar[w->slot];int ti=MC_ToolIndex(tool);
  if(!w->creative && !w->slots[w->slot].count)ti=-1;
  w->equipped=ti<0?0:(uint8_t)tool;
  /* A bow is an action item. Long-pressing it keeps firing on its cooldown
     and must never fall through into block mining. */
  if(tool==MC_BOW)return;
  unsigned tier=ti>=0 && ti<3?(unsigned)ti+1:0;
  int hard=h.block==MC_STONE || h.block==MC_COBBLE || h.block==MC_COAL_ORE || h.block==MC_IRON_ORE || h.block==MC_FURNACE || h.block==MC_BRICK || h.block==MC_SLAB || h.block==MC_SLAB_TOP || h.block==MC_DOUBLE_SLAB || (h.block>=MC_STAIR && h.block<=MC_STAIR_WEST);
  int suited=hard?tier>0:(tool==MC_AXE && (h.block==MC_WOOD || h.block==MC_BIRCH_WOOD || h.block==MC_DARK_WOOD || h.block==MC_PLANKS || h.block==MC_BIRCH_PLANKS || h.block==MC_DARK_PLANKS || h.block==MC_TABLE || h.block==MC_CHEST || h.block==MC_DOOR || h.block==MC_DOOR_OPEN || h.block==MC_FENCE)) ||
    (tool==MC_SHOVEL && (h.block==MC_GRASS || h.block==MC_DIRT || h.block==MC_SAND)) || (tool==MC_SWORD && h.block==MC_LEAVES);
  float sec=hard?(tier?1.3f/(tier*.8f):4):(h.block==MC_WOOD||h.block==MC_BIRCH_WOOD||h.block==MC_DARK_WOOD)?1.4f:h.block==MC_LEAVES?.25f:.55f;
  if(!hard && suited)sec*=.35f;
  if(w->creative)sec=.15f;
  w->mining+=dt/sec;if(w->mining<1)return;
  int ci=chest_at(w,h.x,h.y,h.z),fi=furnace_at(w,h.x,h.y,h.z);
  if(ci>=0)for(unsigned i=0;i<MC_CHEST_SLOTS;i++)if(w->chest_blocks[ci][i].count){w->notice="EMPTY CHEST BEFORE MINING";w->mining=0;return;}
  if(fi>=0 && (w->furnaces[fi].input.count || w->furnaces[fi].fuel.count || w->furnaces[fi].output.count || w->furnaces[fi].burn>0)) {
    w->notice="EMPTY FURNACE AND LET FIRE OUT";w->mining=0;return;
  }
  if(h.block==MC_WHEAT_CROP || h.block==MC_WHEAT_MATURE) {
    w->mining+=dt/.45f;if(w->mining<1)return;
    if(h.block==MC_WHEAT_CROP){w->notice="WHEAT IS STILL GROWING";w->mining=0;return;}
    float px=h.x+.5f,py=h.y+.55f,pz=h.z+.5f;
    if(!MC_Give(w,MC_WHEAT,1) && !MC_DropSpawn(w,MC_WHEAT,1,px,py,pz)){w->notice="BAG FULL";w->mining=0;return;}
    unsigned seeds=1u+(unsigned)((h.x*17+h.z*31)&1u);
    if(!MC_Give(w,MC_WHEAT_SEEDS,seeds))MC_DropSpawn(w,MC_WHEAT_SEEDS,seeds,px,py,pz);
    MC_Set(w,h.x,h.y,h.z,MC_AIR);w->mining=0;w->target_valid=0;
    for(unsigned i=0;i<MC_GROWERS;i++)if(w->growers[i].x==h.x+1 && w->growers[i].y==h.y+1 && w->growers[i].z==h.z+1)memset(&w->growers[i],0,sizeof(MC_Grower));
    MC_SyncInventory(w);w->notice="WHEAT HARVESTED";return;
  }
  unsigned drop=h.block==MC_GRASS||h.block==MC_FARMLAND?MC_DIRT:h.block==MC_STONE?MC_COBBLE:h.block==MC_COAL_ORE?MC_COAL:h.block;
  if(h.block==MC_DUST_ON||h.block==MC_DUST)drop=MC_DUST;
  if(h.block==MC_LAMP_ON||h.block==MC_LAMP)drop=MC_LAMP;
  if(h.block==MC_LEVER_ON||h.block==MC_LEVER)drop=MC_LEVER;
  if(h.block==MC_BUTTON_ON||h.block==MC_BUTTON)drop=MC_BUTTON;
  if(h.block>=MC_STAIR && h.block<=MC_STAIR_WEST)drop=MC_STAIR;
  if(h.block==MC_DOOR_OPEN)drop=MC_DOOR;
  if(h.block==MC_BIRCH_WOOD)drop=MC_BIRCH_WOOD;
  if(h.block==MC_DARK_WOOD)drop=MC_DARK_WOOD;
  if(h.block==MC_SLAB_TOP || h.block==MC_DOUBLE_SLAB)drop=MC_SLAB;
  unsigned drop_count=h.block==MC_DOUBLE_SLAB?2:1;
  if(h.block==MC_LEAVES){unsigned n=(unsigned)(h.x*7+h.y*3+h.z);drop=n%5==0?MC_APPLE:n%3==0?MC_SAPLING:0;}
  int harvest=(!hard || tier>0) && (h.block!=MC_IRON_ORE || tier>=2);
  if(!w->creative && harvest && drop) {
    /* Preserve the established direct pickup path; overflow becomes a world drop. */
    if(!MC_Give(w,drop,drop_count) && !MC_DropSpawn(w,drop,drop_count,h.x+.5f,h.y+.55f,h.z+.5f)) {
      w->notice="BAG FULL";w->mining=0;return;
    }
    if(h.block==MC_COAL_ORE)MC_AddExperience(w,2);
    else if(h.block==MC_IRON_ORE)MC_AddExperience(w,3);
    else if(h.block==MC_WOOD || h.block==MC_BIRCH_WOOD || h.block==MC_DARK_WOOD || h.block==MC_STONE || h.block==MC_COBBLE)MC_AddExperience(w,1);
  }
  if(!w->creative && h.block==MC_GRASS && ((unsigned)(h.x*13+h.z*7)%7u==0u)) {
    if(!MC_Give(w,MC_WHEAT_SEEDS,1))MC_DropSpawn(w,MC_WHEAT_SEEDS,1,h.x+.5f,h.y+.55f,h.z+.5f);
  }
  MC_Set(w,h.x,h.y,h.z,MC_AIR);w->mining=0;w->target_valid=0;
  if(ci>=0)memset(w->chest_map[ci],0,sizeof(w->chest_map[ci]));
  if(fi>=0)memset(&w->furnaces[fi],0,sizeof(MC_Furnace));
  for(unsigned i=0;i<MC_GROWERS;i++)if(w->growers[i].x==h.x+1 && w->growers[i].y==h.y+1 && w->growers[i].z==h.z+1)memset(&w->growers[i],0,sizeof(MC_Grower));
  if(w->spawn[0]==h.x+1 && w->spawn[1]==h.y+1 && w->spawn[2]==h.z+1)memset(w->spawn,0,sizeof(w->spawn));
  if(redstone_kind(h.block))MC_RedstoneRefresh(w);
  w->notice=harvest?(drop?MC_ItemName(drop):"LEAVES CLEARED"):"NEEDS BETTER PICK  NO DROP";
  if(!w->creative && ti>=0) {
    MC_Stack *t=&w->slots[w->slot];
    if(t->wear && !--t->wear){memset(t,0,sizeof(*t));w->equipped=0;w->notice="TOOL BROKE";}
    MC_SyncInventory(w);
  }
}
static void clear_empty(MC_Stack *s){if(!s->count)memset(s,0,sizeof(*s));}
/* Move a bounded number of sand cells near the player.  The cursor and clock
   are runtime-only, so v4 saves remain byte-for-byte compatible.  Restricting
   both the horizontal radius and vertical window keeps worst-case work small
   on the 15 FPS target while still making placed sand fall promptly. */
static void falling_sand(MC_World *w,float dt)
{
  w->gravity_clock+=dt;
  if(w->gravity_clock<.2f)return;
  w->gravity_clock=fmodf(w->gravity_clock,.2f);
  int bx=(int)floorf(w->x)-6,bz=(int)floorf(w->z)-6;
  int y0=(int)floorf(w->y)-4,y1=(int)floorf(w->y)+8;
  if(y0<2)y0=2;
  if(y1>=MC_MAP_Y)y1=MC_MAP_Y-1;
  /* 12 cells per update: a full 13x13 ring is revisited about every 2.8 s. */
  for(unsigned n=0;n<12;n++) {
    unsigned cell=w->gravity_cursor++%169u;
    int x=bx+(int)(cell%13u),z=bz+(int)(cell/13u);
    for(int y=y1;y>=y0;y--) {
      if(MC_Get(w,x,y,z)!=MC_SAND || MC_Get(w,x,y-1,z)!=MC_AIR)continue;
      MC_Set(w,x,y,z,MC_AIR);MC_Set(w,x,y-1,z,MC_SAND);break;
    }
  }
}
/* Move a bounded number of nearby water cells.  Water has no inventory item
   or saved level metadata, so the map byte itself is the complete state.  A
   cell first falls into air below; when supported, it may spread to one of
   four horizontal air cells that also have a solid floor.  The radius,
   vertical window and per-tick budget keep worst-case work predictable on the
   15 FPS target and prevent a whole-world ocean scan. */
static void flowing_water(MC_World *w,float dt)
{
  w->fluid_clock+=dt;
  if(w->fluid_clock<.25f)return;
  w->fluid_clock=fmodf(w->fluid_clock,.25f);
  int bx=(int)floorf(w->x)-7,bz=(int)floorf(w->z)-7;
  int y0=(int)floorf(w->y)-6,y1=(int)floorf(w->y)+10;
  if(y0<1)y0=1;
  if(y1>=MC_MAP_Y-1)y1=MC_MAP_Y-2;
  static const int dirs[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
  /* 16 cells over a 15x15 window; newly written cells are visited on later
     rounds, which turns a large spill into a smooth, bounded update. */
  for(unsigned n=0;n<16;n++) {
    unsigned cell=w->fluid_cursor++%225u;
    int x=bx+(int)(cell%15u),z=bz+(int)(cell/15u);
    for(int y=y1;y>=y0;y--) {
      if(MC_Get(w,x,y,z)!=MC_WATER)continue;
      if(MC_Get(w,x,y-1,z)==MC_AIR) {
        MC_Set(w,x,y,z,MC_AIR);MC_Set(w,x,y-1,z,MC_WATER);break;
      }
      if(!MC_IsSolid(MC_Get(w,x,y-1,z)))break;
      unsigned start=(w->fluid_cursor+n)&3u;
      for(unsigned d=0;d<4;d++) {
        const int *dir=dirs[(start+d)&3u];int nx=x+dir[0],nz=z+dir[1];
        if(MC_Get(w,nx,y,nz)!=MC_AIR || !MC_IsSolid(MC_Get(w,nx,y-1,nz)))continue;
        MC_Set(w,nx,y,nz,MC_WATER);break;
      }
      break;
    }
  }
}
/* Crop coordinates are the plant cell, one block above its farmland.
   Each check has a fixed 9x9 irrigation area and bounded lighting search.
   Local torch/lamp light is deliberately simplified, as in mob spawning. */
unsigned MC_CropGrowthNeeds(const MC_World *w,int x,int y,int z) {
  unsigned needs=0;
  if(MC_Get(w,x,y-1,z)!=MC_FARMLAND)needs|=MC_CROP_NEEDS_SOIL;
  if(MC_Get(w,x,y+1,z)!=MC_AIR)needs|=MC_CROP_NEEDS_SPACE;
  int water=0,light=0;
  for(int dz=-4;dz<=4 && !water;dz++)for(int dx=-4;dx<=4;dx++)
    if(MC_Get(w,x+dx,y-1,z+dz)==MC_WATER){water=1;break;}
  if(!water)needs|=MC_CROP_NEEDS_WATER;
  if(sinf(w->time*(6.2831853f/600))>-.38f) {
    light=1;
    for(int yy=y+1;yy<MC_MAP_Y;yy++) {
      unsigned b=MC_Get(w,x,yy,z);
      if(b!=MC_AIR && b!=MC_GLASS && b!=MC_TORCH){light=0;break;}
    }
  }
  for(int dy=-2;dy<=2 && !light;dy++)for(int dz=-4;dz<=4 && !light;dz++)for(int dx=-4;dx<=4;dx++) {
    unsigned b=MC_Get(w,x+dx,y+dy,z+dz);
    if(b==MC_TORCH || b==MC_LAMP_ON){light=1;break;}
  }
  if(!light)needs|=MC_CROP_NEEDS_LIGHT;
  return needs;
}
void MC_NotifySave(MC_World *w,unsigned result) {
  if(result>MC_SAVE_VERIFY_FAILED)result=MC_SAVE_IDLE;
  w->save_feedback=(uint8_t)result;
  w->save_feedback_time=result==MC_SAVE_IDLE || result==MC_SAVE_BUSY?0:result==MC_SAVE_OK?3:6;
}
void MC_SystemsTick(MC_World *w,float dt) {
  if(!(dt>0))return;
  /* UI feedback keeps wall-clock duration even while the inventory pauses
     gameplay or a long frame forces physics to clamp its time step. */
  if(w->save_feedback_time>0) {
    w->save_feedback_time=fmaxf(0,w->save_feedback_time-dt);
    if(!w->save_feedback_time)w->save_feedback=MC_SAVE_IDLE;
  }
  dt=fminf(dt,.1f);
  int redstone_dirty=0;
  for(unsigned i=0;i<MC_REDSTONE_BUTTONS;i++) if(w->redstone_button_timer[i]>0) {
    w->redstone_button_timer[i]-=dt;
    if(w->redstone_button_timer[i]<=0) {
      int x=(int)w->redstone_button_pos[i][0]-1,y=(int)w->redstone_button_pos[i][1]-1,z=(int)w->redstone_button_pos[i][2]-1;
      if(MC_Get(w,x,y,z)==MC_BUTTON_ON)MC_Set(w,x,y,z,MC_BUTTON);
      memset(w->redstone_button_pos[i],0,sizeof(w->redstone_button_pos[i]));w->redstone_button_timer[i]=0;redstone_dirty=1;
    }
  }
  w->redstone_clock+=dt;
  if(w->redstone_clock>=.25f){w->redstone_clock=fmodf(w->redstone_clock,.25f);redstone_dirty=1;}
  if(redstone_dirty)MC_RedstoneRefresh(w);
  MC_ProjectilesTick(w,dt);
  falling_sand(w,dt);
  flowing_water(w,dt);
  automation_tick(w,dt);
  for(unsigned i=0;i<MC_DROPS;i++) {
    MC_Drop *d=&w->drops[i];if(!d->count)continue;
    d->age+=dt;d->spin+=dt*3.2f;
    if(d->age>300 || !d->item || d->item>=MC_ITEM_COUNT){memset(d,0,sizeof(*d));continue;}
    d->vy-=18*dt;d->y+=d->vy*dt;
    int bx=(int)floorf(d->x),by=(int)floorf(d->y-.12f),bz=(int)floorf(d->z);
    if(MC_IsSolid(MC_Get(w,bx,by,bz)) && d->y<by+1.12f){d->y=by+1.12f;d->vy=0;}
    float dx=d->x-w->x,dy=d->y-(w->y+.65f),dz=d->z-w->z;
    if(dx*dx+dy*dy+dz*dz<1.35f*1.35f && MC_Give(w,d->item,d->count)) {
      w->notice=MC_ItemName(d->item);memset(d,0,sizeof(*d));continue;
    }
  }
  /* Merge nearby stacks with a fixed O(16^2) budget. */
  for(unsigned i=0;i<MC_DROPS;i++)if(w->drops[i].count)for(unsigned j=i+1;j<MC_DROPS;j++)
    if(w->drops[j].count && w->drops[i].item==w->drops[j].item && fabsf(w->drops[i].x-w->drops[j].x)<.55f && fabsf(w->drops[i].z-w->drops[j].z)<.55f) {
      unsigned room=64-w->drops[i].count,n=w->drops[j].count<room?w->drops[j].count:room;
      w->drops[i].count+=(uint8_t)n;w->drops[j].count-=(uint8_t)n;if(!w->drops[j].count)memset(&w->drops[j],0,sizeof(MC_Drop));
    }
  for(unsigned i=1;i<25;i++)if(w->reserve[i] && i!=MC_BEDROCK && i!=MC_WATER) {
    unsigned n=w->reserve[i]>64?64:w->reserve[i],room=capacity(w->slots,MC_SLOTS,i);
    if(n>room)n=room;
    if(MC_ToolIndex(i)>=0 && n>1)n=1;
    if(n) {
      unsigned empty=0;while(empty<MC_SLOTS && w->slots[empty].count)empty++;
      if(MC_Give(w,i,n)) {
        if(i>=MC_WOOD_PICK && i<=MC_IRON_PICK && empty<MC_SLOTS && w->reserve_wear[i-MC_WOOD_PICK]) {
          w->slots[empty].wear=w->reserve_wear[i-MC_WOOD_PICK];MC_SyncInventory(w);
        }
        w->reserve[i]-=n;w->notice="OLD INVENTORY ITEMS RESTORED";break;
      }
    }
  }
  for(unsigned i=0;i<MC_FURNACES;i++) {
    MC_Furnace *f=&w->furnaces[i];if(!f->x)continue;
    unsigned out=smelt(f->input.item);
    int ready=f->input.count && out && (!f->output.count || f->output.item==out) && f->output.count<64;
    if(f->burn<=0 && ready && f->fuel.count && fuel_time(f->fuel.item)) {
      f->burn=(float)fuel_time(f->fuel.item);f->fuel.count--;clear_empty(&f->fuel);
    }
    if(f->burn>0) {
      float elapsed=fminf(dt,f->burn);f->burn=fmaxf(0,f->burn-dt);
      if(ready) {
        f->progress+=elapsed;
        if(f->progress>=4.99999f){f->output.item=(uint8_t)out;f->output.count++;f->input.count--;clear_empty(&f->input);f->progress=fmaxf(0,f->progress-5);MC_AddExperience(w,1);}
      } else f->progress=0;
    }
    if(!f->input.count)f->progress=0;
  }
  w->crop_clock+=dt;
  float crop_elapsed=w->crop_clock>=1?w->crop_clock:0;
  if(crop_elapsed)w->crop_clock=0;
  /* Up to 16 crop checks once per second. Dry/dark/blocked crops retain age;
     tree timers keep their established cadence and clearance behavior. */
  for(unsigned i=0;i<MC_GROWERS;i++) {
    MC_Grower *g=&w->growers[i];if(!g->x)continue;
    int x=g->x-1,y=g->y-1,z=g->z-1;
    uint8_t current=MC_Get(w,x,y,z);
    if(current==MC_WHEAT_CROP) {
      if(!crop_elapsed || MC_CropGrowthNeeds(w,x,y,z))continue;
      g->age+=crop_elapsed;
      if(g->age<75)continue;
      MC_Set(w,x,y,z,MC_WHEAT_MATURE);memset(g,0,sizeof(*g));continue;
    }
    if(current!=MC_SAPLING){memset(g,0,sizeof(*g));continue;}
    g->age+=dt;
    if(g->age<120)continue;
    int space=1;
    if(x<3 || x>=MC_MAP_X-3 || z<3 || z>=MC_MAP_Z-3 || y>MC_MAP_Y-8)space=0;
    for(int dy=1;dy<=5 && space;dy++)for(int dz=-2;dz<=2;dz++)for(int dx=-2;dx<=2;dx++) {
      uint8_t b=MC_Get(w,x+dx,y+dy,z+dz);if(b!=MC_AIR && b!=MC_LEAVES)space=0;
    }
    if(!space){g->age=110;continue;}
    /* Prevent a growing trunk from enclosing the player. */
    if(fabsf(w->x-(x+.5f))<2.8f && fabsf(w->z-(z+.5f))<2.8f && w->y<y+6 && w->y+1.8f>y){g->age=110;continue;}
    uint32_t variety=(uint32_t)x*0x9e3779b9u^(uint32_t)z*0x85ebca6bu^w->seed;
    uint8_t trunk=((variety&7u)==0u)?MC_BIRCH_WOOD:((variety&3u)==0u?MC_DARK_WOOD:MC_WOOD);
    for(int dy=0;dy<4;dy++)MC_Set(w,x,y+dy,z,trunk);
    for(int dy=2;dy<=5;dy++)for(int dz=-2;dz<=2;dz++)for(int dx=-2;dx<=2;dx++)
      if(abs(dx)+abs(dz)+(dy==5?2:0)<=4 && MC_Get(w,x+dx,y+dy,z+dz)==MC_AIR)MC_Set(w,x+dx,y+dy,z+dz,MC_LEAVES);
    memset(g,0,sizeof(*g));
  }
}
int MC_OpenTarget(MC_World *w) { return open_target_at(w,400,240); }
int MC_ContainerTap(MC_World *w,unsigned slot) {
  MC_Stack *src=&w->slots[w->selected_slot];
  if(w->open_chest<MC_CHESTS && slot<MC_CHEST_SLOTS) {
    MC_Stack *dst=&w->chest_blocks[w->open_chest][slot];
    int ok;
    if(src->count)ok=MC_MoveStack(src,dst,w->split);
    else ok=MC_MoveStack(dst,src,w->split);
    MC_SyncInventory(w);return ok;
  }
  if(w->open_furnace<MC_FURNACES && slot<3) {
    MC_Furnace *f=&w->furnaces[w->open_furnace];
    MC_Stack *dst=slot==0?&f->input:slot==1?&f->fuel:&f->output;
    if(slot==2 || !src->count) {
      if(!dst->count)return 0;
      if(!add_stack(w->slots,MC_SLOTS,*dst)){w->notice="BAG FULL";return 0;}
      memset(dst,0,sizeof(*dst));if(slot==0)f->progress=0;
    } else {
      if((slot==0 && !smelt(src->item)) || (slot==1 && !fuel_time(src->item))){w->notice="INVALID INPUT OR FUEL";return 0;}
      if(dst->count && dst->item!=src->item){w->notice="EMPTY SLOT FIRST";return 0;}
      if(!MC_MoveStack(src,dst,w->split))return 0;
    }
    MC_SyncInventory(w);return 1;
  }
  return 0;
}
int MC_TransferSlot(MC_World *w,unsigned slot,int shift) {
  if(slot>=MC_SLOTS)return 0;
  w->selected_slot=(uint8_t)slot;
  if(w->open_chest<MC_CHESTS || w->open_furnace<MC_FURNACES)return 1;
  if(w->moving_slot<MC_SLOTS) {
    unsigned from=w->moving_slot;w->moving_slot=255;
    int ok=MC_MoveStack(&w->slots[from],&w->slots[slot],shift);
    MC_SyncInventory(w);return ok;
  }
  if(w->slots[slot].count)w->moving_slot=(uint8_t)slot;
  return 1;
}
