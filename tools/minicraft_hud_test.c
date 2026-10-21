#include "minicraft.h"
#include "mc_ui.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void reset(MC_World *w)
{
  MC_NewPlayer(w,0);w->x=w->z=10.5f;w->y=2.01f;w->notice=0;
}
static void draw(const MC_World *w,uint16_t *fb)
{
  for(unsigned i=0;i<800*480;i++)fb[i]=0x4a69;
  MC_UIDraw(fb,w,0,15,67,"TEST",1,0,0);
}
static void changed_only(const uint16_t *a,const uint16_t *b,int x0,int y0,int x1,int y1)
{
  unsigned changes=0;
  for(int y=0;y<480;y++)for(int x=0;x<800;x++)if(a[y*800+x]!=b[y*800+x]) {
    changes++;assert(x>=x0&&x<x1&&y>=y0&&y<y1);
  }
  assert(changes);
}
static int different(const uint16_t *a,const uint16_t *b,int x0,int y0,int x1,int y1)
{
  for(int y=y0;y<y1;y++)for(int x=x0;x<x1;x++)if(a[y*800+x]!=b[y*800+x])return 1;
  return 0;
}
int main(void)
{
  MC_World w={.blocks=calloc(1,MC_MAP_BYTES)};
  uint16_t *storage=malloc((800*480+2)*2),*other=malloc(800*480*2);assert(w.blocks&&storage&&other);
  uint16_t *fb=storage+1;storage[0]=storage[800*480+1]=0xa55a;
  reset(&w);w.health=w.hunger=20;w.experience=MC_XpForLevel(3)+1;draw(&w,fb);
  w.health=19;draw(&w,other);changed_only(fb,other,246,359,460,373);
  assert(!different(fb,other,240,380,640,393));
  memcpy(fb,other,800*480*2);w.health=18;draw(&w,other);changed_only(fb,other,246,359,460,373);
  memcpy(fb,other,800*480*2);w.hunger=19;draw(&w,other);changed_only(fb,other,522,359,691,373);
  memcpy(fb,other,800*480*2);w.hunger=18;draw(&w,other);changed_only(fb,other,522,359,691,373);
  memcpy(fb,other,800*480*2);w.experience++;draw(&w,other);changed_only(fb,other,282,382,632,388);
  assert(!different(fb,other,240,359,691,373));
  reset(&w);assert(MC_Give(&w,MC_STONE_PICK,1));w.slots[0].wear=1;MC_SyncInventory(&w);draw(&w,fb);
  w.slots[0].wear=(uint16_t)MC_ToolLife(MC_STONE_PICK);MC_SyncInventory(&w);draw(&w,other);
  assert(different(fb,other,520,393,640,418));
  assert(!different(fb,other,240,359,691,393));
  reset(&w);assert(MC_Give(&w,MC_PLANKS,64));draw(&w,fb);
  w.slots[0].count=1;MC_SyncInventory(&w);draw(&w,other);assert(different(fb,other,520,393,640,418));
  reset(&w);draw(&w,fb);assert(strcmp(MC_Localize("EMPTY HAND"),MC_ItemName(MC_AIR)));
  assert(!strcmp(MC_Localize("EMPTY HAND"),"空手")&&!strcmp(MC_Localize("EMPTY SLOT"),"空槽"));
  /* A bare hand remains visible without displaying an air block icon. */
  assert(fb[346*800+642]!=0x4a69);
  assert(storage[0]==0xa55a&&storage[800*480+1]==0xa55a);

  for(unsigned a=0;a<MC_ARMOR_SLOTS;a++) {
    reset(&w);unsigned id=MC_IRON_HELMET+a;assert(MC_Give(&w,id,1));w.slots[0].wear=17;w.bag=1;w.open_chest=0;
    MC_UIBagTap(&w,30,115);assert(w.selected_slot==0&&w.slots[0].count&&!w.armor[a].count);
    MC_UIBagTap(&w,435,115);assert(!w.slots[0].count&&w.chest_blocks[0][0].item==id&&w.chest_blocks[0][0].wear==17);
    MC_UIBagTap(&w,435,115);assert(w.slots[0].item==id&&w.slots[0].wear==17&&!w.armor[a].count);
    w.open_chest=255;w.open_furnace=0;
    MC_UIBagTap(&w,30,115);MC_UIBagTap(&w,435,160);
    assert(w.slots[0].item==id&&w.slots[0].wear==17&&!w.furnaces[0].input.count&&!w.armor[a].count);
    MC_CloseBag(&w);w.bag=1;
    MC_UIBagTap(&w,30,115);MC_UIBagTap(&w,30,180);
    assert(!w.slots[0].count&&w.slots[8].item==id&&w.slots[8].wear==17&&!w.armor[a].count);
    MC_UIBagTap(&w,30,180);MC_UIBagTap(&w,440+88*((a+1)%MC_ARMOR_SLOTS),410);
    assert(w.slots[8].count&&!w.armor[a].count);
    MC_UIBagTap(&w,440+88*a,410);assert(!w.slots[8].count&&w.armor[a].item==id&&w.armor[a].wear==17&&w.moving_slot==255);
    MC_UIBagTap(&w,440+88*a,410);assert(w.slots[8].item==id&&w.slots[8].wear==17&&!w.armor[a].count);
    w.armor[a]=(MC_Stack){id,1,(uint16_t)MC_ArmorLife(id)};MC_SyncInventory(&w);
    MC_UIBagTap(&w,440+88*a,410);assert(w.armor[a].wear==17&&w.slots[8].wear==MC_ArmorLife(id)&&w.moving_slot==255);
    draw(&w,fb);assert(storage[0]==0xa55a&&storage[800*480+1]==0xa55a);
  }
  free(w.blocks);free(storage);free(other);
  puts("minicraft HUD: half meters / XP separation / selected count and durability / bare hand / armor storage, move, equip and swap: OK");return 0;
}
