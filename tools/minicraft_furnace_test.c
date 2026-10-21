#include "minicraft.h"
#include "mc_save.h"
#include "mc_view.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void reset(MC_World *w)
{
  memset(w->blocks,0,MC_MAP_BYTES);MC_NewPlayer(w,0);
  memset(w->blocks,MC_BEDROCK,MC_MAP_X*MC_MAP_Z);
  for(int z=4;z<35;z++)for(int x=4;x<35;x++)MC_Set(w,x,1,z,MC_STONE);
  w->x=10.5f;w->y=2.01f;w->z=8.5f;w->yaw=w->pitch=0;w->time=435;w->bag=1;
  MC_Set(w,10,2,13,MC_FURNACE);w->furnaces[0]=(MC_Furnace){.x=11,.y=3,.z=14};
}
static void tick(MC_World *w,unsigned count){for(unsigned i=0;i<count;i++)MC_SystemsTick(w,.1f);}
int main(void)
{
  MC_World w={.blocks=calloc(1,MC_MAP_BYTES)},loaded={.blocks=calloc(1,MC_MAP_BYTES)};
  uint8_t *save=malloc(MC_SAVE_SIZE);MC_View *view=calloc(1,sizeof(*view));uint16_t *frame=malloc(sizeof(view->pixels));
  assert(w.blocks&&loaded.blocks&&save&&view&&frame);reset(&w);MC_Furnace *f=&w.furnaces[0];
  assert(MC_FurnaceState(f)==MC_FURNACE_EMPTY);
  f->output=(MC_Stack){MC_IRON,1,0};assert(MC_FurnaceState(f)==MC_FURNACE_DONE);
  f->output=(MC_Stack){0};f->input=(MC_Stack){MC_DIRT,1,0};assert(MC_FurnaceState(f)==MC_FURNACE_INVALID);
  f->input=(MC_Stack){MC_IRON_ORE,1,0};assert(MC_FurnaceState(f)==MC_FURNACE_NEEDS_FUEL);
  f->fuel=(MC_Stack){MC_COAL,2,0};assert(MC_FurnaceState(f)==MC_FURNACE_READY);
  f->output=(MC_Stack){MC_GLASS,1,0};assert(MC_FurnaceState(f)==MC_FURNACE_BLOCKED);
  MC_SaveEncode(&w,4,save);assert(MC_SaveDecode(&loaded,save,MC_SAVE_SIZE)&&MC_FurnaceState(&loaded.furnaces[0])==MC_FURNACE_BLOCKED);
  f->output=(MC_Stack){MC_IRON,64,0};f->progress=2;tick(&w,10);
  assert(f->fuel.count==2&&!f->burn&&f->input.count==1&&f->output.count==64);
  f->burn=.2f;tick(&w,10);assert(!f->burn&&f->fuel.count==2&&!f->progress&&MC_FurnaceState(f)==MC_FURNACE_BLOCKED);
  w.open_furnace=0;assert(MC_ContainerTap(&w,2)&&w.items[MC_IRON]==64);
  assert(MC_FurnaceState(f)==MC_FURNACE_READY);tick(&w,1);
  assert(MC_FurnaceState(f)==MC_FURNACE_SMELTING&&f->fuel.count==1&&f->burn>39&&f->progress>0);

  const unsigned inputs[]={MC_IRON_ORE,MC_SAND,MC_COBBLE,MC_WOOD,MC_BIRCH_WOOD,MC_DARK_WOOD,MC_RAW_MEAT};
  const unsigned outputs[]={MC_IRON,MC_GLASS,MC_BRICK,MC_COAL,MC_COAL,MC_COAL,MC_COOKED_MEAT};
  for(unsigned recipe=0;recipe<sizeof(inputs)/sizeof(inputs[0]);recipe++) {
    reset(&w);f=&w.furnaces[0];f->input=(MC_Stack){inputs[recipe],2,0};f->fuel=(MC_Stack){MC_COAL,1,0};
    assert(MC_SmeltResult(inputs[recipe])==outputs[recipe]);
    MC_SaveEncode(&w,4,save);assert(MC_SaveValidate(save,MC_SAVE_SIZE)&&MC_SaveDecode(&loaded,save,MC_SAVE_SIZE));
    assert(loaded.furnaces[0].input.item==inputs[recipe]);
    tick(&w,10);assert(MC_FurnaceBurningAt(&w,10,2,13)&&MC_FurnaceState(f)==MC_FURNACE_SMELTING);
    MC_SaveEncode(&w,5,save);assert(MC_SaveDecode(&loaded,save,MC_SAVE_SIZE));
    assert(loaded.furnaces[0].burn==f->burn&&loaded.furnaces[0].progress==f->progress&&MC_FurnaceBurningAt(&loaded,10,2,13));
    tick(&w,90);assert(!f->input.count&&f->output.item==outputs[recipe]&&f->output.count==2&&MC_FurnaceState(f)==MC_FURNACE_DONE);
    assert(f->fuel.count==0&&f->burn>29&&w.experience==2);
    tick(&w,310);assert(!f->burn&&f->output.count==2&&MC_FurnaceState(f)==MC_FURNACE_DONE);
    printf("furnace recipe %u -> %u: input, burn, output and save roundtrip OK\n",inputs[recipe],outputs[recipe]);
  }
  assert(MC_FuelSeconds(MC_COAL)==40&&MC_FuelSeconds(MC_BIRCH_WOOD)==15&&MC_FuelSeconds(MC_DARK_PLANKS)==10&&!MC_FuelSeconds(MC_DIRT));
  /* Production progress stays below five across completion boundaries; retain
     strict save validation instead of accepting impossible timer values. */
  reset(&w);f=&w.furnaces[0];f->input=(MC_Stack){MC_IRON_ORE,3,0};f->fuel=(MC_Stack){MC_COAL,1,0};
  for(unsigned step=0;step<=101;step++) {
    assert(f->progress>=0&&f->progress<5&&f->burn>=0&&f->burn<=40);
    if(step<=1 || (step>=49&&step<=51) || step>=99) {
      MC_SaveEncode(&w,7,save);assert(MC_SaveDecode(&loaded,save,MC_SAVE_SIZE));
      assert(loaded.furnaces[0].progress==f->progress&&loaded.furnaces[0].burn==f->burn);
    }
    tick(&w,1);
  }
  for(unsigned wood=MC_BIRCH_WOOD;wood<=MC_DARK_WOOD;wood+=2) {
    reset(&w);f=&w.furnaces[0];f->fuel=(MC_Stack){MC_COAL,1,0};
    MC_Set(&w,11,2,13,MC_CHEST);w.chest_map[0][0]=12;w.chest_map[0][1]=3;w.chest_map[0][2]=14;
    w.chest_blocks[0][0]=(MC_Stack){wood,2,0};w.chest_blocks[0][1]=(MC_Stack){MC_COAL,2,0};tick(&w,60);
    unsigned coal=f->fuel.count+(f->output.item==MC_COAL?f->output.count:0);
    for(unsigned i=0;i<MC_CHEST_SLOTS;i++)if(w.chest_blocks[0][i].item==MC_COAL)coal+=w.chest_blocks[0][i].count;
    assert(f->input.item==wood&&f->input.count==1&&coal==3);
  }
  /* Burning transitions change both the furnace and nearby illumination,
     without a map edit. Warm-to-warm changes keep the v31 background reuse. */
  reset(&w);f=&w.furnaces[0];assert(MC_ViewUpdate(view,&w));uint32_t cold=MC_Crc32(view->pixels,sizeof(view->pixels)),revision=w.map_revision;
  f->burn=5;assert(MC_ViewUpdate(view,&w));uint32_t hot=MC_Crc32(view->pixels,sizeof(view->pixels));assert(hot!=cold&&w.map_revision==revision);
  unsigned flames=0;
  for(unsigned i=0;i<MC_VIEW_W*MC_VIEW_H;i++) {
    uint16_t pixel=view->pixels[i];unsigned r=pixel>>11,g=(pixel>>5)&63,b=pixel&31;
    if(r>15&&g>7&&b<10&&r*2>g+8)flames++;
  }
  assert(flames>0);
  MC_Render(&w,frame,MC_VIEW_W,MC_VIEW_H);assert(!memcmp(frame,view->pixels,sizeof(view->pixels)));
  f->burn=4;f->progress=2;assert(!MC_ViewUpdate(view,&w));
  f->burn=0;assert(MC_ViewUpdate(view,&w)&&cold==MC_Crc32(view->pixels,sizeof(view->pixels)));
  assert(!MC_FurnaceBurningAt(&w,10,2,13));
  MC_Set(&w,24,2,10,MC_FURNACE);f->x=25;f->y=3;f->z=11;
  w.bag=0;assert(MC_DarkAt(&w,25,1,10)&&MC_MobSpawn(&w,25,1,10));memset(w.mobs,0,sizeof(w.mobs));
  MC_Set(&w,25,1,12,MC_FARMLAND);MC_Set(&w,25,2,12,MC_WHEAT_CROP);MC_Set(&w,25,1,13,MC_WATER);
  assert(MC_CropGrowthNeeds(&w,25,2,12)&MC_CROP_NEEDS_LIGHT);
  f->burn=1;assert(!MC_DarkAt(&w,25,1,10)&&!MC_MobSpawn(&w,25,1,10));
  assert(!(MC_CropGrowthNeeds(&w,25,2,12)&MC_CROP_NEEDS_LIGHT));
  f->burn=0;assert(MC_DarkAt(&w,25,1,10)&&(MC_CropGrowthNeeds(&w,25,2,12)&MC_CROP_NEEDS_LIGHT));
  free(w.blocks);free(loaded.blocks);free(save);free(view);free(frame);
  puts("minicraft furnace: 7 states / 7 inputs / variants and automation / blocking and fuel / saved heat / flame, light and view invalidation / crop and mob light: OK");return 0;
}
