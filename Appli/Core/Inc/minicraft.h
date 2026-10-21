#ifndef MINICRAFT_H
#define MINICRAFT_H

#include <stdint.h>

#define MC_MAP_X 256
#define MC_MAP_Y 64
#define MC_MAP_Z 256
#define MC_MAP_BYTES (MC_MAP_X * MC_MAP_Y * MC_MAP_Z)
enum { MC_SAVE_IDLE, MC_SAVE_BUSY, MC_SAVE_OK, MC_SAVE_NO_SD,
       MC_SAVE_LOCKED, MC_SAVE_WRITE_FAILED, MC_SAVE_VERIFY_FAILED };
#define MC_VIEW_W 160
#define MC_VIEW_H 96

enum { MC_AIR, MC_GRASS, MC_DIRT, MC_STONE, MC_SAND, MC_WOOD, MC_LEAVES,
       MC_BEDROCK, MC_WATER, MC_COBBLE, MC_PLANKS, MC_TABLE, MC_FURNACE,
       MC_COAL_ORE, MC_IRON_ORE, MC_GLASS, MC_BRICK, MC_TORCH, MC_BLOCK_COUNT,
       MC_STICK=MC_BLOCK_COUNT, MC_COAL, MC_IRON, MC_WOOD_PICK, MC_STONE_PICK,
       MC_IRON_PICK, MC_APPLE, MC_AXE, MC_SHOVEL, MC_SWORD, MC_CHEST, MC_BED, MC_SAPLING,
       MC_DOOR, MC_DOOR_OPEN, MC_SLAB, MC_SLAB_TOP,
       /* Map-only crop states are appended before the two inventory items so
          all existing block and item IDs remain stable for old saves. */
       MC_FARMLAND, MC_WHEAT_CROP, MC_WHEAT_MATURE,
       MC_WHEAT_SEEDS, MC_WHEAT, MC_FENCE, MC_STAIR,
       MC_STAIR_EAST, MC_STAIR_SOUTH, MC_STAIR_WEST, MC_RAW_MEAT,
       /* Redstone uses paired OFF/ON map states.  Keep these IDs appended so
          v1-v4 saves retain their original item numbering. */
       MC_DUST, MC_DUST_ON, MC_LAMP, MC_LAMP_ON,
       MC_LEVER, MC_LEVER_ON, MC_BUTTON, MC_BUTTON_ON,
       /* Ranged combat IDs are appended so all existing saves keep their
          original item numbering. */
       MC_BOW, MC_ARROW, MC_IRON_HELMET, MC_IRON_CHESTPLATE,
       MC_IRON_LEGGINGS, MC_IRON_BOOTS,
       /* P3 wood variants are appended so every v1-v4 item/map ID stays
          stable.  They are explicit blocks even though they live after the
          legacy item range. */
       MC_BIRCH_WOOD, MC_BIRCH_PLANKS, MC_DARK_WOOD, MC_DARK_PLANKS,
       MC_BREAD, MC_COOKED_MEAT,
       MC_DOUBLE_SLAB, /* map-only: two stone slabs, existing IDs unchanged */
       MC_DOOR_TOP, MC_DOOR_TOP_OPEN,
       MC_ITEM_COUNT };
#define MC_HOTBAR 8
#define MC_SLOTS 32
#define MC_CHEST_SLOTS 24
#define MC_FURNACES 6
#define MC_GROWERS 16
#define MC_TOOLS 7
#define MC_CHESTS 6
#define MC_MOBS 8
#define MC_MOB_HEALTH 12
#define MC_DROPS 16
#define MC_REDSTONE_SOURCES 32
#define MC_REDSTONE_BUTTONS 8
#define MC_PROJECTILES 8
#define MC_ARMOR_SLOTS 4
#define MC_MAX_LEVEL 30
enum { MC_CROP_NEEDS_WATER=1, MC_CROP_NEEDS_LIGHT=2,
       MC_CROP_NEEDS_SOIL=4, MC_CROP_NEEDS_SPACE=8 };
typedef struct { uint8_t item,count; uint16_t wear; } MC_Stack;
typedef struct { uint16_t x,y,z; MC_Stack input,fuel,output; float burn,progress; } MC_Furnace;
typedef struct { uint16_t x,y,z; float age; } MC_Grower;
typedef struct { int x,y,z,nx,ny,nz; float distance; uint8_t block; } MC_Hit;
/* type 0 is the hostile night stalker; type 1 is a passive daytime animal.
   heading is persisted in the unused tail of each fixed 32-byte v4 mob slot. */
typedef struct { float x,y,z,vy,attack,hurt; uint8_t health,on_ground,type; float heading; } MC_Mob;
typedef struct { float x,y,z,vy,age,spin; uint8_t item,count; } MC_Drop;
typedef struct { float x,y,z,vx,vy,vz,age; uint8_t active; } MC_Projectile;

typedef struct {
  uint8_t *blocks;
  uint32_t seed;
  float x, y, z, yaw, pitch, vy;
  uint8_t on_ground, sneak; /* sneak is a transient held input, never serialized */
  uint16_t items[MC_ITEM_COUNT], durability[MC_TOOLS];
  MC_Stack armor[MC_ARMOR_SLOTS];
  uint8_t hotbar[MC_HOTBAR], creative_bar[MC_HOTBAR], slot, creative, health, air, equipped, hunger, level;
  uint32_t experience;
  float level_flash;
  float time, fall_speed, breath, hurt_timer, hunger_clock, starvation_clock;
  MC_Hit target;
  float mining;
  MC_Stack slots[MC_SLOTS];
  uint8_t selected_slot, moving_slot, split, open_furnace;
  uint8_t open_chest, bag, recipe_page, change_world, recipe_requires;
  MC_Stack chest_blocks[MC_CHESTS][MC_CHEST_SLOTS];
  MC_Furnace furnaces[MC_FURNACES];
  MC_Grower growers[MC_GROWERS];
  uint16_t spawn[3];
  uint16_t reserve[25]; /* v2 migration overflow, drained when inventory has room */
  uint16_t reserve_wear[3];
  uint16_t chest_map[MC_CHESTS][3];
  uint8_t target_valid, bag_page;
  MC_Mob mobs[MC_MOBS];
  /* Runtime-only breeding state; v4 mob slots remain exactly 32 bytes. */
  uint8_t mob_fed[MC_MOBS];
  float mob_fed_age[MC_MOBS], breed_cooldown;
  MC_Drop drops[MC_DROPS]; /* Runtime-only entities; absent from v4 saves. */
  MC_Projectile projectiles[MC_PROJECTILES]; /* Runtime-only arrows. */
  uint32_t mob_rng;
  float mob_clock, spawn_clock, attack_cooldown, attack_flash, shoot_cooldown;
  /* Runtime-only bounded scheduler for nearby falling sand. */
  float gravity_clock;
  uint16_t gravity_cursor;
  /* Runtime-only bounded water scheduler; fluid state is derived from map
     bytes and therefore remains absent from the fixed v4 save layout. */
  float fluid_clock;
  uint16_t fluid_cursor;
  /* Runtime-only cadence for adjacent chest/furnace automation.  The link is
     derived from persistent block coordinates, so no v4 fields are needed. */
  float automation_clock;
  /* Runtime-only one-second crop checks; growers and age remain in v4 slots. */
  float crop_clock;
  /* Runtime-only redstone scheduler; map bytes keep lever state, while
     button timers and the propagation clock are discarded on save/load. */
  float redstone_clock;
  /* Derived cache only; reset on generation/load and excluded from saves. */
  uint32_t map_revision,redstone_revision;
  int16_t redstone_center[3];
  uint8_t redstone_valid;
  uint16_t redstone_button_pos[MC_REDSTONE_BUTTONS][3];
  float redstone_button_timer[MC_REDSTONE_BUTTONS];
  uint8_t mob_target;
  /* Temporary button feedback, never part of the save format. */
  uint8_t save_feedback;
  uint32_t scene_serial;
  float save_feedback_time;
  const char *notice;
} MC_World;

uint8_t MC_Get(const MC_World *world, int x, int y, int z);
void MC_Set(MC_World *world, int x, int y, int z, uint8_t block);
void MC_Generate(MC_World *world, uint32_t seed);
void MC_GenerateTerrain(MC_World *world,uint32_t seed);
void MC_NotifySave(MC_World *world,unsigned result);
uint8_t MC_DoorBase(uint8_t block);
int MC_IsDoor(uint8_t block);
int MC_DoorUpper(uint8_t block);
void MC_RemoveBlock(MC_World *world,int x,int y,int z);
void MC_Respawn(MC_World *world);
int MC_IsSolid(uint8_t block);
int MC_Edit(MC_World *world, int place, uint8_t selected);
int MC_EditAt(MC_World *world,int place,uint8_t selected,float sx,float sy);
void MC_Step(MC_World *world, float forward, float strafe, float dt, int jump);
void MC_Render(const MC_World *world, uint16_t *pixels, unsigned width, unsigned height);
int MC_Raycast(const MC_World *world, int *x, int *y, int *z,
               int *nx, int *ny, int *nz, float reach);
uint32_t MC_Crc32(const void *data, unsigned length);
int MC_Pick(const MC_World *w, float x, float y, MC_Hit *hit);
/* Earliest solid-part hit along a segment; fraction is in [0,1]. */
int MC_BlockSegment(const MC_World *w,float x,float y,float z,float ex,float ey,float ez,float *fraction);
int MC_BodyClear(const MC_World *w,float x,float y,float z);
int MC_PickMob(const MC_World *w,float x,float y,float reach,float *distance);
int MC_MobRay(const MC_World *w,float dx,float dy,float dz,float reach,unsigned mask,int *index,float *distance,int *axis,int *sign);
int MC_Attack(MC_World *w,float x,float y);
int MC_Shoot(MC_World *w,float x,float y);
int MC_MobFeed(MC_World *w,float x,float y);
/* Returns nonzero when this touch must not continue into mining. */
int MC_SceneTap(MC_World *w,float x,float y);
void MC_MobsTick(MC_World *w,float dt);
int MC_MobSpawn(MC_World *w,int x,int y,int z);
int MC_PassiveSpawn(MC_World *w,int x,int y,int z);
int MC_DarkAt(const MC_World *w,int x,int y,int z);
unsigned MC_CropGrowthNeeds(const MC_World *w,int x,int y,int z);
unsigned MC_FoodHealth(unsigned item);
unsigned MC_FoodHunger(unsigned item);
void MC_Hurt(MC_World *w,unsigned damage);
void MC_NewPlayer(MC_World *w, int legacy);
void MC_PlayTick(MC_World *w, float dt, int holding, float x, float y);
int MC_Place(MC_World *w);
/* Touch-friendly variants use the screen ray that was tapped.  The legacy
   wrappers below keep the centered crosshair behavior for existing callers. */
int MC_PlaceAt(MC_World *w,float x,float y);
int MC_Craft(MC_World *w, unsigned recipe);
int MC_Use(MC_World *w);
int MC_UseAt(MC_World *w,float x,float y);
int MC_Give(MC_World *w,unsigned item,unsigned count);
int MC_DropSpawn(MC_World *w,unsigned item,unsigned count,float x,float y,float z);
void MC_SyncInventory(MC_World *w);
void MC_SetMode(MC_World *w,int creative);
void MC_CloseBag(MC_World *w);
int MC_IsBlock(unsigned item);
int MC_ToolIndex(unsigned item);
unsigned MC_ToolLife(unsigned item);
int MC_ArmorIndex(unsigned item);
unsigned MC_ArmorLife(unsigned item);
unsigned MC_ArmorPoints(const MC_World *w);
unsigned MC_XpForLevel(unsigned level);
unsigned MC_XpLevel(const MC_World *w);
unsigned MC_XpProgress(const MC_World *w);
unsigned MC_XpToNext(const MC_World *w);
unsigned MC_AddExperience(MC_World *w,unsigned amount);
unsigned MC_AttackDamage(const MC_World *w,unsigned weapon);
int MC_EquipArmor(MC_World *w,unsigned inventory_slot);
int MC_UnequipArmor(MC_World *w,unsigned armor_slot);
int MC_MoveStack(MC_Stack *from,MC_Stack *to,int split);
int MC_ContainerTap(MC_World *w,unsigned slot);
void MC_SystemsTick(MC_World *w,float dt);
void MC_ProjectilesTick(MC_World *w,float dt);
void MC_RedstoneRefresh(MC_World *w);
int MC_OpenTarget(MC_World *w);
int MC_TransferSlot(MC_World *w,unsigned slot,int shift);
const char *MC_ItemName(unsigned item);
uint16_t MC_ItemColor(unsigned item);
unsigned MC_RecipeCount(void);
const char *MC_RecipeName(unsigned recipe);
const char *MC_RecipeCost(unsigned recipe);
const char *MC_RecipeStatus(const MC_World *w,unsigned recipe);
unsigned MC_RecipeResult(unsigned recipe);
unsigned MC_RecipeIngredient(unsigned recipe,unsigned part);
unsigned MC_RecipeIngredientCount(unsigned recipe,unsigned part);
void MC_SelectItem(MC_World *w, unsigned item);
/* Long press restarts after drag motion exceeds 12 LCD pixels. */
typedef struct { int x,y,anchor_x,anchor_y; uint32_t since; uint8_t moved; } MC_Hold;
void MC_HoldStart(MC_Hold *h,int x,int y,uint32_t now);
void MC_HoldMove(MC_Hold *h,int x,int y,uint32_t now);
int MC_HoldReady(const MC_Hold *h,uint32_t now);
int MC_HoldTap(const MC_Hold *h,uint32_t now);

#endif
