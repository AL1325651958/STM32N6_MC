#include "mc_ui.h"
#include "mc_icons.h"
#include "mc_zh.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

/* Original 5x7 bitmap alphabet, drawn at LCD resolution for legible controls. */
static const uint8_t font[][5]={
 {0x3e,0x51,0x49,0x45,0x3e},{0,0x42,0x7f,0x40,0},{0x62,0x51,0x49,0x49,0x46},{0x22,0x41,0x49,0x49,0x36},
 {0x18,0x14,0x12,0x7f,0x10},{0x27,0x45,0x45,0x45,0x39},{0x3c,0x4a,0x49,0x49,0x30},{1,0x71,9,5,3},
 {0x36,0x49,0x49,0x49,0x36},{6,0x49,0x49,0x29,0x1e},
 {0x7e,0x11,0x11,0x11,0x7e},{0x7f,0x49,0x49,0x49,0x36},{0x3e,0x41,0x41,0x41,0x22},{0x7f,0x41,0x41,0x22,0x1c},
 {0x7f,0x49,0x49,0x49,0x41},{0x7f,9,9,9,1},{0x3e,0x41,0x49,0x49,0x7a},{0x7f,8,8,8,0x7f},
 {0,0x41,0x7f,0x41,0},{0x20,0x40,0x41,0x3f,1},{0x7f,8,0x14,0x22,0x41},{0x7f,0x40,0x40,0x40,0x40},
 {0x7f,2,0x0c,2,0x7f},{0x7f,4,8,0x10,0x7f},{0x3e,0x41,0x41,0x41,0x3e},{0x7f,9,9,9,6},
 {0x3e,0x41,0x51,0x21,0x5e},{0x7f,9,0x19,0x29,0x46},{0x26,0x49,0x49,0x49,0x32},{1,1,0x7f,1,1},
 {0x3f,0x40,0x40,0x40,0x3f},{0x1f,0x20,0x40,0x20,0x1f},{0x3f,0x40,0x38,0x40,0x3f},{0x63,0x14,8,0x14,0x63},
 {7,8,0x70,8,7},{0x61,0x51,0x49,0x45,0x43},
 {0x63,0x13,0x08,0x64,0x63} /* percent */
};
static void rect(uint16_t *fb,int x,int y,int w,int h,uint16_t c)
{
  for(int yy=y;yy<y+h;yy++) for(int xx=x;xx<x+w;xx++) if((unsigned)xx<800 && (unsigned)yy<480) fb[yy*800+xx]=c;
}
static void stroke(uint16_t *fb,int x0,int y0,int x1,int y1,int width,uint16_t c)
{
  int dx=x1-x0,dy=y1-y0,steps=abs(dx)>abs(dy)?abs(dx):abs(dy);
  if(!steps){rect(fb,x0-width/2,y0-width/2,width,width,c);return;}
  for(int i=0;i<=steps;i++) {
    int x=x0+(dx*i)/steps,y=y0+(dy*i)/steps;
    rect(fb,x-width/2,y-width/2,width,width,c);
  }
}
const char *MC_Localize(const char *s)
{
  if(!s)return "";
  unsigned lo=0,hi=sizeof(mc_translations)/sizeof(mc_translations[0]);
  while(lo<hi) {
    unsigned mid=(lo+hi)/2;int order=strcmp(s,mc_translations[mid].en);
    if(!order)return mc_translations[mid].zh;
    if(order<0)hi=mid;else lo=mid+1;
  }
  return s;
}
static unsigned next_codepoint(const unsigned char **input)
{
  const unsigned char *p=*input;unsigned cp=*p++;
  if(cp>=0xe0 && cp<0xf0 && p[0] && p[1]){cp=((cp&15)<<12)|((p[0]&63)<<6)|(p[1]&63);p+=2;}
  else if(cp>=0xc0 && cp<0xe0 && p[0]){cp=((cp&31)<<6)|(p[0]&63);p++;}
  *input=p;return cp;
}
static const MC_Hanzi *hanzi(unsigned cp)
{
  unsigned lo=0,hi=sizeof(mc_hanzi)/sizeof(mc_hanzi[0]);
  while(lo<hi){unsigned mid=(lo+hi)/2;if(mc_hanzi[mid].codepoint==cp)return &mc_hanzi[mid];if(mc_hanzi[mid].codepoint>cp)hi=mid;else lo=mid+1;}
  return NULL;
}
static void draw_text(uint16_t *fb,int x,int y,const char *s,uint16_t color,int compact)
{
  const unsigned char *p=(const unsigned char *)MC_Localize(s);
  while(*p) {
    unsigned cp=next_codepoint(&p);
    if(cp>=128) {
      const MC_Hanzi *g=hanzi(cp);int edge=compact?12:16;
      if(g) {
        for(int yy=0;yy<edge;yy++)for(int xx=0;xx<edge;xx++)
          if((compact?g->small[yy]:g->large[yy])&(1u<<(edge-1-xx)))rect(fb,x+xx,y+yy,1,1,color);
      } else{rect(fb,x,y,edge,1,color);rect(fb,x,y+edge-1,edge,1,color);}
      x+=edge;
    } else {
      int scale=compact?1:2;
      int k=cp>='0'&&cp<='9'?(int)cp-'0':cp>='A'&&cp<='Z'?(int)cp-'A'+10:cp=='%'?36:-1;
      if(k>=0)for(int col=0;col<5;col++)for(int row=0;row<7;row++)
        if(font[k][col]&(1<<row))rect(fb,x+col*scale,y+row*scale,scale,scale,color);
      if(cp=='-')rect(fb,x,y+3*scale,4*scale,scale,color);
      if(cp=='/')for(int i=0;i<6;i++)rect(fb,x+(5-i)*scale,y+i*scale,scale,scale,color);
      x+=6*scale;
    }
  }
}
static void text(uint16_t *fb,int x,int y,const char *s,uint16_t color)
{draw_text(fb,x,y,s,color,0);}
static void panel(uint16_t *fb,int x,int y,int w,int h)
{
  for(int yy=y;yy<y+h;yy++) for(int xx=x;xx<x+w;xx++) fb[yy*800+xx]=(fb[yy*800+xx]>>1)&0x7bef;
  rect(fb,x,y,w,2,0x94b2);rect(fb,x,y+h-2,w,2,0x94b2);
  rect(fb,x,y,2,h,0x94b2);rect(fb,x+w-2,y,2,h,0x94b2);
}
int MC_UIRole(unsigned x,unsigned y)
{
  if(x>=700 && y<66)return MC_TOUCH_SAVE;
  if(x>=600 && x<690 && y<66)return MC_TOUCH_BAG;
  if(x>=690 && y>=216 && y<278)return MC_TOUCH_SNEAK;
  if(x>=690 && y>=360 && y<422)return MC_TOUCH_JUMP;
  if(x>=240 && x<640 && y>=424)return MC_TOUCH_SELECT;
  if(x<240 && y>=250)return MC_TOUCH_MOVE;
  return MC_TOUCH_LOOK;
}

/* Small text is used only in the inventory; controls retain the 2x font. */
static void small(uint16_t *fb,int x,int y,const char *s,uint16_t color)
{draw_text(fb,x,y,s,color,1);}
int MC_MenuTap(unsigned x,unsigned y,int creating)
{
  if(x<200 || x>=600)return MC_MENU_NONE;
  if(creating) {
    if(y>=180 && y<228)return MC_MENU_SEED;
    if(y>=240 && y<288)return MC_MENU_MODE;
    if(y>=306 && y<362)return MC_MENU_CREATE;
    if(y>=378 && y<426)return MC_MENU_BACK;
  } else {
    if(y>=180 && y<252)return MC_MENU_WORLD1;
    if(y>=276 && y<348)return MC_MENU_WORLD2;
  }
  return MC_MENU_NONE;
}
void MC_MenuDraw(uint16_t *fb,const uint8_t banks[2],int creating,unsigned bank,uint32_t seed,int creative,const char *message)
{
  for(int y=0;y<480;y++)rect(fb,0,y,800,1,y<260?0x19ea:0x2225);
  for(int x=0;x<800;x+=40)rect(fb,x,260+(x/40%3)*16,40,220,0x2b46);
  panel(fb,140,38,520,410);text(fb,350,66,"MINICRAFT N6",0xffe0);
  text(fb,creating?368:376,110,creating?"CREATE WORLD":"MAIN MENU",0xffff);
  char label[48];
  if(creating) {
    snprintf(label,sizeof(label),MC_Localize("WORLD %u"),bank+1);small(fb,370,148,label,0xffc0);
    panel(fb,200,180,400,48);snprintf(label,sizeof(label),MC_Localize("SEED %lu  CHANGE"),(unsigned long)seed);text(fb,214,196,label,0xffff);
    panel(fb,200,240,400,48);text(fb,260,256,creative?"MODE CREATIVE":"MODE SURVIVAL",0xffff);
    panel(fb,200,306,400,56);text(fb,316,328,"CREATE WORLD",0xffe0);
    panel(fb,200,378,400,48);text(fb,384,394,"BACK",0xffff);
  } else for(unsigned i=0;i<2;i++) {
    int y=180+(int)i*96;panel(fb,200,y,400,72);
    snprintf(label,sizeof(label),MC_Localize("WORLD %u  %s"),i+1,MC_Localize(banks[i]==1?"CONTINUE":banks[i]==2?"UNAVAILABLE":"CREATE"));
    text(fb,220,y+20,label,banks[i]==2?0xf9a4:0xffff);
    small(fb,220,y+48,banks[i]==1?"LOAD SAVED PROGRESS":banks[i]==2?"SAVE EXISTS  PRESERVED":"EMPTY SLOT  NEW SEED AND MODE",0xffc0);
  }
  small(fb,160,460,message?message:"SELECT A WORLD",0xffff);
}

static unsigned catalog_item(unsigned index);
static unsigned catalog_count(void);
void MC_UIBagTap(MC_World *w,unsigned x,unsigned y)
{
  if(y<66){if(x>=600&&x<690)MC_CloseBag(w);return;}
  if(w->creative && x>=24 && x<408 && y>=76 && y<108) {
    w->bag_page=(uint8_t)((w->bag_page+1)%((catalog_count()+MC_SLOTS-1)/MC_SLOTS));
    return;
  }
  if(y>=410 && y<440 && x<410) {
    if(x>=24 && x<152) {MC_SetMode(w,!w->creative);w->notice=w->creative?"CREATIVE CATALOG":"SURVIVAL INVENTORY";}
    else if(x>=160 && x<280){w->split^=1;w->moving_slot=255;}
    else if(x>=290 && x<404)w->change_world=1;
    return;
  }
  /* The four bottom-right slots are armor slots in survival inventory.  A
     direct tap equips an armor item from the bag or removes the worn piece. */
  if(!w->creative && w->open_chest>=MC_CHESTS && w->open_furnace>=MC_FURNACES &&
     x>=430 && x<782 && y>=390 && y<440) {
    unsigned a=(x-430)/88;
    if(a<MC_ARMOR_SLOTS)MC_UnequipArmor(w,a);
    return;
  }
  if(w->creative && x>=430 && x<782 && y>=390 && y<440) {w->slot=(uint8_t)((x-430)/44);return;}
  if(x>=24 && x<408 && y>=108 && y<380) {
    unsigned i=(x-24)/48+((y-108)/68)*8;
    if(w->creative)MC_SelectItem(w,catalog_item(w->bag_page*MC_SLOTS+i));
    else if(MC_ArmorIndex(w->slots[i].item)>=0)MC_EquipArmor(w,i);
    else MC_TransferSlot(w,i,w->split);
    return;
  }
  if(w->open_chest<MC_CHESTS) {
    if(x>=430&&x<778&&y>=108&&y<380)MC_ContainerTap(w,(x-430)/58+((y-108)/68)*6);
  } else if(w->open_furnace<MC_FURNACES) {
    if(x>=430&&x<772&&y>=150&&y<240)MC_ContainerTap(w,(x-430)/114);
  } else {
    if(x>=430&&x<782&&y>=78&&y<104)w->recipe_page=(uint8_t)((w->recipe_page+1)%((MC_RecipeCount()+3)/4));
    else if(x>=430&&x<782&&y>=108&&y<380)MC_Craft(w,w->recipe_page*4+(y-108)/68);
  }
}
static void icon_rect(uint16_t *fb,int x,int y,int width,int height,unsigned id)
{
  if(!id||id>=MC_ITEM_COUNT)return;
  const MC_Icon *sprite=&mc_icons[id];
  if(width<=0||height<=0)return;
  for(int py=0;py<height;py++)for(int px=0;px<width;px++) {
    unsigned sx=(unsigned)px*MC_ICON_EDGE/(unsigned)width;
    unsigned sy=(unsigned)py*MC_ICON_EDGE/(unsigned)height;
    unsigned p=sy*MC_ICON_EDGE+sx;
    unsigned index=(sprite->pixels[p>>1]>>(p&1?0:4))&15;
    if(index)rect(fb,x+px,y+py,1,1,sprite->colors[index]);
  }
}
static void icon_shadow(uint16_t *fb,int x,int y,int width,int height,unsigned id)
{
  if(!id||id>=MC_ITEM_COUNT||width<=0||height<=0)return;
  const MC_Icon *sprite=&mc_icons[id];
  for(int py=0;py<height;py++)for(int px=0;px<width;px++) {
    unsigned sx=(unsigned)px*MC_ICON_EDGE/(unsigned)width;
    unsigned sy=(unsigned)py*MC_ICON_EDGE/(unsigned)height;
    unsigned p=sy*MC_ICON_EDGE+sx;
    unsigned index=(sprite->pixels[p>>1]>>(p&1?0:4))&15;
    if(index)rect(fb,x+px+2,y+py+3,1,1,0x2104);
  }
}
static void icon(uint16_t *fb,int x,int y,unsigned id,int size)
{ icon_rect(fb,x,y,size,size,id); }
static int catalog_visible(unsigned id)
{
  return !(id==MC_DOUBLE_SLAB || id==MC_WATER || id==MC_BEDROCK || id==MC_DOOR_OPEN || id==MC_SLAB_TOP || id==MC_FARMLAND || id==MC_WHEAT_CROP || id==MC_WHEAT_MATURE || id==MC_DUST_ON || id==MC_LAMP_ON || id==MC_LEVER_ON || id==MC_BUTTON_ON || (id>=MC_STAIR_EAST && id<=MC_STAIR_WEST));
}
static unsigned catalog_count(void)
{
  unsigned count=0;
  for(unsigned id=1;id<MC_ITEM_COUNT;id++)if(catalog_visible(id))count++;
  return count;
}
static unsigned catalog_item(unsigned index)
{
  unsigned seen=0;
  for(unsigned id=1;id<MC_ITEM_COUNT;id++) {
    if(!catalog_visible(id))continue;
    if(seen++==index)return id;
  }
  return 0;
}
/* First-person item view: keep the hand below the item so every selected
   hotbar object reads as something the player is carrying, including blocks. */
static void held_item(uint16_t *fb,const MC_World *w)
{
  unsigned id=w->hotbar[w->slot];
  if(!id || id>=MC_ITEM_COUNT)return;
  int swing=w->attack_flash>0?(int)(sinf((.18f-w->attack_flash)*18.0f)*18.0f):0;
  /* Handheld geometry follows the silhouette instead of scaling every icon
     to the same square.  This keeps torches and saplings slender while
     swords and tools remain readable at the 800x480 HUD resolution. */
  int iw=70,ih=70,ix=590+swing,iy=293,gx=642+swing,gy=346;
  switch(id) {
    case MC_TORCH: iw=26;ih=72;ix=639+swing;iy=270;gx=651+swing;gy=337;break;
    case MC_SAPLING: iw=38;ih=70;ix=621+swing;iy=273;gx=645+swing;gy=338;break;
    case MC_WHEAT_SEEDS: iw=36;ih=30;ix=624+swing;iy=305;gx=646+swing;gy=334;break;
    case MC_WHEAT: iw=36;ih=78;ix=624+swing;iy=263;gx=646+swing;gy=337;break;
    case MC_STICK: iw=24;ih=76;ix=635+swing;iy=267;gx=648+swing;gy=338;break;
    case MC_APPLE: iw=46;ih=46;ix=627+swing;iy=292;gx=648+swing;gy=337;break;
    case MC_RAW_MEAT: iw=46;ih=42;ix=627+swing;iy=296;gx=648+swing;gy=337;break;
    case MC_BREAD: iw=54;ih=34;ix=622+swing;iy=302;gx=646+swing;gy=337;break;
    case MC_COOKED_MEAT: iw=46;ih=42;ix=627+swing;iy=296;gx=648+swing;gy=337;break;
    case MC_SWORD: iw=46;ih=101;ix=620+swing;iy=246;gx=648+swing;gy=343;break;
    case MC_BOW: iw=78;ih=84;ix=588+swing;iy=268;gx=639+swing;gy=342;break;
    case MC_ARROW: iw=22;ih=78;ix=640+swing;iy=268;gx=650+swing;gy=340;break;
    case MC_AXE:
    case MC_SHOVEL: iw=70;ih=82;ix=599+swing;iy=274;gx=648+swing;gy=342;break;
    case MC_WOOD_PICK:
    case MC_STONE_PICK:
    case MC_IRON_PICK: iw=78;ih=80;ix=594+swing;iy=276;gx=647+swing;gy=342;break;
    case MC_CHEST: iw=72;ih=58;ix=596+swing;iy=288;gx=650+swing;gy=342;break;
    case MC_BED: iw=78;ih=52;ix=591+swing;iy=295;gx=649+swing;gy=343;break;
    case MC_FENCE: iw=54;ih=74;ix=611+swing;iy=275;gx=648+swing;gy=342;break;
    case MC_STAIR: iw=70;ih=66;ix=603+swing;iy=283;gx=649+swing;gy=342;break;
    case MC_DOOR:
    case MC_DOOR_OPEN: iw=50;ih=82;ix=616+swing;iy=272;gx=648+swing;gy=341;break;
    case MC_DUST: iw=32;ih=18;ix=633+swing;iy=318;gx=649+swing;gy=340;break;
    case MC_LAMP: iw=54;ih=54;ix=609+swing;iy=294;gx=649+swing;gy=342;break;
    case MC_LEVER: iw=32;ih=46;ix=631+swing;iy=297;gx=649+swing;gy=340;break;
    case MC_BUTTON: iw=28;ih=28;ix=633+swing;iy=307;gx=649+swing;gy=339;break;
    default: break;
  }
  /* The forearm points at the actual grip instead of forming a detached
     vertical block.  Keep it behind the icon so slender items remain clear. */
  stroke(fb,716+swing,466,gx,gy+12,28,0x4208);
  stroke(fb,713+swing,462,gx,gy+10,21,0xd4a6);
  rect(fb,gx-13,gy-9,27,22,0xd4a6);
  rect(fb,gx-9,gy-12,18,8,0xe6bd);
  icon_shadow(fb,ix,iy,iw,ih,id);
  icon_rect(fb,ix,iy,iw,ih,id);
}
/* A compact top-down locator keeps the nearby terrain and the player's base
   readable without adding another touch control.  The map is deliberately
   derived from the current world so it costs no save bytes and never goes
   stale after building or mining. */
static uint16_t map_color(uint8_t b)
{
  switch(b) {
    case MC_WATER:return 0x3d9f;
    case MC_GRASS:return 0x56a5;
    case MC_DIRT:return 0x8b07;
    case MC_SAND:return 0xd66f;
    case MC_STONE:
    case MC_COBBLE:
    case MC_BRICK:
    case MC_SLAB:
    case MC_SLAB_TOP:
    case MC_DOUBLE_SLAB:
    case MC_STAIR:
    case MC_STAIR_EAST:
    case MC_STAIR_SOUTH:
    case MC_STAIR_WEST:return 0x8410;
    case MC_WOOD:
    case MC_PLANKS:
    case MC_TABLE:
    case MC_FENCE:return 0xa365;
    case MC_BIRCH_WOOD:
    case MC_BIRCH_PLANKS:return 0xd6b6;
    case MC_DARK_WOOD:
    case MC_DARK_PLANKS:return 0x5a04;
    case MC_LEAVES:return 0x3b85;
    case MC_GLASS:return 0x9f7f;
    case MC_TORCH:return 0xfaa0;
    case MC_FARMLAND:return 0x6a64;
    case MC_WHEAT_CROP:return 0x5d87;
    case MC_WHEAT_MATURE:return 0xf6a0;
    case MC_DUST:
    case MC_DUST_ON:return 0xf800;
    case MC_LAMP:return 0x8410;
    case MC_LAMP_ON:return 0xffe0;
    case MC_LEVER:
    case MC_LEVER_ON:return 0xffc0;
    case MC_BUTTON:
    case MC_BUTTON_ON:return 0xb48a;
    default:return 0x2104;
  }
}
static uint8_t map_surface(const MC_World *w,int x,int z)
{
  if(x<0||z<0||x>=MC_MAP_X||z>=MC_MAP_Z)return MC_AIR;
  /* Coordinates are already checked: walk the PSRAM column directly,
     avoiding tens of thousands of cross-file MC_Get calls per HUD frame. */
  unsigned column=(unsigned)z*MC_MAP_X+(unsigned)x;
  for(int y=MC_MAP_Y-1;y>0;y--) {
    uint8_t b=w->blocks[(unsigned)y*MC_MAP_X*MC_MAP_Z+column];
    if(b!=MC_AIR)return b;
  }
  return MC_AIR;
}
static void mini_map(uint16_t *fb,const MC_World *w)
{
  const int ox=686,oy=74,ow=104,oh=126,cx=738,cy=139,cell=3,radius=14;
  panel(fb,ox,oy,ow,oh);text(fb,722,80,"MAP",0xffe0);
  rect(fb,ox+7,oy+23,87,87,0x1082);
  int px=(int)floorf(w->x),pz=(int)floorf(w->z);
  for(int dz=-radius;dz<=radius;dz++)for(int dx=-radius;dx<=radius;dx++) {
    int wx=px+dx,wz=pz+dz;uint16_t c=map_color(map_surface(w,wx,wz));
    rect(fb,cx+dx*cell-1,cy+dz*cell-1,cell,cell,c);
  }
  /* Bed spawn and chest locations are persistent landmarks. */
  if(w->spawn[0] && abs((int)w->spawn[0]-1-px)<=radius && abs((int)w->spawn[2]-1-pz)<=radius) {
    int sx=cx+((int)w->spawn[0]-1-px)*cell,sz=cy+((int)w->spawn[2]-1-pz)*cell;
    rect(fb,sx-3,sz-3,7,2,0xffe0);rect(fb,sx-3,sz+1,7,2,0xffe0);
    rect(fb,sx-3,sz-1,2,3,0xffe0);rect(fb,sx+2,sz-1,2,3,0xffe0);
  }
  for(unsigned i=0;i<MC_CHESTS;i++)if(w->chest_map[i][0]) {
    int dx=(int)w->chest_map[i][0]-1-px,dz=(int)w->chest_map[i][2]-1-pz;
    if(abs(dx)<=radius&&abs(dz)<=radius)rect(fb,cx+dx*cell-2,cy+dz*cell-2,5,5,0xffc0);
  }
  /* A two-pixel arrow shows facing direction while preserving the center dot. */
  rect(fb,cx-2,cy-2,5,5,0xffff);
  int ax=(int)roundf(sinf(w->yaw)*4),az=(int)roundf(cosf(w->yaw)*4);
  rect(fb,cx+ax-1,cy+az-1,3,3,0xf800);
  rect(fb,694,189,5,5,0xffe0);small(fb,702,185,"BED",0xffe0);small(fb,738,185,"YOU",0xffff);
}
static void crop_hint(uint16_t *fb,const MC_World *w)
{
  if(!w->target_valid)return;
  const MC_Hit *h=&w->target;
  if(h->block!=MC_WHEAT_CROP && h->block!=MC_WHEAT_MATURE)return;
  const char *hint="READY  HOLD TO HARVEST";
  uint16_t color=0x07e0;char growing[32];
  if(h->block==MC_WHEAT_CROP) {
    unsigned needs=MC_CropGrowthNeeds(w,h->x,h->y,h->z);
    if(needs&MC_CROP_NEEDS_SOIL)hint="NEEDS FARMLAND BELOW";
    else if(needs&MC_CROP_NEEDS_SPACE)hint="NEEDS SPACE ABOVE";
    else if(needs&MC_CROP_NEEDS_WATER)hint="NEEDS WATER WITHIN 4 BLOCKS";
    else if(needs&MC_CROP_NEEDS_LIGHT)hint="NEEDS SUNLIGHT OR TORCH";
    else {
      unsigned percent=0;
      for(unsigned i=0;i<MC_GROWERS;i++) {
        const MC_Grower *g=&w->growers[i];
        if(g->x==h->x+1 && g->y==h->y+1 && g->z==h->z+1){percent=(unsigned)(fminf(75,g->age)*100/75);break;}
      }
      snprintf(growing,sizeof(growing),MC_Localize("WHEAT GROWING  %u%%"),percent);hint=growing;
    }
    if(needs)color=0xffc0;
  }
  panel(fb,280,300,240,26);small(fb,290,309,hint,color);
}
static const char *scene_action_hint(unsigned item)
{
  if(item==MC_BOW)return "TAP SHOOT";
  if(item==MC_SHOVEL)return "TAP TILL";
  if(item==MC_WHEAT)return "TAP FEED";
  if(MC_FoodHunger(item))return "TAP EAT";
  if(item==MC_SWORD||item==MC_AXE||item==MC_WOOD_PICK||item==MC_STONE_PICK||item==MC_IRON_PICK)return "TAP ATTACK";
  return MC_IsBlock(item)?"TAP PLACE":"TAP USE";
}
static void stack(uint16_t *fb,int x,int y,int width,int height,const MC_Stack *s,int chosen,int infinite)
{
  char n[16];uint16_t rim=chosen?0xffcb:0x52ab;
  rect(fb,x,y,width-3,height-3,rim);rect(fb,x+2,y+2,width-7,height-7,chosen?0x4247:0x18e3);
  unsigned size=width<42?28:32;
  icon(fb,x+(width-(int)size)/2,y+3,s->item,(int)size);
  if(s->item) {
    snprintf(n,sizeof(n),"%u",s->count);const char *label=infinite?"INF":n;
    int length=infinite?4:s->count>=10?2:1;
    if(infinite||s->count>1){int cx=x+width-7-length*6,cy=y+height-19;small(fb,cx+1,cy+1,label,0);small(fb,cx,cy,label,0xffff);}
    unsigned life=MC_ToolLife(s->item);if(!life)life=MC_ArmorLife(s->item);
    if(s->wear && life) {
      unsigned wear=s->wear>life?life:s->wear;
      rect(fb,x+4,y+height-10,width-11,3,0x4208);
      rect(fb,x+4,y+height-10,(int)((width-11)*wear/life),3,wear*4<life?0xf9a4:wear*2<life?0xffc0:0x07e0);
    }
  }
}
void MC_UIDraw(uint16_t *fb,const MC_World *w,unsigned selected,unsigned fps,unsigned max_ms,const char *status,int touch_ready,float move,float strafe)
{
  (void)selected;(void)status;(void)touch_ready;
  char info[96];
  panel(fb,10,8,578,30);
  snprintf(info,sizeof(info),MC_Localize("X %d Y %d Z %d FPS %u MAX %uMS"),(int)w->x,(int)w->y,(int)w->z,fps,max_ms);
  text(fb,20,16,info,0xffff);
  panel(fb,600,8,90,54);text(fb,629,28,w->bag?"CLOSE":"BAG",0xffff);
  {
    static const char *labels[]={"SAVE","SAVING","SAVED","NO SD","READ ONLY","WRITE FAILED","VERIFY FAILED"};
    unsigned result=w->save_feedback<=MC_SAVE_VERIFY_FAILED?w->save_feedback:MC_SAVE_IDLE;
    uint16_t color=result==MC_SAVE_OK?0x07e0:result>=MC_SAVE_NO_SD?0xf9a4:result==MC_SAVE_BUSY?0xffe0:0xffff;
    const unsigned char *p=(const unsigned char *)MC_Localize(labels[result]);int width=0;
    while(*p)width+=next_codepoint(&p)>=128?16:12;
    panel(fb,700,8,90,54);text(fb,745-width/2,28,labels[result],color);
    if(result!=MC_SAVE_IDLE)rect(fb,702,58,86,2,color);
  }
  if(w->bag) {
    panel(fb,12,76,776,370);
    if(w->creative) {
      snprintf(info,sizeof(info),MC_Localize("CATALOG %u-%u  NEXT"),w->bag_page+1,(catalog_count()+MC_SLOTS-1)/MC_SLOTS);
      text(fb,24,84,info,0xffe0);
    } else text(fb,24,84,"BAG  32 SLOTS",0xffe0);
    if(w->open_chest<MC_CHESTS)text(fb,430,84,"CHEST  24 SLOTS",0xffe0);
    else if(w->open_furnace<MC_FURNACES)text(fb,430,84,"FURNACE",0xffe0);
    else {snprintf(info,sizeof(info),MC_Localize("CRAFT %u-%u  NEXT"),w->recipe_page+1,(MC_RecipeCount()+3)/4);text(fb,430,84,info,0xffe0);}
    for(unsigned i=0;i<MC_SLOTS;i++) {
      MC_Stack s=w->creative?(MC_Stack){catalog_item(w->bag_page*MC_SLOTS+i),1,0}:w->slots[i];
      if(s.item==MC_WATER||s.item==MC_BEDROCK)s=(MC_Stack){0};
      stack(fb,24+(i%8)*48,108+(i/8)*68,48,68,&s,!w->creative&&(i==w->selected_slot||i==w->moving_slot),w->creative);
      if(!w->creative && i<8) {snprintf(info,sizeof(info),"%u",i+1);small(fb,26+i*48,110,info,0xffe0);}
    }
    if(w->open_chest<MC_CHESTS) {
      for(unsigned i=0;i<MC_CHEST_SLOTS;i++)stack(fb,430+(i%6)*58,108+(i/6)*68,58,68,&w->chest_blocks[w->open_chest][i],0,0);
      small(fb,430,386,"SELECT BAG SLOT THEN TAP CHEST",0xffff);
      small(fb,430,400,"EMPTY BAG SLOT WITHDRAWS ITEMS",0xffff);
    } else if(w->open_furnace<MC_FURNACES) {
      const MC_Furnace *f=&w->furnaces[w->open_furnace];
      text(fb,438,120,"INPUT",0xffff);text(fb,552,120,"FUEL",0xffff);text(fb,666,120,"OUT",0xffff);
      stack(fb,430,150,110,90,&f->input,0,0);stack(fb,544,150,110,90,&f->fuel,0,0);stack(fb,658,150,110,90,&f->output,0,0);
      rect(fb,438,260,320,12,0x2104);rect(fb,438,260,(int)(320*f->progress/5),12,0xffe0);
      snprintf(info,sizeof(info),MC_Localize("FIRE %u SEC  PROCESS 5 SEC"),(unsigned)ceilf(f->burn));small(fb,438,286,info,0xffe0);
      small(fb,438,310,"INPUT  ORE SAND LOG OR COBBLE",0xffff);
      small(fb,438,328,"FUEL   COAL LOG OR PLANKS",0xffff);
      small(fb,438,350,"SELECT BAG ITEM THEN TAP INPUT OR FUEL",0xffff);
      small(fb,438,368,"TAP OUTPUT TO COLLECT",0xffff);
    } else for(unsigned i=0;i<4;i++) {
      unsigned r=w->recipe_page*4+i;int y=108+(int)i*68;if(r>=MC_RecipeCount())break;
      rect(fb,430,y,348,64,0x2945);icon(fb,440,y+13,MC_RecipeResult(r),32);
      text(fb,480,y+7,MC_RecipeName(r),0xffff);
      small(fb,480,y+30,MC_RecipeCost(r),0xffc0);small(fb,480,y+47,MC_RecipeStatus(w,r),0x9cf3);
      for(unsigned part=0;part<2;part++) {
        unsigned ingredient=MC_RecipeIngredient(r,part),count=MC_RecipeIngredientCount(r,part);
        if(ingredient){int ix=700+(int)part*36;icon(fb,ix,y+12,ingredient,24);snprintf(info,sizeof(info),"%u",count);small(fb,ix+22,y+34,info,0xffff);}
      }
    }
    unsigned id=w->creative?w->hotbar[w->slot]:w->slots[w->selected_slot].item;
    text(fb,24,386,MC_ItemName(id),0xffff);
    panel(fb,24,410,128,30);text(fb,72,418,w->creative?"CREATIVE":"SURVIVAL",0xffe0);
    panel(fb,160,410,120,30);text(fb,196,418,w->split?"HALF ON":"HALF OFF",0xffff);
    panel(fb,290,410,114,30);text(fb,331,418,"MENU",0xffff);
    if(w->creative)for(unsigned i=0;i<8;i++){MC_Stack s={w->hotbar[i],1,0};stack(fb,430+i*44,390,44,50,&s,i==w->slot,1);}
    else if(w->open_chest>=MC_CHESTS&&w->open_furnace>=MC_FURNACES) {

      for(unsigned i=0;i<MC_ARMOR_SLOTS;i++)stack(fb,430+(int)i*88,394,82,46,&w->armor[i],0,0);
      char armor[24];snprintf(armor,sizeof(armor),MC_Localize("ARMOR %u PTS"),MC_ArmorPoints(w));small(fb,430,380,armor,0xffc0);
    }
  } else {
    mini_map(fb,w);
    held_item(fb,w);
    rect(fb,391,239,19,2,0xffff);rect(fb,400,231,2,19,0xffff);
    for(int yy=-78;yy<=78;yy++)for(int xx=-78;xx<=78;xx++){int d=xx*xx+yy*yy;if(d<78*78&&d>74*74)fb[(350+yy)*800+110+xx]=0xc638;}
    int jx=110+(int)(fmaxf(-1,fminf(1,strafe))*45),jy=350-(int)(fmaxf(-1,fminf(1,move))*45);
    rect(fb,jx-10,jy-10,20,20,0xffe0);text(fb,94,444,"MOVE",0xffff);
    /* Scene taps are the contextual action.  The held item is visible at
       lower right, so no second PLACE/USE button is needed. */
    unsigned held=w->hotbar[w->slot];
    uint16_t action_color=held==MC_BOW?0xfca0:held==MC_WHEAT?0x07e0:
      (held==MC_SWORD||held==MC_AXE||held==MC_WOOD_PICK||held==MC_STONE_PICK||held==MC_IRON_PICK)?0xf9a4:0xffe0;
    small(fb,690,288,scene_action_hint(held),action_color);
    panel(fb,690,216,100,62);text(fb,724,228,"SNEAK",w->sneak?0x07e0:0xffff);small(fb,700,258,w->sneak?"EDGE GUARD":"HOLD TO SNEAK",0xffe0);
    panel(fb,690,360,100,62);text(fb,724,384,"JUMP",0xffff);
    for(unsigned i=0;i<MC_HOTBAR;i++){MC_Stack s=w->creative?(MC_Stack){w->hotbar[i],1,0}:w->slots[i];stack(fb,240+i*50,424,50,56,&s,i==w->slot,w->creative);}
    panel(fb,640,393,44,56);icon(fb,642,397,w->hotbar[w->slot],40);
    panel(fb,240,393,400,25);text(fb,249,399,MC_ItemName(w->hotbar[w->slot]),0xffff);
    /* Compact XP bar fits between the survival meters and the item label. */
    {
      unsigned next=MC_XpToNext(w),progress=MC_XpProgress(w);
      char xp[24];snprintf(xp,sizeof(xp),MC_Localize("LVL %u"),(unsigned)MC_XpLevel(w));
      small(fb,240,378,xp,w->level_flash>0?0xffe0:0xffc0);
      rect(fb,282,378,350,6,0x2104);
      if(next)rect(fb,283,379,(int)(348u*(progress>next?next:progress)/next),4,w->level_flash>0?0xffe0:0x07e0);
      if(w->level_flash>0)small(fb,500,378,"LEVEL UP",0xffe0);
    }
    if(w->mining>0){rect(fb,300,274,200,8,0x2104);rect(fb,302,276,(int)(196*fminf(1,w->mining)),4,0xffe0);}
    crop_hint(fb,w);
    for(unsigned i=0;i<10;i++){int x=246+(int)i*22;rect(fb,x,369,16,12,i*2<w->health?0xe924:0x4208);if(i*2+1>=w->health)rect(fb,x+8,369,8,12,0x4208);}
    small(fb,492,347,"FOOD",0xffc0);
    for(unsigned i=0;i<10;i++) {
      int x=522+(int)i*17;rect(fb,x,369,13,12,i*2<w->hunger?0xfaa0:0x4208);
      if(i*2+1>=w->hunger)rect(fb,x+7,369,6,12,0x4208);
    }
    if(w->air<10){snprintf(info,sizeof(info),MC_Localize("AIR %u"),w->air);text(fb,490,325,info,0x7fff);}
    if(!w->creative&&w->slots[w->slot].wear){snprintf(info,sizeof(info),MC_Localize("TOOL %u"),w->slots[w->slot].wear);text(fb,550,347,info,0xffff);}
    if(w->hurt_timer>0){rect(fb,0,0,800,4,0xf800);rect(fb,0,476,800,4,0xf800);}
    if(w->mob_target<MC_MOBS && w->mobs[w->mob_target].health){
      rect(fb,330,252,140,14,0x2104);rect(fb,332,254,136*w->mobs[w->mob_target].health/MC_MOB_HEALTH,10,0xe924);
    }
    if(w->attack_flash>0){rect(fb,393,236,15,4,0xffe0);rect(fb,398,231,4,15,0xffe0);}
  }
  if(w->notice){int y=w->bag?450:74;panel(fb,10,y,w->bag?780:570,24);small(fb,20,y+8,w->notice,0xffff);}
}
