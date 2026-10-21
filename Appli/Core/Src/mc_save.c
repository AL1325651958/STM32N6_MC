#include "mc_save.h"
const char *MC_WorldFile(unsigned bank,unsigned slot)
{
  static const char *names[2][2]={{"MC0.SAV","MC1.SAV"},{"MC2.SAV","MC3.SAV"}};
  return bank<2&&slot<2?names[bank][slot]:0;
}
int MC_DeleteWorld(unsigned bank,const MC_Storage *s)
{
  if(bank>=2||!s||!s->remove||!s->sync||!s->absent)return 0;
  for(unsigned slot=0;slot<2;slot++)if(s->remove(s->context,MC_WorldFile(bank,slot))) {
    s->sync(s->context);return 0;
  }
  if(s->sync(s->context))return 0;
  for(unsigned slot=0;slot<2;slot++)if(s->absent(s->context,MC_WorldFile(bank,slot))!=1)return 0;
  return 1;
}
#include <math.h>
#include <string.h>
/* Versioned metadata offsets are fixed, independent of C struct padding. */
enum { M_SLOTS=80,M_BAR=208,M_CHESTS=216,M_COORD=792,M_FURNACES=828,M_GROWERS=984,M_MOBS=1144,M_MOB_STATE=1400,M_ARMOR=1420,M_XP=1436 };
_Static_assert(M_SLOTS+MC_SLOTS*4==M_BAR,"slot layout");
_Static_assert(M_BAR+MC_HOTBAR==M_CHESTS,"bar layout");
_Static_assert(M_CHESTS+MC_CHESTS*MC_CHEST_SLOTS*4==M_COORD,"chest layout");
_Static_assert(M_COORD+MC_CHESTS*6==M_FURNACES,"coordinate layout");
_Static_assert(M_FURNACES+MC_FURNACES*26==M_GROWERS,"furnace layout");
_Static_assert(M_GROWERS+MC_GROWERS*10==M_MOBS,"grower layout");
_Static_assert(M_MOBS+MC_MOBS*32==M_MOB_STATE,"mob layout");
_Static_assert(M_ARMOR+MC_ARMOR_SLOTS*4<=MC_SAVE_META,"metadata overflow");
_Static_assert(M_XP+6<=MC_SAVE_META,"experience metadata overflow");
static uint32_t get32(const uint8_t*p){return(uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put32(uint8_t*p,uint32_t v){for(int i=0;i<4;i++)p[i]=(uint8_t)(v>>(i*8));}
static uint16_t get16(const uint8_t*p){return(uint16_t)(p[0]|(uint16_t)p[1]<<8);}
static void put16(uint8_t*p,uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static float getfloat(const uint8_t*p){uint32_t v=get32(p);float f;memcpy(&f,&v,4);return f;}
static void putfloat(uint8_t*p,float f){uint32_t v;memcpy(&v,&f,4);put32(p,v);}
static void swrite(uint8_t*p,const MC_Stack*s){p[0]=s->item;p[1]=s->count;put16(p+2,s->wear);}
static void sread(MC_Stack*s,const uint8_t*p){s->item=p[0];s->count=p[1];s->wear=get16(p+2);}
static int svalid(const uint8_t*p){
  unsigned id=p[0],n=p[1],wear=get16(p+2);
  if(!n)return id==0&&wear==0;
  if(!id||id>=MC_ITEM_COUNT||(MC_DoorUpper((uint8_t)id)||id==MC_DOUBLE_SLAB||id==MC_LAVA||id==MC_FARMLAND||id==MC_WHEAT_CROP||id==MC_WHEAT_MATURE||id==MC_DUST_ON||id==MC_LAMP_ON||id==MC_LEVER_ON||id==MC_BUTTON_ON)||id==MC_WATER||id==MC_BEDROCK||n>64)return 0;
  int t=MC_ToolIndex(id),a=MC_ArmorIndex(id);
  if(t>=0)return n==1&&wear>0&&wear<=MC_ToolLife(id);
  if(a>=0)return n==1&&wear>0&&wear<=MC_ArmorLife(id);
  return wear==0;
}
static int coord(const uint8_t*p,unsigned mx,unsigned mz){unsigned x=get16(p),y=get16(p+2),z=get16(p+4);return !x?(!y&&!z):(x<=mx&&y>=2&&y<=MC_MAP_Y&&z>=1&&z<=mz);}
static int range(float f,float lo,float hi){return isfinite(f)&&f>=lo&&f<=hi;}

void MC_SaveEncode(const MC_World*w,uint32_t generation,uint8_t*p){
  memset(p,0,MC_SAVE_HEADER);memcpy(p,"N6MC",4);put32(p+4,MC_SAVE_VERSION);put32(p+8,MC_MAP_X);put32(p+12,MC_MAP_Y);put32(p+16,MC_MAP_Z);put32(p+20,generation);put32(p+24,w->seed);
  putfloat(p+28,w->x);putfloat(p+32,w->y);putfloat(p+36,w->z);putfloat(p+40,w->yaw);putfloat(p+44,w->pitch);memcpy(p+MC_SAVE_HEADER,w->blocks,MC_MAP_BYTES);
  uint8_t*m=p+MC_SAVE_V1_SIZE;memset(m,0,MC_SAVE_META);m[0]=1;m[1]=w->creative;m[2]=w->slot;m[3]=w->health;m[4]=w->air;m[5]=w->equipped;m[6]=w->hunger;m[7]=1;
  putfloat(m+8,w->time);putfloat(m+12,w->fall_speed);
  for(unsigned i=0;i<3;i++)put16(m+16+i*2,w->spawn[i]);
  for(unsigned i=0;i<25;i++)put16(m+24+i*2,w->reserve[i]);
  for(unsigned i=0;i<3;i++)put16(m+74+i*2,w->reserve_wear[i]);
  for(unsigned i=0;i<MC_SLOTS;i++)swrite(m+M_SLOTS+i*4,&w->slots[i]);
  memcpy(m+M_BAR,w->creative_bar,MC_HOTBAR);
  for(unsigned c=0;c<MC_CHESTS;c++){
    for(unsigned i=0;i<MC_CHEST_SLOTS;i++)swrite(m+M_CHESTS+(c*MC_CHEST_SLOTS+i)*4,&w->chest_blocks[c][i]);
    for(unsigned k=0;k<3;k++)put16(m+M_COORD+c*6+k*2,w->chest_map[c][k]);
  }
  for(unsigned i=0;i<MC_FURNACES;i++){
    uint8_t*q=m+M_FURNACES+i*26;const MC_Furnace*f=&w->furnaces[i];put16(q,f->x);put16(q+2,f->y);put16(q+4,f->z);swrite(q+6,&f->input);swrite(q+10,&f->fuel);swrite(q+14,&f->output);putfloat(q+18,f->burn);putfloat(q+22,f->progress);
  }
  for(unsigned i=0;i<MC_GROWERS;i++){uint8_t*q=m+M_GROWERS+i*10;put16(q,w->growers[i].x);put16(q+2,w->growers[i].y);put16(q+4,w->growers[i].z);putfloat(q+6,w->growers[i].age);}
  for(unsigned i=0;i<MC_MOBS;i++) {
    uint8_t*q=m+M_MOBS+i*32;const MC_Mob*a=&w->mobs[i];if(!a->health)continue;putfloat(q,a->x);putfloat(q+4,a->y);putfloat(q+8,a->z);putfloat(q+12,a->vy);putfloat(q+16,a->attack);putfloat(q+20,a->hurt);q[24]=a->health;q[25]=a->on_ground;q[26]=a->type;putfloat(q+28,a->heading);
  }
  put32(m+M_MOB_STATE,w->mob_rng);putfloat(m+M_MOB_STATE+4,w->mob_clock);putfloat(m+M_MOB_STATE+8,w->spawn_clock);
  putfloat(m+M_MOB_STATE+12,w->attack_cooldown);putfloat(m+M_MOB_STATE+16,w->attack_flash);
  for(unsigned i=0;i<MC_ARMOR_SLOTS;i++)swrite(m+M_ARMOR+i*4,&w->armor[i]);
  put32(m+M_XP,w->experience>MC_XpForLevel(MC_MAX_LEVEL)?MC_XpForLevel(MC_MAX_LEVEL):w->experience);m[M_XP+4]=(uint8_t)MC_XpLevel(w);m[M_XP+5]=1;
  memcpy(m+MC_SAVE_META,w->explored,MC_EXPLORE_BYTES);
  put32(p+48,MC_Crc32(m,MC_SAVE_META+MC_EXPLORE_BYTES));put32(p+52,MC_SAVE_META+MC_EXPLORE_BYTES);put32(p+56,MC_Crc32(p+MC_SAVE_HEADER,MC_MAP_BYTES));put32(p+60,MC_Crc32(p,60));
}
static uint8_t saved_block(const uint8_t*p,const uint8_t*q,unsigned mx,unsigned mz){return p[MC_SAVE_HEADER+((get16(q+2)-1)*mz+get16(q+4)-1)*mx+get16(q)-1];}
uint32_t MC_SaveValidate(const uint8_t*p,unsigned length){
  if(length<MC_SAVE_HEADER)return 0;
  unsigned v=get32(p+4),mx=get32(p+8),my=get32(p+12),mz=get32(p+16);
  if(memcmp(p,"N6MC",4)||v<1||v>MC_SAVE_VERSION||my!=MC_MAP_Y||mx!=mz||(mx!=128&&mx!=MC_MAP_X)||!get32(p+20))return 0;
  unsigned bytes=mx*my*mz,base=MC_SAVE_HEADER+bytes;
  unsigned metadata=v==1?0:v==2?MC_SAVE_V2_META:v==3?MC_SAVE_V3_META:v>=9?MC_SAVE_META+MC_EXPLORE_BYTES:MC_SAVE_META;
  if(length!=base+metadata)return 0;
  if(get32(p+60)!=MC_Crc32(p,60)||get32(p+56)!=MC_Crc32(p+MC_SAVE_HEADER,bytes))return 0;
  if(!range(getfloat(p+28),.28f,mx-.28f)||!range(getfloat(p+32),1,MC_MAP_Y+4)||!range(getfloat(p+36),.28f,mz-.28f)||!range(getfloat(p+40),-100,100)||!range(getfloat(p+44),-1.4f,1.4f))return 0;
  /* Validate door pairs during the existing block pass, avoiding another
     four-MiB scan. Preserve legacy lone lower doors under existing builds. */
  for(unsigned i=0;i<bytes;i++) {
    uint8_t b=p[MC_SAVE_HEADER+i];
    if(v==1?b>=9:v==2?b>=18:(b!=MC_AIR&&!MC_IsBlock(b)))return 0;
    if(v<7&&b>=MC_DOOR_ROTATED_FIRST)return 0;
    if(v<8&&b==MC_LAVA)return 0;
    if(MC_DoorUpper(b)) {
      if(v<6 || i<mx*mz || p[MC_SAVE_HEADER+i-mx*mz]!=MC_DoorBase(b))return 0;
    } else if(b>=MC_DOOR_ROTATED_FIRST&&MC_IsDoor(b)) {
      if(i+mx*mz>=bytes || p[MC_SAVE_HEADER+i+mx*mz]!=MC_DoorState(MC_DoorFacing(b),MC_DoorOpen(b),1))return 0;
    }
  }
  for(unsigned i=0;i<mx*mz;i++)if(p[MC_SAVE_HEADER+i]!=MC_BEDROCK)return 0;
  if(v>=2){
    const uint8_t*m=p+base;unsigned len=v==2?MC_SAVE_V2_META:v==3?MC_SAVE_V3_META:MC_SAVE_META;
    if(v>=9)len=MC_SAVE_META+MC_EXPLORE_BYTES;
    if(get32(p+52)!=len||get32(p+48)!=MC_Crc32(m,len))return 0;
    if(v==2){
      for(unsigned i=0;i<25;i++)if(get16(m+i*2)>999)return 0;
      for(unsigned i=0;i<MC_HOTBAR;i++)if(m[50+i]>=25)return 0;
      if(m[58]>=MC_HOTBAR||m[59]>1||m[60]==0||m[60]>20||m[61]>10||(m[62]&&(m[62]<MC_WOOD_PICK||m[62]>MC_IRON_PICK)))return 0;
      for(unsigned i=0;i<3;i++)if(get16(m+64+i*2)>250)return 0;
      if(!range(getfloat(m+72),0,599.99994f))return 0;
    }else{
      if(m[0]!=1||m[1]>1||m[2]>=MC_HOTBAR||m[3]==0||m[3]>20||m[4]>10||(m[5]&&MC_ToolIndex(m[5])<0)||!range(getfloat(m+8),0,599.99994f)||!range(getfloat(m+12),0,100)||!coord(m+16,mx,mz))return 0;
      for(unsigned i=0;i<25;i++)if(get16(m+24+i*2)>999)return 0;
      for(unsigned i=0;i<3;i++)if(get16(m+74+i*2)>MC_ToolLife(MC_WOOD_PICK+i))return 0;
      for(unsigned i=0;i<MC_HOTBAR;i++)if(m[M_BAR+i]>=MC_ITEM_COUNT)return 0;
      for(unsigned i=0;i<MC_SLOTS;i++)if(!svalid(m+M_SLOTS+i*4))return 0;
      for(unsigned c=0;c<MC_CHESTS;c++){
        const uint8_t*q=m+M_COORD+c*6;
        if(!coord(q,mx,mz)||(get16(q)&&saved_block(p,q,mx,mz)!=MC_CHEST))return 0;
        for(unsigned i=0;i<MC_CHEST_SLOTS;i++){
          const uint8_t*t=m+M_CHESTS+(c*MC_CHEST_SLOTS+i)*4;
          if(!svalid(t)||(!get16(q)&&t[1]))return 0;
        }
        if(get16(q))for(unsigned j=0;j<c;j++)if(!memcmp(q,m+M_COORD+j*6,6))return 0;
      }
      for(unsigned i=0;i<MC_FURNACES;i++){
        const uint8_t*q=m+M_FURNACES+i*26;
        if(!coord(q,mx,mz)||!svalid(q+6)||!svalid(q+10)||!svalid(q+14)||!range(getfloat(q+18),0,40)||!range(getfloat(q+22),0,4.999999f))return 0;
        if(get16(q)){
          if(saved_block(p,q,mx,mz)!=MC_FURNACE||(q[7]&&!MC_SmeltResult(q[6]))||(q[11]&&!MC_FuelSeconds(q[10]))||(q[15]&&q[14]!=MC_IRON&&q[14]!=MC_GLASS&&q[14]!=MC_COAL&&q[14]!=MC_BRICK&&q[14]!=MC_COOKED_MEAT))return 0;
          for(unsigned j=0;j<i;j++)if(!memcmp(q,m+M_FURNACES+j*26,6))return 0;
        }else if(q[7]||q[11]||q[15]||getfloat(q+18)||getfloat(q+22))return 0;
      }
      for(unsigned i=0;i<MC_GROWERS;i++){
        const uint8_t*q=m+M_GROWERS+i*10;
        if(!coord(q,mx,mz)||!range(getfloat(q+6),0,120.1f)||(get16(q)&&saved_block(p,q,mx,mz)!=MC_SAPLING&&saved_block(p,q,mx,mz)!=MC_WHEAT_CROP))return 0;
        if(get16(q))for(unsigned j=0;j<i;j++)if(!memcmp(q,m+M_GROWERS+j*10,6))return 0;
      }
      if(v>=4) {
        for(unsigned i=0;i<MC_ARMOR_SLOTS;i++) {
          const uint8_t*q=m+M_ARMOR+i*4;
          if(!svalid(q)||(q[1]&&MC_ArmorIndex(q[0])!=(int)i))return 0;
        }
        if(m[7]>1 || (m[7] && m[6]>20) || m[M_XP+5]>1)return 0;
        if(m[M_XP+5]) {
          unsigned xp=get32(m+M_XP),level=m[M_XP+4];
          if(xp>MC_XpForLevel(MC_MAX_LEVEL)||level>MC_MAX_LEVEL||level!=MC_XpLevel(&(MC_World){.experience=xp}))return 0;
        }
        for(unsigned i=0;i<MC_MOBS;i++) {
          const uint8_t*q=m+M_MOBS+i*32;float x=getfloat(q),y=getfloat(q+4),z=getfloat(q+8),vy=getfloat(q+12);
          if(q[24]>MC_MOB_HEALTH||q[25]>1||q[26]>1||!range(getfloat(q+16),0,1.5f)||!range(getfloat(q+20),0,1)||!range(getfloat(q+28),-3.15f,3.15f)||
             !range(vy,-16,16)||(!q[24] && (x||y||z||vy||getfloat(q+16)||getfloat(q+20)||q[25]))||
             (q[24] && (!range(x,.3f,mx-.3f)||!range(y,1,MC_MAP_Y+2)||!range(z,.3f,mz-.3f))))return 0;
        }
        if(!get32(m+M_MOB_STATE)||!range(getfloat(m+M_MOB_STATE+4),0,1000)||!range(getfloat(m+M_MOB_STATE+8),0,120)||!range(getfloat(m+M_MOB_STATE+12),0,1)||!range(getfloat(m+M_MOB_STATE+16),0,1))return 0;
      }
    }
  }
  return get32(p+20);
}
int MC_SaveDecode(MC_World*w,const uint8_t*p,unsigned length){
  if(!MC_SaveValidate(p,length))return 0;
  unsigned v=get32(p+4),mx=get32(p+8),mz=get32(p+16);
  unsigned bytes=mx*MC_MAP_Y*mz;
  if(mx==MC_MAP_X)memcpy(w->blocks,p+MC_SAVE_HEADER,MC_MAP_BYTES);
  else {
    /* Generate only terrain outside the preserved legacy square. No fresh
       structure/container registrations may overwrite the saved inventory. */
    MC_GenerateTerrain(w,get32(p+24));
    for(unsigned y=0;y<MC_MAP_Y;y++)for(unsigned z=0;z<mz;z++)
      memcpy(w->blocks+(y*MC_MAP_Z+z)*MC_MAP_X,p+MC_SAVE_HEADER+(y*mz+z)*mx,mx);
  }
  if(v<6)for(unsigned y=1;y<MC_MAP_Y-1;y++)for(unsigned z=0;z<MC_MAP_Z;z++)for(unsigned x=0;x<MC_MAP_X;x++) {
    uint8_t b=MC_Get(w,x,y,z);
    if((b==MC_DOOR||b==MC_DOOR_OPEN)&&MC_Get(w,x,y+1,z)==MC_AIR)MC_Set(w,x,y+1,z,b==MC_DOOR?MC_DOOR_TOP:MC_DOOR_TOP_OPEN);
  }
  w->seed=get32(p+24);w->x=getfloat(p+28);w->y=getfloat(p+32);w->z=getfloat(p+36);w->yaw=getfloat(p+40);w->pitch=getfloat(p+44);w->vy=0;w->on_ground=0;MC_NewPlayer(w,v==1);
  const uint8_t*m=p+MC_SAVE_HEADER+bytes;
  if(v==2){
    w->creative=m[59];w->slot=m[58];w->health=m[60];w->air=m[61];w->time=getfloat(m+72);memcpy(w->creative_bar,m+50,MC_HOTBAR);
    /* Preserve v2 hotbar order; excess items remain in a saved migration reserve. */
    for(unsigned i=0;i<25;i++)w->reserve[i]=get16(m+i*2);
    for(unsigned i=0;i<3;i++){unsigned wear=get16(m+64+i*2);w->reserve_wear[i]=wear&&wear<=MC_ToolLife(MC_WOOD_PICK+i)?wear:MC_ToolLife(MC_WOOD_PICK+i);}
    for(unsigned i=0;i<MC_HOTBAR;i++){
      unsigned id=m[50+i];if(!id||id==MC_BEDROCK||id==MC_WATER||!w->reserve[id])continue;
      unsigned n=MC_ToolIndex(id)>=0?1:w->reserve[id]>64?64:w->reserve[id];
      unsigned wear=MC_ToolLife(id);if(id>=MC_WOOD_PICK&&id<=MC_IRON_PICK){wear=get16(m+64+(id-MC_WOOD_PICK)*2);if(!wear || wear>MC_ToolLife(id))wear=MC_ToolLife(id);}
      w->slots[i]=(MC_Stack){id,n,wear};w->reserve[id]-=n;
    }
    for(unsigned id=1;id<25;id++)if(id!=MC_BEDROCK&&id!=MC_WATER){
      while(w->reserve[id]){unsigned n=MC_ToolIndex(id)>=0?1:w->reserve[id]>64?64:w->reserve[id];if(!MC_Give(w,id,n))break;w->reserve[id]-=n;}
    }
    for(unsigned i=0;i<MC_SLOTS;i++) {
      unsigned id=w->slots[i].item;
      if(id>=MC_WOOD_PICK && id<=MC_IRON_PICK) {
        unsigned wear=get16(m+64+(id-MC_WOOD_PICK)*2);
        if(wear && wear<=MC_ToolLife(id))w->slots[i].wear=(uint16_t)wear;
      }
    }
    w->equipped=m[62];MC_Notify(w,"V2 MIGRATED  EXTRA ITEMS IN RESERVE");
  }else if(v>=3){
      w->creative=m[1];w->slot=m[2];w->health=m[3];w->air=m[4];w->equipped=m[5];w->hunger=v>=4&&m[7]==1?m[6]:20;w->time=getfloat(m+8);w->fall_speed=getfloat(m+12);
    for(unsigned i=0;i<3;i++)w->spawn[i]=get16(m+16+i*2);
    for(unsigned i=0;i<25;i++)w->reserve[i]=get16(m+24+i*2);
    for(unsigned i=0;i<3;i++)w->reserve_wear[i]=get16(m+74+i*2);
    for(unsigned i=0;i<MC_SLOTS;i++)sread(&w->slots[i],m+M_SLOTS+i*4);
    memcpy(w->creative_bar,m+M_BAR,MC_HOTBAR);
    for(unsigned c=0;c<MC_CHESTS;c++){
      for(unsigned i=0;i<MC_CHEST_SLOTS;i++)sread(&w->chest_blocks[c][i],m+M_CHESTS+(c*MC_CHEST_SLOTS+i)*4);
      for(unsigned k=0;k<3;k++)w->chest_map[c][k]=get16(m+M_COORD+c*6+k*2);
    }
    for(unsigned i=0;i<MC_FURNACES;i++){const uint8_t*q=m+M_FURNACES+i*26;MC_Furnace*f=&w->furnaces[i];f->x=get16(q);f->y=get16(q+2);f->z=get16(q+4);sread(&f->input,q+6);sread(&f->fuel,q+10);sread(&f->output,q+14);f->burn=getfloat(q+18);f->progress=getfloat(q+22);}
    for(unsigned i=0;i<MC_GROWERS;i++){const uint8_t*q=m+M_GROWERS+i*10;w->growers[i]=(MC_Grower){get16(q),get16(q+2),get16(q+4),getfloat(q+6)};}
    if(v>=4) {
      for(unsigned i=0;i<MC_MOBS;i++) {
        const uint8_t*q=m+M_MOBS+i*32;w->mobs[i]=(MC_Mob){getfloat(q),getfloat(q+4),getfloat(q+8),getfloat(q+12),getfloat(q+16),getfloat(q+20),q[24],q[25],q[26],getfloat(q+28)};
      }
      w->mob_rng=get32(m+M_MOB_STATE);w->mob_clock=getfloat(m+M_MOB_STATE+4);w->spawn_clock=getfloat(m+M_MOB_STATE+8);
      w->attack_cooldown=getfloat(m+M_MOB_STATE+12);w->attack_flash=getfloat(m+M_MOB_STATE+16);
      for(unsigned i=0;i<MC_ARMOR_SLOTS;i++)sread(&w->armor[i],m+M_ARMOR+i*4);
      if(m[M_XP+5]) { w->experience=get32(m+M_XP);w->level=(uint8_t)MC_XpLevel(w); }
    }
    if(v>=9)memcpy(w->explored,m+MC_SAVE_META,MC_EXPLORE_BYTES);
    MC_Notify(w,"WORLD AND INVENTORY LOADED");
  }
  if(v<9)MC_UpdateExploration(w);
  MC_SyncInventory(w);
  /* Redstone propagation is runtime-only.  Lever states persist in the map;
     button pulses are intentionally cleared on load and the nearby network
     is rebuilt from its stable sources. */
  MC_RedstoneRefresh(w);
  return 1;
}
