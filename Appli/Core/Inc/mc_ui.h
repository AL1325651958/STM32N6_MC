#ifndef MC_UI_H
#define MC_UI_H
#include "minicraft.h"
enum { MC_TOUCH_NONE, MC_TOUCH_MOVE, MC_TOUCH_LOOK, MC_TOUCH_MINE, MC_TOUCH_PLACE, MC_TOUCH_JUMP, MC_TOUCH_SAVE, MC_TOUCH_SELECT, MC_TOUCH_BAG, MC_TOUCH_USE, MC_TOUCH_SNEAK };
void MC_UIBagTap(MC_World *w,unsigned x,unsigned y);
int MC_UIRole(unsigned x,unsigned y);
void MC_UIDraw(uint16_t *fb,const MC_World *world,unsigned selected,unsigned fps,unsigned max_ms,
               const char *status,int touch_ready,float move,float strafe);
#endif
