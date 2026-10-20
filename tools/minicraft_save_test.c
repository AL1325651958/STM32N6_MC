#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "minicraft.h"
#include "mc_save.h"
static void put32(uint8_t *p,uint32_t n){for(int i=0;i<4;i++)p[i]=(uint8_t)(n>>(i*8));}
static void put16(uint8_t *p,unsigned n){p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);}
static void putfloat(uint8_t *p,float f){uint32_t u;memcpy(&u,&f,4);put32(p,u);}
static void crc(uint8_t *p,unsigned len){put32(p+48,MC_Crc32(p+MC_SAVE_V1_SIZE,len));put32(p+52,len);put32(p+60,MC_Crc32(p,60));}
static void roundtrip(MC_World *a,MC_World *b,uint8_t *p){MC_SaveEncode(a,4,p);assert(MC_SaveValidate(p,MC_SAVE_SIZE)==4);assert(MC_SaveDecode(b,p,MC_SAVE_SIZE));}
static void invalid(MC_World *w,uint8_t *p,uint8_t *copy,unsigned offset,unsigned value){
 memcpy(p,copy,MC_SAVE_SIZE);p[MC_SAVE_V1_SIZE+offset]=(uint8_t)value;crc(p,MC_SAVE_META);
 assert(!MC_SaveValidate(p,MC_SAVE_SIZE));uint32_t before=MC_Crc32(w->blocks,MC_MAP_BYTES);float x=w->x;
 assert(!MC_SaveDecode(w,p,MC_SAVE_SIZE));assert(x==w->x&&before==MC_Crc32(w->blocks,MC_MAP_BYTES));
}
int main(int argc,char **argv) {
 MC_World a={.blocks=calloc(1,MC_MAP_BYTES)},b={.blocks=calloc(1,MC_MAP_BYTES)};
 uint8_t *save=calloc(1,MC_SAVE_SIZE),*copy=malloc(MC_SAVE_SIZE);assert(a.blocks&&b.blocks&&save&&copy);
 MC_Generate(&a,77);a.x=12.5f;a.y=22;a.z=19.5f;a.yaw=2.1f;a.pitch=-.4f;
 assert(MC_Give(&a,MC_IRON,99));assert(MC_Give(&a,MC_STONE_PICK,1));a.slots[2].wear=71;a.equipped=MC_STONE_PICK;
 a.slot=3;a.health=13;a.air=7;a.time=599.75f;a.reserve[MC_WOOD]=999;
 a.reserve[MC_WOOD_PICK]=2;a.reserve_wear[0]=17;
 MC_Set(&a,30,40,30,MC_CHEST);a.chest_map[0][0]=31;a.chest_map[0][1]=41;a.chest_map[0][2]=31;
 a.chest_blocks[0][2]=(MC_Stack){MC_AXE,1,17};a.chest_blocks[0][23]=(MC_Stack){MC_PLANKS,64,0};
 MC_Set(&a,31,40,30,MC_FURNACE);a.furnaces[0]=(MC_Furnace){32,41,31,{MC_IRON_ORE,10,0},{MC_COAL,2,0},{MC_IRON,4,0},21.5f,2.5f};
 MC_Set(&a,35,40,30,MC_SAPLING);a.growers[0]=(MC_Grower){36,41,31,78.5f};
 MC_Set(&a,50,40,30,MC_FARMLAND);MC_Set(&a,50,41,30,MC_WHEAT_CROP);a.growers[1]=(MC_Grower){51,42,31,32.5f};
 MC_Set(&a,36,40,30,MC_BED);a.spawn[0]=37;a.spawn[1]=41;a.spawn[2]=31;
 MC_Set(&a,40,40,30,MC_DOOR);MC_Set(&a,41,40,30,MC_DOOR_OPEN);
 roundtrip(&a,&b,save);assert(b.seed==a.seed&&b.x==a.x&&!memcmp(a.blocks,b.blocks,MC_MAP_BYTES));
 assert(MC_Get(&b,40,40,30)==MC_DOOR&&MC_Get(&b,41,40,30)==MC_DOOR_OPEN);
 assert(b.items[MC_IRON]==99&&b.equipped==MC_STONE_PICK&&b.slots[2].wear==71&&b.slot==3&&b.health==13&&b.air==7&&b.time==a.time);
 assert(!memcmp(a.slots,b.slots,sizeof(a.slots))&&!memcmp(a.chest_blocks,b.chest_blocks,sizeof(a.chest_blocks)));
 assert(!memcmp(a.chest_map,b.chest_map,sizeof(a.chest_map))&&!memcmp(a.spawn,b.spawn,sizeof(a.spawn))&&b.reserve[MC_WOOD]==999);
 assert(b.reserve[MC_WOOD_PICK]==2&&b.reserve_wear[0]==17);
 assert(b.furnaces[0].progress==2.5f&&b.furnaces[0].burn==21.5f&&b.growers[0].age==78.5f);
 assert(MC_Get(&b,50,40,30)==MC_FARMLAND&&MC_Get(&b,50,41,30)==MC_WHEAT_CROP&&b.growers[1].age==32.5f);
 /* A v3 file has identical legacy metadata but no entities. */
 memcpy(copy,save,MC_SAVE_SIZE);put32(save+4,3);crc(save,MC_SAVE_V3_META);
 assert(MC_SaveDecode(&b,save,MC_SAVE_V3_SIZE));
 assert(!memcmp(a.slots,b.slots,sizeof(a.slots))&&b.mob_rng&&!b.mobs[0].health);
 memcpy(save,copy,MC_SAVE_SIZE);
 for(int i=0;i<26;i++){MC_SystemsTick(&b,.1f);}
 assert(b.furnaces[0].output.count==5&&b.furnaces[0].input.count==9);
 memcpy(copy,save,MC_SAVE_SIZE);save[MC_SAVE_SIZE-1]^=1;assert(!MC_SaveValidate(save,MC_SAVE_SIZE));memcpy(save,copy,MC_SAVE_SIZE);
 save[MC_SAVE_HEADER+12000]^=1;assert(!MC_SaveValidate(save,MC_SAVE_SIZE));memcpy(save,copy,MC_SAVE_SIZE);
 for(unsigned n=0;n<64;n++)assert(!MC_SaveValidate(save,n));
 assert(!MC_SaveValidate(save,MC_SAVE_SIZE-1));assert(!MC_SaveValidate(save,MC_SAVE_SIZE+1));
 invalid(&b,save,copy,2,8);invalid(&b,save,copy,5,MC_APPLE);invalid(&b,save,copy,81,65);invalid(&b,save,copy,80,MC_AIR);
 invalid(&b,save,copy,80+2*4+2,251);invalid(&b,save,copy,792,255);invalid(&b,save,copy,828+10,MC_STICK);
 invalid(&b,save,copy,74,61);
 invalid(&b,save,copy,1144+24,MC_MOB_HEALTH+1);
 invalid(&b,save,copy,1144+25,2);
 memcpy(save,copy,MC_SAVE_SIZE);putfloat(save+MC_SAVE_V1_SIZE+8,NAN);crc(save,MC_SAVE_META);assert(!MC_SaveValidate(save,MC_SAVE_SIZE));
 /* Synthetic v1 migration preserves every terrain byte. */
 MC_Generate(&a,77);for(unsigned i=0;i<MC_MAP_BYTES;i++)if(a.blocks[i]>=9)a.blocks[i]=MC_STONE;
 MC_SaveEncode(&a,8,save);put32(save+4,1);put32(save+48,0);put32(save+52,0);put32(save+60,MC_Crc32(save,60));
 assert(MC_SaveDecode(&b,save,MC_SAVE_V1_SIZE));assert(b.creative&&b.health==20&&!memcmp(a.blocks,b.blocks,MC_MAP_BYTES));
 /* v2 preserves aggregate counts (including overflow), old hotbar and tool wear. */
 MC_Generate(&a,78);MC_SaveEncode(&a,9,save);put32(save+4,2);uint8_t *m=save+MC_SAVE_V1_SIZE;memset(m,0,MC_SAVE_META);
 for(unsigned id=1;id<25;id++)if(id!=MC_BEDROCK&&id!=MC_WATER)put16(m+id*2,id>=MC_WOOD_PICK&&id<=MC_IRON_PICK?2:999);
 const uint8_t bar[8]={MC_WOOD,MC_DIRT,MC_COBBLE,MC_PLANKS,MC_TABLE,MC_TORCH,MC_GLASS,MC_STONE_PICK};memcpy(m+50,bar,8);
 m[58]=7;m[60]=17;m[61]=8;m[62]=MC_STONE_PICK;put16(m+64,39);put16(m+66,71);put16(m+68,120);putfloat(m+72,399.5f);crc(save,MC_SAVE_V2_META);
 assert(MC_SaveValidate(save,MC_SAVE_V2_SIZE)==9);assert(MC_SaveDecode(&b,save,MC_SAVE_V2_SIZE));
 for(unsigned id=1;id<25;id++)if(id!=MC_BEDROCK&&id!=MC_WATER)assert(b.items[id]+b.reserve[id]==(unsigned)(id>=MC_WOOD_PICK&&id<=MC_IRON_PICK?2:999));
 assert(b.slots[7].item==MC_STONE_PICK&&b.slots[7].wear==71&&b.health==17&&b.slot==7&&b.time==399.5f);
 roundtrip(&b,&a,copy);assert(a.reserve[MC_WOOD]==b.reserve[MC_WOOD]);
 /* Real SD files are opened strictly read-only; migrate in memory only. */
 for(int i=1;i<argc;i++) {
   FILE *f=fopen(argv[i],"rb");assert(f);size_t n=fread(save,1,MC_SAVE_SIZE,f);assert(!ferror(f));assert(fgetc(f)==EOF);fclose(f);
   assert((n==MC_SAVE_V1_SIZE||n==MC_SAVE_V2_SIZE||n==MC_SAVE_V3_SIZE||n==MC_SAVE_SIZE)&&MC_SaveValidate(save,(unsigned)n));
   uint32_t before=MC_Crc32(save+MC_SAVE_HEADER,MC_MAP_BYTES);assert(MC_SaveDecode(&b,save,(unsigned)n));
   if(n==MC_SAVE_V1_SIZE)assert(b.creative&&b.health==20);
   MC_SaveEncode(&b,12,copy);assert(MC_SaveValidate(copy,MC_SAVE_SIZE)==12);assert(MC_Crc32(copy+MC_SAVE_HEADER,MC_MAP_BYTES)==before);
   printf("Legacy save roundtrip, identical terrain: %s\n",argv[i]);
 }
 free(copy);free(save);free(a.blocks);free(b.blocks);puts("minicraft v1/v2/v3/v4 save / corruption / migration / persistent systems: OK");return 0;
}
