#include "mc_view.h"
#include <string.h>

void MC_ViewInvalidate(MC_View *view){view->valid=0;}

int MC_ViewUpdate(MC_View *view,const MC_World *w)
{
  if(!w->bag) {
    view->valid=0;
    MC_Render(w,view->pixels,MC_VIEW_W,MC_VIEW_H);return 1;
  }
  MC_ViewKey key;
  /* Explicit zeroing and member writes make byte comparison independent
     of C padding, and avoid CRC collisions in an image-validity decision. */
  memset(&key,0,sizeof(key));
  key.world=w;key.blocks=w->blocks;key.serial=w->scene_serial;key.revision=w->map_revision;key.seed=w->seed;
  key.x=w->x;key.y=w->y;key.z=w->z;key.yaw=w->yaw;key.pitch=w->pitch;key.time=w->time;
  if(w->target_valid) {
    key.target_valid=1;key.target_x=w->target.x;key.target_y=w->target.y;key.target_z=w->target.z;
    key.target_block=w->target.block;key.mining=w->mining;
  }
  for(unsigned i=0;i<MC_MOBS;i++)if(w->mobs[i].health) {
    const MC_Mob *m=&w->mobs[i];MC_ViewMob *v=&key.mobs[i];
    v->active=1;v->x=m->x;v->y=m->y;v->z=m->z;v->type=m->type;v->hurt=m->hurt>0;
  }
  for(unsigned i=0;i<MC_DROPS;i++)if(w->drops[i].count) {
    const MC_Drop *d=&w->drops[i];MC_ViewDrop *v=&key.drops[i];
    v->active=1;v->x=d->x;v->y=d->y;v->z=d->z;v->item=d->item;
  }
  for(unsigned i=0;i<MC_PROJECTILES;i++)if(w->projectiles[i].active) {
    const MC_Projectile *p=&w->projectiles[i];MC_ViewProjectile *v=&key.projectiles[i];
    v->active=1;v->x=p->x;v->y=p->y;v->z=p->z;v->vx=p->vx;v->vy=p->vy;v->vz=p->vz;
  }
  for(unsigned i=0;i<MC_FURNACES;i++) {
    const MC_Furnace *f=&w->furnaces[i];
    if(f->x && f->burn>0 && MC_Get(w,f->x-1,f->y-1,f->z-1)==MC_FURNACE)
      key.burning_furnaces[i]=((uint32_t)(f->y-1)*MC_MAP_Z+f->z-1)*MC_MAP_X+f->x;
  }
  if(view->valid && !memcmp(&view->key,&key,sizeof(key)))return 0;
  MC_Render(w,view->pixels,MC_VIEW_W,MC_VIEW_H);
  memcpy(&view->key,&key,sizeof(key));view->valid=1;return 1;
}
