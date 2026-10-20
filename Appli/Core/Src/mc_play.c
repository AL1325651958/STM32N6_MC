#include "minicraft.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
static const char *names[MC_ITEM_COUNT]={"AIR","GRASS","DIRT","STONE","SAND","LOG","LEAVES","BEDROCK","WATER","COBBLE","PLANKS","WORKBENCH","FURNACE","COAL ORE","IRON ORE","GLASS","BRICKS","TORCH","STICKS","COAL","IRON","WOOD PICK","STONE PICK","IRON PICK","APPLE","AXE","SHOVEL","SWORD","CHEST","BED","SAPLING","DOOR","DOOR OPEN","STONE SLAB","SLAB TOP","FARMLAND","WHEAT CROP","WHEAT MATURE","WHEAT SEEDS","WHEAT"};
static const uint16_t colors[MC_ITEM_COUNT]={0,0x6cc6,0x8b07,0x8410,0xd66f,0x8b45,0x3b85,0x3186,0x3397,0x7390,0xb48a,0x9347,0x630c,0x4208,0xb3ac,0x9f7f,0xb2a8,0xff20,0x9b05,0x2104,0xc638,0xa366,0x94b2,0xdefb,0xe924,0x9b05,0x9b05,0xdefb,0x9347,0x6cc6,0x3b85,0xa365,0x9b05,0x8c71,0x8c71,0x6a64,0x4d8b,0xf6a0,0x7e24,0xdda2};
const char *MC_ItemName(unsigned i){return i<MC_ITEM_COUNT?names[i]:"EMPTY";}uint16_t MC_ItemColor(unsigned i){return i<MC_ITEM_COUNT?colors[i]:0;}
void MC_HoldStart(MC_Hold*h,int x,int y,uint32_t n){*h=(MC_Hold){x,y,x,y,n};}void MC_HoldMove(MC_Hold*h,int x,int y,uint32_t n){int dx=x-h->anchor_x,dy=y-h->anchor_y;h->x=x;h->y=y;if(dx*dx+dy*dy>144){h->anchor_x=x;h->anchor_y=y;h->since=n;}}int MC_HoldReady(const MC_Hold*h,uint32_t n){return(uint32_t)(n-h->since)>=450;}
int MC_IsBlock(unsigned i) {
  return (i>MC_AIR && i<MC_BLOCK_COUNT) || i==MC_CHEST || i==MC_BED || i==MC_SAPLING || i==MC_DOOR || i==MC_DOOR_OPEN || i==MC_SLAB || i==MC_SLAB_TOP || i==MC_FARMLAND || i==MC_WHEAT_CROP || i==MC_WHEAT_MATURE;
}
static int map_only(unsigned i){return i==MC_FARMLAND||i==MC_WHEAT_CROP||i==MC_WHEAT_MATURE;}
int MC_ToolIndex(unsigned i) {
  if(i>=MC_WOOD_PICK && i<=MC_IRON_PICK)return (int)i-MC_WOOD_PICK;
  if(i>=MC_AXE && i<=MC_SWORD)return (int)i-MC_AXE+3;
  return -1;
}
unsigned MC_ToolLife(unsigned i) {
  static const uint16_t life[MC_TOOLS]={60,132,250,90,90,90};
  int t=MC_ToolIndex(i);return t<0?0:life[t];
}
static unsigned capacity(const MC_Stack *slots,unsigned len,unsigned id) {
  unsigned room=0,limit=MC_ToolIndex(id)<0?64:1;
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
    unsigned n=MC_ToolIndex(src.item)>=0?1:src.count>64?64:src.count;
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
  if(!add_stack(w->slots,MC_SLOTS,src))return 0;
  MC_SyncInventory(w);return 1;
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
  w->creative=(uint8_t)!!creative;MC_SyncInventory(w);w->moving_slot=255;
}
void MC_CloseBag(MC_World *w) {
  w->bag=0;w->open_chest=w->open_furnace=255;w->moving_slot=255;
  w->recipe_requires=0;
}
void MC_NewPlayer(MC_World *w,int legacy) {
  memset(w->items,0,sizeof(w->items));memset(w->slots,0,sizeof(w->slots));
  memset(w->chest_blocks,0,sizeof(w->chest_blocks));memset(w->chest_map,0,sizeof(w->chest_map));
  memset(w->furnaces,0,sizeof(w->furnaces));memset(w->growers,0,sizeof(w->growers));
  memset(w->reserve,0,sizeof(w->reserve));memset(w->spawn,0,sizeof(w->spawn));memset(w->durability,0,sizeof(w->durability));
  memset(w->reserve_wear,0,sizeof(w->reserve_wear));
  const uint8_t bar[8]={MC_WOOD,MC_DIRT,MC_COBBLE,MC_PLANKS,MC_TABLE,MC_TORCH,MC_GLASS,MC_APPLE};
  memcpy(w->creative_bar,bar,8);w->slot=0;w->creative=(uint8_t)legacy;
  w->health=20;w->air=10;w->equipped=0;w->time=90;
  w->fall_speed=w->breath=w->hurt_timer=0;w->target_valid=0;w->mining=0;
  w->bag=w->recipe_page=w->change_world=w->bag_page=w->recipe_requires=w->split=0;
  w->selected_slot=0;w->open_chest=w->open_furnace=w->moving_slot=255;
  w->sneak=0;
  memset(w->mobs,0,sizeof(w->mobs));memset(w->drops,0,sizeof(w->drops));w->mob_rng=w->seed^0x9e3779b9u;if(!w->mob_rng)w->mob_rng=1;
  w->mob_target=255;w->mob_clock=w->spawn_clock=w->attack_cooldown=w->attack_flash=0;
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
static const R recipes[]={{MC_PLANKS,4,MC_WOOD,1,0,0,0},{MC_STICK,4,MC_PLANKS,2,0,0,0},{MC_TABLE,1,MC_PLANKS,4,0,0,0},{MC_WOOD_PICK,1,MC_PLANKS,3,MC_STICK,2,MC_TABLE},{MC_STONE_PICK,1,MC_COBBLE,3,MC_STICK,2,MC_TABLE},{MC_FURNACE,1,MC_COBBLE,8,0,0,MC_TABLE},{MC_COAL,1,MC_WOOD,1,MC_PLANKS,1,MC_FURNACE},{MC_TORCH,4,MC_COAL,1,MC_STICK,1,0},{MC_IRON,1,MC_IRON_ORE,1,MC_COAL,1,MC_FURNACE},{MC_IRON_PICK,1,MC_IRON,3,MC_STICK,2,MC_TABLE},{MC_GLASS,4,MC_SAND,4,MC_COAL,1,MC_FURNACE},{MC_BRICK,4,MC_COBBLE,4,MC_COAL,1,MC_FURNACE},{MC_AXE,1,MC_PLANKS,3,MC_STICK,2,MC_TABLE},{MC_SHOVEL,1,MC_PLANKS,1,MC_STICK,2,MC_TABLE},{MC_SWORD,1,MC_PLANKS,2,MC_STICK,1,MC_TABLE},{MC_CHEST,1,MC_PLANKS,8,0,0,MC_TABLE},{MC_BED,1,MC_PLANKS,3,MC_SAPLING,3,MC_TABLE},{MC_DOOR,1,MC_PLANKS,6,0,0,MC_TABLE},{MC_SLAB,6,MC_COBBLE,3,0,0,MC_TABLE}};
static const char*cost[]={"1 LOG - 4 PLANKS","2 PLANKS - 4 STICKS","4 PLANKS","3 PLANKS 2 STICKS","3 COBBLE 2 STICKS","8 COBBLE","1 LOG 1 PLANK","1 COAL 1 STICK","1 ORE 1 COAL","3 IRON 2 STICKS","4 SAND 1 COAL","4 COBBLE 1 COAL","3 PLANKS 2 STICKS","1 PLANK 2 STICKS","2 PLANKS 1 STICK","8 PLANKS","3 PLANKS 3 SAPLINGS","6 PLANKS","3 COBBLE - 6 STONE SLABS"};
unsigned MC_RecipeCount(void){return sizeof(recipes)/sizeof(recipes[0]);}const char*MC_RecipeName(unsigned r){return r<MC_RecipeCount()?MC_ItemName(recipes[r].out):"";}const char*MC_RecipeCost(unsigned r){return r<MC_RecipeCount()?cost[r]:"";}
unsigned MC_RecipeResult(unsigned r){return r<MC_RecipeCount()?recipes[r].out:0;}
unsigned MC_RecipeIngredient(unsigned r,unsigned p){if(r>=MC_RecipeCount()||p>1)return 0;return p?recipes[r].b:recipes[r].a;}
unsigned MC_RecipeIngredientCount(unsigned r,unsigned p){if(r>=MC_RecipeCount()||p>1)return 0;return p?recipes[r].nb:recipes[r].na;}
static int near(const MC_World*w,uint8_t b){for(int y=(int)w->y-2;y<=(int)w->y+2;y++)for(int z=(int)w->z-3;z<=(int)w->z+3;z++)for(int x=(int)w->x-3;x<=(int)w->x+3;x++)if(MC_Get(w,x,y,z)==b)return 1;return 0;}
const char*MC_RecipeStatus(const MC_World*w,unsigned r){if(r>=MC_RecipeCount())return "";R c=recipes[r];if(c.station==MC_FURNACE)return "USE FURNACE INPUT AND FUEL";if(!w->creative&&c.station&&!near(w,c.station))return "NEEDS WORKBENCH";return w->creative||(w->items[c.a]>=c.na&&w->items[c.b]>=c.nb)?"READY":"NEED MATERIALS";}
int MC_Craft(MC_World *w,unsigned r) {
  if(r>=MC_RecipeCount())return 0;
  R c=recipes[r];
  /* Smelting has its own persistent input/fuel/output and timer. */
  if(c.station==MC_FURNACE){w->notice="OPEN FURNACE  ADD INPUT AND FUEL";return 0;}
  if(!w->creative && c.station && !near(w,c.station)){w->notice="PLACE WORKBENCH NEARBY";return 0;}
  if(!w->creative && (w->items[c.a]<c.na || w->items[c.b]<c.nb)){w->notice="NEED MORE MATERIALS";return 0;}
  MC_Stack before[MC_SLOTS];memcpy(before,w->slots,sizeof(before));
  if(!w->creative){remove_item(w,c.a,c.na);remove_item(w,c.b,c.nb);}
  if(!MC_Give(w,c.out,c.n)) {
    memcpy(w->slots,before,sizeof(before));MC_SyncInventory(w);w->notice="BAG FULL  NO MATERIALS USED";return 0;
  }
  if(MC_ToolIndex(c.out)>=0){w->equipped=c.out;for(unsigned i=0;i<MC_HOTBAR;i++)if(w->slots[i].item==c.out){w->slot=i;break;}}
  else MC_SelectItem(w,c.out);
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
static int drop_spawn(MC_World *w,unsigned item,unsigned count,float x,float y,float z) {
  if(!item || item>=MC_ITEM_COUNT || !count)return 0;
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
static int fuel_time(unsigned item){return item==MC_COAL?40:item==MC_WOOD?15:item==MC_PLANKS?10:0;}
static unsigned smelt(unsigned item){return item==MC_IRON_ORE?MC_IRON:item==MC_SAND?MC_GLASS:item==MC_COBBLE?MC_BRICK:item==MC_WOOD?MC_COAL:0;}
int MC_Use(MC_World *w) {
  if(MC_OpenTarget(w))return 1;
  unsigned b=w->hotbar[w->slot];
  if(b==MC_APPLE && w->health<20 && (w->creative || w->items[b])) {
    if(!w->creative){MC_Stack *s=&w->slots[w->slot];if(!--s->count)memset(s,0,sizeof(*s));}
    MC_SyncInventory(w);w->health=(uint8_t)fminf(20,w->health+5);w->notice="ATE APPLE";return 1;
  }
  if(b==MC_SHOVEL) {
    MC_Hit h;if(MC_Pick(w,400,240,&h) && (h.block==MC_GRASS||h.block==MC_DIRT) && MC_Get(w,h.x,h.y+1,h.z)==MC_AIR) {
      MC_Set(w,h.x,h.y,h.z,MC_FARMLAND);
      if(!w->creative) { MC_Stack *s=&w->slots[w->slot]; if(s->wear && !--s->wear){memset(s,0,sizeof(*s));w->equipped=0;w->notice="SHOVEL BROKE";} MC_SyncInventory(w); }
      w->notice="TILLED SOIL";return 1;
    }
    w->notice="TILL GRASS OR DIRT";return 0;
  }
  if(b==MC_WHEAT_SEEDS) {
    MC_Hit h;if(MC_Pick(w,400,240,&h) && h.block==MC_FARMLAND && MC_Get(w,h.x,h.y+1,h.z)==MC_AIR) {
      int gi=-1;for(unsigned i=0;i<MC_GROWERS;i++)if(!w->growers[i].x){gi=(int)i;break;}
      if(gi<0){w->notice="MAX 16 CROPS";return 0;}
      MC_Set(w,h.x,h.y+1,h.z,MC_WHEAT_CROP);w->growers[gi]=(MC_Grower){h.x+1,h.y+2,h.z+1,0};
      if(!w->creative){MC_Stack*s=&w->slots[w->slot];if(!--s->count)memset(s,0,sizeof(*s));MC_SyncInventory(w);}w->notice="WHEAT PLANTED";return 1;
    }
    w->notice="PLANT ON TILLED SOIL";return 0;
  }
  if(MC_ToolIndex(b)>=0){MC_SelectItem(w,b);return 0;}
  return MC_Place(w);
}
int MC_Place(MC_World *w) {
  unsigned b=w->hotbar[w->slot];
  if(!MC_IsBlock(b) || map_only(b) || b==MC_BEDROCK || b==MC_WATER || b==MC_DOOR_OPEN || b==MC_SLAB_TOP)return 0;
  if(!w->creative && !w->items[b]){w->notice="NO BLOCKS";return 0;}
  int x,y,z,nx,ny,nz,ci=-1,fi=-1,gi=-1;
  MC_Hit hit;
  if(!MC_Pick(w,400,240,&hit))return 0;
  x=hit.x;y=hit.y;z=hit.z;nx=hit.nx;ny=hit.ny;nz=hit.nz;
  x+=nx;y+=ny;z+=nz;
  uint8_t placed=(uint8_t)b;
  if(b==MC_SLAB) {
    float dy=sinf(w->pitch);
    float py=w->y+1.62f+dy*hit.distance;
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
  if(!MC_Edit(w,1,placed))return 0;
  if(ci>=0){w->chest_map[ci][0]=x+1;w->chest_map[ci][1]=y+1;w->chest_map[ci][2]=z+1;}
  if(fi>=0){w->furnaces[fi].x=x+1;w->furnaces[fi].y=y+1;w->furnaces[fi].z=z+1;}
  if(gi>=0)w->growers[gi]=(MC_Grower){x+1,y+1,z+1,0};
  if(!w->creative){MC_Stack *s=&w->slots[w->slot];if(!--s->count)memset(s,0,sizeof(*s));}
  MC_SyncInventory(w);w->notice=MC_ItemName(b);return 1;
}
void MC_PlayTick(MC_World *w,float dt,int holding,float x,float y) {
  if(!(dt>0)||w->bag)return;
  dt=fminf(dt,.1f);w->time=fmodf(w->time+dt,600);
  w->hurt_timer=fmaxf(0,w->hurt_timer-dt);
  if(MC_Get(w,(int)w->x,(int)(w->y+1.62f),(int)w->z)==MC_WATER) {
    w->fall_speed=0;w->breath+=dt;
    if(w->breath>=1){w->breath-=1;if(w->air)w->air--;else MC_Hurt(w,2);}
  } else {w->air=10;w->breath=0;}
  if(w->on_ground){if(w->fall_speed>11)MC_Hurt(w,(unsigned)((w->fall_speed-10)*.8f));w->fall_speed=0;}
  else if(-w->vy>w->fall_speed)w->fall_speed=-w->vy;
  MC_MobsTick(w,dt);
  MC_Hit h;int ok=MC_Pick(w,holding?x:400,holding?y:240,&h);
  float mob_distance;int mob=MC_PickMob(w,holding?x:400,holding?y:240,3.5f,&mob_distance);
  if(mob>=0 && (!ok || mob_distance<h.distance)) {
    w->target_valid=0;w->mining=0;w->mob_target=(uint8_t)mob;
    if(holding)MC_Attack(w,x,y);
    return;
  }
  w->mob_target=255;
  if(!ok || !holding || !w->target_valid || h.x!=w->target.x || h.y!=w->target.y || h.z!=w->target.z)w->mining=0;
  w->target_valid=(uint8_t)ok;if(ok)w->target=h;
  if(!ok || !holding || h.y==0 || h.block==MC_BEDROCK || h.block==MC_WATER)return;
  unsigned tool=w->hotbar[w->slot];int ti=MC_ToolIndex(tool);
  if(!w->creative && !w->slots[w->slot].count)ti=-1;
  w->equipped=ti<0?0:(uint8_t)tool;
  unsigned tier=ti>=0 && ti<3?(unsigned)ti+1:0;
  int hard=h.block==MC_STONE || h.block==MC_COBBLE || h.block==MC_COAL_ORE || h.block==MC_IRON_ORE || h.block==MC_FURNACE || h.block==MC_BRICK || h.block==MC_SLAB || h.block==MC_SLAB_TOP;
  int suited=hard?tier>0:(tool==MC_AXE && (h.block==MC_WOOD || h.block==MC_PLANKS || h.block==MC_TABLE || h.block==MC_CHEST || h.block==MC_DOOR || h.block==MC_DOOR_OPEN)) ||
    (tool==MC_SHOVEL && (h.block==MC_GRASS || h.block==MC_DIRT || h.block==MC_SAND)) || (tool==MC_SWORD && h.block==MC_LEAVES);
  float sec=hard?(tier?1.3f/(tier*.8f):4):h.block==MC_WOOD?1.4f:h.block==MC_LEAVES?.25f:.55f;
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
    if(!MC_Give(w,MC_WHEAT,1) && !drop_spawn(w,MC_WHEAT,1,px,py,pz)){w->notice="BAG FULL";w->mining=0;return;}
    unsigned seeds=1u+(unsigned)((h.x*17+h.z*31)&1u);
    if(!MC_Give(w,MC_WHEAT_SEEDS,seeds))drop_spawn(w,MC_WHEAT_SEEDS,seeds,px,py,pz);
    MC_Set(w,h.x,h.y,h.z,MC_FARMLAND);w->mining=0;w->target_valid=0;
    for(unsigned i=0;i<MC_GROWERS;i++)if(w->growers[i].x==h.x+1 && w->growers[i].y==h.y+2 && w->growers[i].z==h.z+1)memset(&w->growers[i],0,sizeof(MC_Grower));
    MC_SyncInventory(w);w->notice="WHEAT HARVESTED";return;
  }
  unsigned drop=h.block==MC_GRASS||h.block==MC_FARMLAND?MC_DIRT:h.block==MC_STONE?MC_COBBLE:h.block==MC_COAL_ORE?MC_COAL:h.block;
  if(h.block==MC_DOOR_OPEN)drop=MC_DOOR;
  if(h.block==MC_SLAB_TOP)drop=MC_SLAB;
  if(h.block==MC_LEAVES){unsigned n=(unsigned)(h.x*7+h.y*3+h.z);drop=n%5==0?MC_APPLE:n%3==0?MC_SAPLING:0;}
  int harvest=(!hard || tier>0) && (h.block!=MC_IRON_ORE || tier>=2);
  if(!w->creative && harvest && drop) {
    /* Preserve the established direct pickup path; overflow becomes a world drop. */
    if(!MC_Give(w,drop,1) && !drop_spawn(w,drop,1,h.x+.5f,h.y+.55f,h.z+.5f)) {
      w->notice="BAG FULL";w->mining=0;return;
    }
  }
  if(!w->creative && h.block==MC_GRASS && ((unsigned)(h.x*13+h.z*7)%7u==0u)) {
    if(!MC_Give(w,MC_WHEAT_SEEDS,1))drop_spawn(w,MC_WHEAT_SEEDS,1,h.x+.5f,h.y+.55f,h.z+.5f);
  }
  MC_Set(w,h.x,h.y,h.z,MC_AIR);w->mining=0;w->target_valid=0;
  if(ci>=0)memset(w->chest_map[ci],0,sizeof(w->chest_map[ci]));
  if(fi>=0)memset(&w->furnaces[fi],0,sizeof(MC_Furnace));
  for(unsigned i=0;i<MC_GROWERS;i++)if(w->growers[i].x==h.x+1 && w->growers[i].y==h.y+1 && w->growers[i].z==h.z+1)memset(&w->growers[i],0,sizeof(MC_Grower));
  if(w->spawn[0]==h.x+1 && w->spawn[1]==h.y+1 && w->spawn[2]==h.z+1)memset(w->spawn,0,sizeof(w->spawn));
  w->notice=harvest?(drop?MC_ItemName(drop):"LEAVES CLEARED"):"NEEDS BETTER PICK  NO DROP";
  if(!w->creative && ti>=0) {
    MC_Stack *t=&w->slots[w->slot];
    if(t->wear && !--t->wear){memset(t,0,sizeof(*t));w->equipped=0;w->notice="TOOL BROKE";}
    MC_SyncInventory(w);
  }
}
static void clear_empty(MC_Stack *s){if(!s->count)memset(s,0,sizeof(*s));}
void MC_SystemsTick(MC_World *w,float dt) {
  if(!(dt>0))return;
  dt=fminf(dt,.1f);
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
        if(f->progress>=4.99999f){f->output.item=(uint8_t)out;f->output.count++;f->input.count--;clear_empty(&f->input);f->progress=fmaxf(0,f->progress-5);}
      } else f->progress=0;
    }
    if(!f->input.count)f->progress=0;
  }
  /* Fixed budget: 16 timers, growth clearance checked once when mature. */
  for(unsigned i=0;i<MC_GROWERS;i++) {
    MC_Grower *g=&w->growers[i];if(!g->x)continue;g->age+=dt;
    int x=g->x-1,y=g->y-1,z=g->z-1;
    uint8_t current=MC_Get(w,x,y,z);
    if(current==MC_WHEAT_CROP) {
      if(g->age<75)continue;
      if(MC_Get(w,x,y-1,z)!=MC_FARMLAND || MC_Get(w,x,y+1,z)!=MC_AIR){g->age=70;continue;}
      MC_Set(w,x,y,z,MC_WHEAT_MATURE);memset(g,0,sizeof(*g));continue;
    }
    if(g->age<120)continue;
    int space=1;
    if(current!=MC_SAPLING){memset(g,0,sizeof(*g));continue;}
    if(x<3 || x>=MC_MAP_X-3 || z<3 || z>=MC_MAP_Z-3 || y>MC_MAP_Y-8)space=0;
    for(int dy=1;dy<=5 && space;dy++)for(int dz=-2;dz<=2;dz++)for(int dx=-2;dx<=2;dx++) {
      uint8_t b=MC_Get(w,x+dx,y+dy,z+dz);if(b!=MC_AIR && b!=MC_LEAVES)space=0;
    }
    if(!space){g->age=110;continue;}
    /* Prevent a growing trunk from enclosing the player. */
    if(fabsf(w->x-(x+.5f))<2.8f && fabsf(w->z-(z+.5f))<2.8f && w->y<y+6 && w->y+1.8f>y){g->age=110;continue;}
    for(int dy=0;dy<4;dy++)MC_Set(w,x,y+dy,z,MC_WOOD);
    for(int dy=2;dy<=5;dy++)for(int dz=-2;dz<=2;dz++)for(int dx=-2;dx<=2;dx++)
      if(abs(dx)+abs(dz)+(dy==5?2:0)<=4 && MC_Get(w,x+dx,y+dy,z+dz)==MC_AIR)MC_Set(w,x+dx,y+dy,z+dz,MC_LEAVES);
    memset(g,0,sizeof(*g));
  }
}
int MC_OpenTarget(MC_World *w) {
  int x,y,z,nx,ny,nz;if(!MC_Raycast(w,&x,&y,&z,&nx,&ny,&nz,4))return 0;
  uint8_t b=MC_Get(w,x,y,z);w->moving_slot=255;
  if(b==MC_DOOR || b==MC_DOOR_OPEN) {
    if(b==MC_DOOR_OPEN) {
      MC_Set(w,x,y,z,MC_DOOR);
      if(!MC_BodyClear(w,w->x,w->y,w->z)) {
        MC_Set(w,x,y,z,MC_DOOR_OPEN);w->notice="MOVE CLEAR TO CLOSE";return 1;
      }
    } else MC_Set(w,x,y,z,MC_DOOR_OPEN);
    w->notice=b==MC_DOOR?"DOOR OPEN":"DOOR CLOSED";
    w->target_valid=0;w->mining=0;
    return 1;
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
    for(unsigned i=0;i<MC_MOBS;i++)if(w->mobs[i].health) {
      float dx=w->mobs[i].x-w->x,dz=w->mobs[i].z-w->z;
      if(dx*dx+dz*dz<64){w->notice="ENEMIES NEARBY  CANNOT SLEEP";return 1;}
    }
    w->spawn[0]=x+1;w->spawn[1]=y+1;w->spawn[2]=z+1;
    if(w->time>=300 || w->time<25){w->time=90;w->notice="SLEPT  SPAWN POINT SET";}
    else w->notice="SPAWN SET  SLEEP AT NIGHT";
    return 1;
  }
  return 0;
}
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
