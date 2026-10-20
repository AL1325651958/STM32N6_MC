#include "mc_ui.h"
#include "mc_icons.h"
#include <stdio.h>
#include <math.h>

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
 {7,8,0x70,8,7},{0x61,0x51,0x49,0x45,0x43}
};
static void rect(uint16_t *fb,int x,int y,int w,int h,uint16_t c)
{
  for(int yy=y;yy<y+h;yy++) for(int xx=x;xx<x+w;xx++) if((unsigned)xx<800 && (unsigned)yy<480) fb[yy*800+xx]=c;
}
static void text(uint16_t *fb,int x,int y,const char *s,uint16_t c)
{
  for(;*s;s++,x+=12) {
    int k=*s>='0' && *s<='9'?*s-'0':*s>='A' && *s<='Z'?*s-'A'+10:-1;
    if(k>=0) { for(int col=0;col<5;col++) for(int row=0;row<7;row++) if(font[k][col]&(1<<row)) rect(fb,x+col*2,y+row*2,2,2,c); }
    else if(*s=='-') rect(fb,x,y+6,8,2,c);
  }
}
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
  if(x>=690 && y>=288 && y<350)return MC_TOUCH_PLACE;
  if(x>=690 && y>=216 && y<278)return MC_TOUCH_SNEAK;
  if(x>=690 && y>=360 && y<422)return MC_TOUCH_JUMP;
  if(x>=240 && x<640 && y>=424)return MC_TOUCH_SELECT;
  if(x<240 && y>=250)return MC_TOUCH_MOVE;
  return MC_TOUCH_LOOK;
}

/* Small text is used only in the inventory; controls retain the 2x font. */
static void small(uint16_t *fb,int x,int y,const char *s,uint16_t c)
{
  for(;*s;s++,x+=6) {
    int k=*s>='0'&&*s<='9'?*s-'0':*s>='A'&&*s<='Z'?*s-'A'+10:-1;
    if(k>=0)for(int col=0;col<5;col++)for(int row=0;row<7;row++)
      if(font[k][col]&(1<<row))rect(fb,x+col,y+row,1,1,c);
    if(*s=='-')rect(fb,x,y+3,4,1,c);
  }
}
static unsigned catalog_item(unsigned index);
void MC_UIBagTap(MC_World *w,unsigned x,unsigned y)
{
  if(y<66){if(x>=600&&x<690)MC_CloseBag(w);return;}
  if(y>=410 && y<440 && x<410) {
    if(x>=24 && x<152) {MC_SetMode(w,!w->creative);w->notice=w->creative?"CREATIVE CATALOG":"SURVIVAL INVENTORY";}
    else if(x>=160 && x<280){w->split^=1;w->moving_slot=255;}
    else if(x>=290 && x<404)w->change_world=1;
    return;
  }
  if(w->creative && x>=430 && x<782 && y>=390 && y<440) {w->slot=(uint8_t)((x-430)/44);return;}
  if(x>=24 && x<408 && y>=108 && y<380) {
    unsigned i=(x-24)/48+((y-108)/68)*8;
    if(w->creative)MC_SelectItem(w,catalog_item(i));
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
static void icon(uint16_t *fb,int x,int y,unsigned id,int size)
{ icon_rect(fb,x,y,size,size,id); }
static unsigned catalog_item(unsigned index)
{
  unsigned seen=0;
  for(unsigned id=1;id<MC_ITEM_COUNT;id++) {
    if(id==MC_WATER || id==MC_BEDROCK || id==MC_DOOR_OPEN || id==MC_SLAB_TOP || id==MC_FARMLAND || id==MC_WHEAT_CROP || id==MC_WHEAT_MATURE)continue;
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
  int swing=w->attack_flash>0?(int)(sinf(w->attack_flash*34.0f)*18.0f):0;
  /* Handheld geometry follows the silhouette instead of scaling every icon
     to the same square.  This keeps torches and saplings slender while
     swords and tools remain readable at the 800x480 HUD resolution. */
  int iw=70,ih=70,ix=590+swing,iy=293;
  switch(id) {
    case MC_TORCH: iw=34;ih=66;ix=630+swing;iy=274;break;
    case MC_SAPLING: iw=46;ih=66;ix=616+swing;iy=276;break;
    case MC_WHEAT_SEEDS: iw=42;ih=34;ix=622+swing;iy=304;break;
    case MC_WHEAT: iw=42;ih=76;ix=622+swing;iy=266;break;
    case MC_STICK: iw=30;ih=72;ix=632+swing;iy=270;break;
    case MC_APPLE: iw=48;ih=48;ix=626+swing;iy=294;break;
    case MC_SWORD: iw=50;ih=94;ix=618+swing;iy=255;break;
    case MC_AXE:
    case MC_SHOVEL: iw=76;ih=80;ix=596+swing;iy=276;break;
    case MC_WOOD_PICK:
    case MC_STONE_PICK:
    case MC_IRON_PICK: iw=82;ih=78;ix=592+swing;iy=278;break;
    case MC_CHEST: iw=78;ih=62;ix=592+swing;iy=286;break;
    case MC_BED: iw=82;ih=54;ix=588+swing;iy=294;break;
    case MC_DOOR:
    case MC_DOOR_OPEN: iw=54;ih=82;ix=614+swing;iy=274;break;
    default: break;
  }
  /* Dark outline and a warm skin-tone cuff give the icon a readable grip. */
  rect(fb,642+swing,360,42,58,0x4208);
  rect(fb,646+swing,357,34,61,0xd4a6);
  rect(fb,650+swing,352,27,20,0xe6bd);
  rect(fb,ix-3,iy-3,iw+6,ih+6,0x2104);
  icon_rect(fb,ix,iy,iw,ih,id);
}
static void stack(uint16_t *fb,int x,int y,int width,int height,const MC_Stack *s,int chosen,int infinite)
{
  char n[16];uint16_t rim=chosen?0xffcb:0x52ab;
  rect(fb,x,y,width-3,height-3,rim);rect(fb,x+2,y+2,width-7,height-7,chosen?0x4247:0x18e3);
  unsigned size=width<42?28:32;
  icon(fb,x+(width-(int)size)/2,y+3,s->item,(int)size);
  if(s->item) {
    snprintf(n,sizeof(n),"%u",s->count);const char *label=infinite?"INF":n;
    int length=infinite?3:s->count>=10?2:1;
    if(infinite||s->count>1){int cx=x+width-7-length*6,cy=y+height-19;small(fb,cx+1,cy+1,label,0);small(fb,cx,cy,label,0xffff);}
    if(s->wear && MC_ToolLife(s->item)) {
      unsigned life=MC_ToolLife(s->item),wear=s->wear>life?life:s->wear;
      rect(fb,x+4,y+height-10,width-11,3,0x4208);
      rect(fb,x+4,y+height-10,(int)((width-11)*wear/life),3,wear*4<life?0xf9a4:wear*2<life?0xffc0:0x07e0);
    }
  }
}
void MC_UIDraw(uint16_t *fb,const MC_World *w,unsigned selected,unsigned fps,unsigned max_ms,const char *status,int touch_ready,float move,float strafe)
{
  (void)selected;char info[96];
  panel(fb,10,8,578,62);
  snprintf(info,sizeof(info),"X %d Y %d Z %d FPS %u MAX %uMS",(int)w->x,(int)w->y,(int)w->z,fps,max_ms);
  text(fb,20,18,info,0xffff);text(fb,20,43,touch_ready?status:"TOUCH NOT FOUND",touch_ready?0xffc0:0xf800);
  panel(fb,600,8,90,54);text(fb,614,28,w->bag?"CLOSE":"BAG",0xffff);
  panel(fb,700,8,90,54);text(fb,718,28,"SAVE",0xffff);
  if(w->bag) {
    panel(fb,12,76,776,370);
    text(fb,24,84,w->creative?"CREATIVE CATALOG":"BAG  32 SLOTS",0xffe0);
    if(w->open_chest<MC_CHESTS)text(fb,430,84,"CHEST  24 SLOTS",0xffe0);
    else if(w->open_furnace<MC_FURNACES)text(fb,430,84,"FURNACE",0xffe0);
    else {snprintf(info,sizeof(info),"CRAFT %u-%u  NEXT",w->recipe_page+1,(MC_RecipeCount()+3)/4);text(fb,430,84,info,0xffe0);}
    for(unsigned i=0;i<MC_SLOTS;i++) {
      MC_Stack s=w->creative?(MC_Stack){catalog_item(i),1,0}:w->slots[i];
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
      snprintf(info,sizeof(info),"FIRE %u SEC  PROCESS 5 SEC",(unsigned)ceilf(f->burn));small(fb,438,286,info,0xffe0);
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
    panel(fb,24,410,128,30);text(fb,29,418,w->creative?"CREATIVE":"SURVIVAL",0xffe0);
    panel(fb,160,410,120,30);text(fb,166,418,w->split?"HALF ON":"HALF OFF",0xffff);
    panel(fb,290,410,114,30);text(fb,310,418,"WORLD",0xffff);
    if(w->creative)for(unsigned i=0;i<8;i++){MC_Stack s={w->hotbar[i],1,0};stack(fb,430+i*44,390,44,50,&s,i==w->slot,1);}
    else if(w->open_chest>=MC_CHESTS&&w->open_furnace>=MC_FURNACES)small(fb,430,410,"TAP SOURCE THEN DESTINATION TO MOVE",0xffff);
  } else {
    held_item(fb,w);
    rect(fb,391,239,19,2,0xffff);rect(fb,400,231,2,19,0xffff);
    for(int yy=-78;yy<=78;yy++)for(int xx=-78;xx<=78;xx++){int d=xx*xx+yy*yy;if(d<78*78&&d>74*74)fb[(350+yy)*800+110+xx]=0xc638;}
    int jx=110+(int)(fmaxf(-1,fminf(1,strafe))*45),jy=350-(int)(fmaxf(-1,fminf(1,move))*45);
    rect(fb,jx-10,jy-10,20,20,0xffe0);text(fb,86,444,"MOVE",0xffff);
    panel(fb,690,288,100,62);text(fb,710,312,w->hotbar[w->slot]==MC_APPLE?"EAT":"USE",0xffff);
    panel(fb,690,216,100,62);text(fb,710,228,"SNEAK",w->sneak?0x07e0:0xffff);small(fb,700,258,w->sneak?"EDGE GUARD":"HOLD TO BUILD",0xffe0);
    panel(fb,690,360,100,62);text(fb,716,384,"JUMP",0xffff);
    for(unsigned i=0;i<MC_HOTBAR;i++){MC_Stack s=w->creative?(MC_Stack){w->hotbar[i],1,0}:w->slots[i];stack(fb,240+i*50,424,50,56,&s,i==w->slot,w->creative);}
    panel(fb,640,393,44,56);icon(fb,642,397,w->hotbar[w->slot],40);
    panel(fb,240,393,400,25);text(fb,249,399,MC_ItemName(w->hotbar[w->slot]),0xffff);
    if(w->mining>0){rect(fb,300,274,200,8,0x2104);rect(fb,302,276,(int)(196*fminf(1,w->mining)),4,0xffe0);}
    for(unsigned i=0;i<10;i++){int x=246+(int)i*22;rect(fb,x,369,16,12,i*2<w->health?0xe924:0x4208);if(i*2+1>=w->health)rect(fb,x+8,369,8,12,0x4208);}
    if(w->air<10){snprintf(info,sizeof(info),"AIR %u",w->air);text(fb,490,369,info,0x7fff);}
    if(!w->creative&&w->slots[w->slot].wear){snprintf(info,sizeof(info),"TOOL %u",w->slots[w->slot].wear);text(fb,490,347,info,0xffff);}
    if(w->hurt_timer>0){rect(fb,0,0,800,4,0xf800);rect(fb,0,476,800,4,0xf800);}
    if(w->mob_target<MC_MOBS && w->mobs[w->mob_target].health){
      rect(fb,330,252,140,14,0x2104);rect(fb,332,254,136*w->mobs[w->mob_target].health/MC_MOB_HEALTH,10,0xe924);
    }
    if(w->attack_flash>0){rect(fb,393,236,15,4,0xffe0);rect(fb,398,231,4,15,0xffe0);}
  }
  if(w->notice){int y=w->bag?450:74;panel(fb,10,y,780,24);small(fb,20,y+8,w->notice,0xffff);}
}
