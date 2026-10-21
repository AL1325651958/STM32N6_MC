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
 MC_Generate(&a,77);
 /* Save-button state is transient, cannot affect encoded bytes, and cannot
    survive loading an existing world. */
 MC_SaveEncode(&a,4,copy);MC_NotifySave(&a,MC_SAVE_OK);MC_SaveEncode(&a,4,save);
 assert(!memcmp(copy,save,MC_SAVE_SIZE));
 MC_NotifySave(&b,MC_SAVE_VERIFY_FAILED);assert(MC_SaveDecode(&b,save,MC_SAVE_SIZE));
 assert(b.save_feedback==MC_SAVE_IDLE&&!b.save_feedback_time);
 MC_Generate(&a,77);assert(a.save_feedback==MC_SAVE_IDLE);
 /* Finished food in a registered furnace survives a real save roundtrip. */
 MC_Set(&a,50,40,30,MC_FURNACE);
 a.furnaces[0]=(MC_Furnace){51,41,31,{MC_RAW_MEAT,2,0},{MC_COAL,1,0},{MC_COOKED_MEAT,1,0},3,1};
 roundtrip(&a,&b,save);assert(b.furnaces[0].output.item==MC_COOKED_MEAT);
 MC_Generate(&a,77);

 uint16_t ruin_coords[2][3];
 for(unsigned c=0;c<2;c++) {
   memcpy(ruin_coords[c],a.chest_map[c],sizeof(ruin_coords[c]));
   assert(ruin_coords[c][0]&&ruin_coords[c][1]&&ruin_coords[c][2]);
   int cx=ruin_coords[c][0]-1,h=ruin_coords[c][1]-3,cz=ruin_coords[c][2]-2;
   assert(MC_Get(&a,cx,h+2,cz-3)==MC_AIR);
   assert(MC_Get(&a,cx,h+3,cz-3)==MC_AIR);
   assert(MC_Get(&a,cx,h+4,cz-3)==MC_AIR);
   assert(MC_Get(&a,cx,h+2,cz+1)==MC_CHEST);
   assert(a.chest_blocks[c][0].item==MC_TORCH&&a.chest_blocks[c][0].count==8);
   assert(a.chest_blocks[c][1].item==MC_COAL&&a.chest_blocks[c][1].count==4);
   assert(a.chest_blocks[c][2].item==MC_APPLE&&a.chest_blocks[c][2].count==2);
   assert(a.chest_blocks[c][3].item==MC_STONE_PICK&&a.chest_blocks[c][3].count==1);
 }
 roundtrip(&a,&b,save);
 assert(!memcmp(a.chest_map,b.chest_map,sizeof(a.chest_map)));
 assert(!memcmp(a.chest_blocks,b.chest_blocks,sizeof(a.chest_blocks)));
 MC_Generate(&b,77);
 assert(!memcmp(a.chest_map,b.chest_map,sizeof(a.chest_map)));
 a.x=12.5f;a.y=22;a.z=19.5f;a.yaw=2.1f;a.pitch=-.4f;
 assert(MC_Give(&a,MC_IRON,99));assert(MC_Give(&a,MC_STONE_PICK,1));a.slots[2].wear=71;a.equipped=MC_STONE_PICK;
 a.slot=3;a.health=13;a.air=7;a.hunger=7;a.time=599.75f;a.reserve[MC_WOOD]=999;
 a.experience=MC_XpForLevel(5)+2;a.level=(uint8_t)MC_XpLevel(&a);
 a.armor[0]=(MC_Stack){MC_IRON_HELMET,1,73};
 a.reserve[MC_WOOD_PICK]=2;a.reserve_wear[0]=17;
 assert(MC_Give(&a,MC_BIRCH_WOOD,3)&&MC_Give(&a,MC_DARK_PLANKS,5));
 MC_Set(&a,29,40,30,MC_CHEST);a.chest_map[0][0]=30;a.chest_map[0][1]=41;a.chest_map[0][2]=31;
 a.chest_blocks[0][2]=(MC_Stack){MC_AXE,1,17};a.chest_blocks[0][23]=(MC_Stack){MC_PLANKS,64,0};
 MC_Set(&a,31,40,30,MC_FURNACE);a.furnaces[0]=(MC_Furnace){32,41,31,{MC_IRON_ORE,10,0},{MC_COAL,2,0},{MC_IRON,4,0},21.5f,2.5f};
 MC_Set(&a,33,40,30,MC_FURNACE);a.furnaces[1]=(MC_Furnace){34,41,31,{0},{MC_BIRCH_WOOD,1,0},{0},0,0};
 MC_Set(&a,35,40,30,MC_SAPLING);a.growers[0]=(MC_Grower){36,41,31,78.5f};
 MC_Set(&a,50,40,30,MC_FARMLAND);MC_Set(&a,50,41,30,MC_WHEAT_CROP);a.growers[1]=(MC_Grower){51,42,31,32.5f};
 a.crop_clock=.7f;b.crop_clock=.9f;
 MC_Set(&a,36,40,30,MC_BED);a.spawn[0]=37;a.spawn[1]=41;a.spawn[2]=31;
 MC_Set(&a,42,40,30,MC_DOUBLE_SLAB);
 MC_Set(&a,40,40,30,MC_DOOR);MC_Set(&a,41,40,30,MC_DOOR_OPEN);
 roundtrip(&a,&b,save);assert(b.seed==a.seed&&b.x==a.x&&!memcmp(a.blocks,b.blocks,MC_MAP_BYTES));
 assert(MC_Get(&b,42,40,30)==MC_DOUBLE_SLAB);
 assert(MC_Get(&b,40,40,30)==MC_DOOR&&MC_Get(&b,41,40,30)==MC_DOOR_OPEN);
 assert(b.items[MC_IRON]==99&&b.equipped==MC_STONE_PICK&&b.slots[2].wear==71&&b.slot==3&&b.health==13&&b.air==7&&b.hunger==7&&b.time==a.time);
 assert(b.armor[0].item==MC_IRON_HELMET&&b.armor[0].count==1&&b.armor[0].wear==73);
 assert(b.experience==a.experience&&b.level==5&&MC_XpProgress(&b)==2);
 assert(!memcmp(a.slots,b.slots,sizeof(a.slots))&&!memcmp(a.chest_blocks,b.chest_blocks,sizeof(a.chest_blocks)));
 assert(!memcmp(a.chest_map,b.chest_map,sizeof(a.chest_map))&&!memcmp(a.spawn,b.spawn,sizeof(a.spawn))&&b.reserve[MC_WOOD]==999);
 assert(b.items[MC_BIRCH_WOOD]==3&&b.items[MC_DARK_PLANKS]==5);
 assert(b.reserve[MC_WOOD_PICK]==2&&b.reserve_wear[0]==17);
 assert(b.furnaces[0].progress==2.5f&&b.furnaces[0].burn==21.5f&&b.furnaces[1].fuel.item==MC_BIRCH_WOOD&&b.growers[0].age==78.5f);
 assert(MC_Get(&b,50,40,30)==MC_FARMLAND&&MC_Get(&b,50,41,30)==MC_WHEAT_CROP&&b.growers[1].age==32.5f);
 assert(b.crop_clock==0);
 /* A loaded dry crop keeps its saved progress, then resumes when irrigated. */
 for(int z=26;z<=34;z++)for(int x=46;x<=54;x++)if(MC_Get(&b,x,40,z)==MC_WATER)MC_Set(&b,x,40,z,MC_AIR);
 for(int i=0;i<20;i++)MC_SystemsTick(&b,.1f);
 assert(b.growers[1].age==32.5f);
 MC_Set(&b,54,39,30,MC_STONE);MC_Set(&b,54,40,30,MC_WATER);b.time=90;
 for(int y=42;y<MC_MAP_Y;y++)MC_Set(&b,50,y,30,MC_AIR);
 for(int i=0;i<20;i++)MC_SystemsTick(&b,.1f);
 assert(b.growers[1].age>34&&b.growers[1].age<35);
 assert(MC_SaveDecode(&b,save,MC_SAVE_SIZE)&&b.crop_clock==0&&b.growers[1].age==32.5f);
 /* Old v4 metadata has no hunger marker and therefore loads with full hunger. */
 memcpy(copy,save,MC_SAVE_SIZE);uint8_t *meta=save+MC_SAVE_V1_SIZE;meta[6]=0;meta[7]=0;meta[MC_SAVE_META-95]=0;crc(save,MC_SAVE_META);
 assert(MC_SaveValidate(save,MC_SAVE_SIZE)==4);assert(MC_SaveDecode(&b,save,MC_SAVE_SIZE)&&b.hunger==20);
 assert(b.experience==0&&b.level==0);
 memcpy(save,copy,MC_SAVE_SIZE);meta=save+MC_SAVE_V1_SIZE;meta[6]=21;meta[7]=1;crc(save,MC_SAVE_META);assert(!MC_SaveValidate(save,MC_SAVE_SIZE));
 memcpy(save,copy,MC_SAVE_SIZE);
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
 invalid(&b,save,copy,74,61);invalid(&b,save,copy,80,MC_DOUBLE_SLAB);
 invalid(&b,save,copy,1420+1,2);invalid(&b,save,copy,1420+2,0);
 invalid(&b,save,copy,1144+24,MC_MOB_HEALTH+1);
 invalid(&b,save,copy,1144+25,2);
 memcpy(save,copy,MC_SAVE_SIZE);putfloat(save+MC_SAVE_V1_SIZE+8,NAN);crc(save,MC_SAVE_META);assert(!MC_SaveValidate(save,MC_SAVE_SIZE));
 /* Synthetic v1 migration preserves every terrain byte. */
 MC_Generate(&a,77);for(unsigned i=0;i<MC_MAP_BYTES;i++)if(a.blocks[i]>=9)a.blocks[i]=MC_STONE;
 MC_SaveEncode(&a,8,save);put32(save+4,1);put32(save+48,0);put32(save+52,0);put32(save+60,MC_Crc32(save,60));
 assert(MC_SaveDecode(&b,save,MC_SAVE_V1_SIZE));assert(b.creative&&b.health==20&&!memcmp(a.blocks,b.blocks,MC_MAP_BYTES));
 /* v2 preserves aggregate counts (including overflow), old hotbar and tool wear. */
 MC_Generate(&a,78);for(unsigned i=0;i<MC_MAP_BYTES;i++)if(a.blocks[i]>=18)a.blocks[i]=MC_STONE;
 MC_SaveEncode(&a,9,save);put32(save+4,2);uint8_t *m=save+MC_SAVE_V1_SIZE;memset(m,0,MC_SAVE_META);
 for(unsigned id=1;id<25;id++)if(id!=MC_BEDROCK&&id!=MC_WATER)put16(m+id*2,id>=MC_WOOD_PICK&&id<=MC_IRON_PICK?2:999);
 const uint8_t bar[8]={MC_WOOD,MC_DIRT,MC_COBBLE,MC_PLANKS,MC_TABLE,MC_TORCH,MC_GLASS,MC_STONE_PICK};memcpy(m+50,bar,8);
 m[58]=7;m[60]=17;m[61]=8;m[62]=MC_STONE_PICK;put16(m+64,39);put16(m+66,71);put16(m+68,120);putfloat(m+72,399.5f);crc(save,MC_SAVE_V2_META);
 assert(MC_SaveValidate(save,MC_SAVE_V2_SIZE)==9);assert(MC_SaveDecode(&b,save,MC_SAVE_V2_SIZE));
 for(unsigned id=1;id<25;id++)if(id!=MC_BEDROCK&&id!=MC_WATER)assert(b.items[id]+b.reserve[id]==(unsigned)(id>=MC_WOOD_PICK&&id<=MC_IRON_PICK?2:999));
 assert(b.slots[7].item==MC_STONE_PICK&&b.slots[7].wear==71&&b.health==17&&b.slot==7&&b.time==399.5f);
 roundtrip(&b,&a,copy);assert(a.reserve[MC_WOOD]==b.reserve[MC_WOOD]);
 /* A real legacy-dimension v4 layout migrates without relocating player,
    containers, or any byte in its original 128x64x128 terrain square. */
 MC_GenerateTerrain(&a,123);MC_NewPlayer(&a,0);a.x=a.z=10.5f;a.y=40;
 MC_Set(&a,120,40,120,MC_CHEST);a.chest_map[0][0]=121;a.chest_map[0][1]=41;a.chest_map[0][2]=121;
 a.chest_blocks[0][0]=(MC_Stack){MC_WOOD,7,0};MC_SaveEncode(&a,11,save);
 const unsigned old_bytes=128u*64u*128u,old_base=64u+old_bytes,old_length=old_base+MC_SAVE_META;
 uint8_t *legacy=calloc(1,old_length);assert(legacy);memcpy(legacy,save,64);
 for(unsigned y=0;y<64;y++)for(unsigned z=0;z<128;z++)
   memcpy(legacy+64+(y*128+z)*128,save+64+(y*MC_MAP_Z+z)*MC_MAP_X,128);
 memcpy(legacy+old_base,save+MC_SAVE_V1_SIZE,MC_SAVE_META);
 put32(legacy+4,4);put32(legacy+8,128);put32(legacy+16,128);
 put32(legacy+56,MC_Crc32(legacy+64,old_bytes));put32(legacy+60,MC_Crc32(legacy,60));
 assert(MC_SaveValidate(legacy,old_length)==11&&MC_SaveDecode(&b,legacy,old_length));
 for(unsigned y=0;y<64;y++)for(unsigned z=0;z<128;z++)
   assert(!memcmp(legacy+64+(y*128+z)*128,b.blocks+(y*MC_MAP_Z+z)*MC_MAP_X,128));
 assert(b.x==10.5f&&b.z==10.5f&&b.chest_blocks[0][0].count==7&&MC_Get(&b,200,1,200)!=MC_AIR);
 MC_SaveEncode(&b,12,copy);assert(copy[4]==5&&MC_SaveValidate(copy,MC_SAVE_SIZE)==12);
 put16(legacy+old_base+792,129);put32(legacy+48,MC_Crc32(legacy+old_base,MC_SAVE_META));
 put32(legacy+60,MC_Crc32(legacy,60));assert(!MC_SaveValidate(legacy,old_length));free(legacy);
 /* Real SD files are opened strictly read-only; migrate in memory only. */
 for(int i=1;i<argc;i++) {
   FILE *f=fopen(argv[i],"rb");assert(f);size_t n=fread(save,1,MC_SAVE_SIZE,f);assert(!ferror(f));assert(fgetc(f)==EOF);fclose(f);
   assert(MC_SaveValidate(save,(unsigned)n));
   unsigned mx=save[8]|(unsigned)save[9]<<8,mz=save[16]|(unsigned)save[17]<<8;
   unsigned version=save[4];assert(MC_SaveDecode(&b,save,(unsigned)n));
   if(version==1)assert(b.creative&&b.health==20);
   MC_SaveEncode(&b,12,copy);assert(MC_SaveValidate(copy,MC_SAVE_SIZE)==12);
   for(unsigned y=0;y<MC_MAP_Y;y++)for(unsigned z=0;z<mz;z++)
     assert(!memcmp(save+MC_SAVE_HEADER+(y*mz+z)*mx,copy+MC_SAVE_HEADER+(y*MC_MAP_Z+z)*MC_MAP_X,mx));
   printf("Legacy save roundtrip, identical terrain: %s\n",argv[i]);
 }
 free(copy);free(save);free(a.blocks);free(b.blocks);puts("minicraft v1/v2/v3/v4/v5 save / corruption / migration / persistent systems: OK");return 0;
}
