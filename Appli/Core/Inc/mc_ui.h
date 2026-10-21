#ifndef MC_UI_H
#define MC_UI_H
#include "minicraft.h"
enum { MC_TOUCH_NONE, MC_TOUCH_MOVE, MC_TOUCH_LOOK, MC_TOUCH_MINE, MC_TOUCH_PLACE, MC_TOUCH_JUMP, MC_TOUCH_SAVE, MC_TOUCH_SELECT, MC_TOUCH_BAG, MC_TOUCH_USE, MC_TOUCH_SNEAK };
enum { MC_MENU_NONE, MC_MENU_WORLD1, MC_MENU_WORLD2, MC_MENU_SEED, MC_MENU_MODE, MC_MENU_CREATE, MC_MENU_BACK,
       MC_MENU_DELETE1, MC_MENU_DELETE2, MC_MENU_DELETE_CONFIRM };
int MC_MenuTap(unsigned x,unsigned y,int creating);
void MC_MenuDraw(uint16_t *fb,const uint8_t banks[2],int creating,unsigned bank,uint32_t seed,int creative,const char *message);
const char *MC_Localize(const char *english);
void MC_UIText(uint16_t *fb,int x,int y,const char *s,uint16_t color,int compact);
void MC_UIBagTap(MC_World *w,unsigned x,unsigned y);
int MC_UIRole(unsigned x,unsigned y);
void MC_UIDraw(uint16_t *fb,const MC_World *world,unsigned selected,unsigned fps,unsigned max_ms,
               const char *status,int touch_ready,float move,float strafe);
#endif
