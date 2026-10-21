#include "minicraft.h"
#include "mc_save.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {int x,z,lo,hi,radius;} Footprint;
int main(void)
{
  MC_World w={.blocks=malloc(MC_MAP_BYTES)},terrain={.blocks=malloc(MC_MAP_BYTES)};
  uint8_t *save=malloc(MC_SAVE_SIZE);assert(w.blocks&&terrain.blocks&&save);
  const uint32_t seeds[]={0,1,2,77,0x20261020u,0xffffffffu,0x12345678u,20261021};
  unsigned regions=0,caches=0;
  for(unsigned seed=0;seed<sizeof(seeds)/sizeof(seeds[0]);seed++) {
    MC_Generate(&w,seeds[seed]);MC_GenerateTerrain(&terrain,seeds[seed]);
    assert(w.x==terrain.x&&w.y==terrain.y&&w.z==terrain.z);
    assert(MC_BodyClear(&w,w.x,w.y,w.z));
    for(unsigned c=0;c<MC_CHESTS;c++)assert(!terrain.chest_map[c][0]);
    Footprint boxes[3];unsigned count=0,surface=0,underground=0;int sx[2],sz[2];
    for(unsigned c=0;c<MC_CHESTS;c++)if(w.chest_map[c][0]) {
      int x=w.chest_map[c][0]-1,y=w.chest_map[c][1]-1,z=w.chest_map[c][2]-1;
      assert(MC_Get(&w,x,y,z)==MC_CHEST&&count<3);
      if(w.chest_blocks[c][0].item==MC_TORCH) {
        int cx=x,cz=z-1,h=y-2;
        assert(surface<2);sx[surface]=cx;sz[surface++]=cz;
        regions|=1u<<((cx>=MC_MAP_X/2)+2*(cz>=MC_MAP_Z/2));
        assert(fabsf(cx+.5f-w.x)>=20 || fabsf(cz+.5f-w.z)>=20);
        assert(MC_BodyClear(&w,cx+.5f,h+2.01f,cz-2.5f));
        assert(w.chest_blocks[c][3].item==MC_STONE_PICK&&w.chest_blocks[c][3].wear==MC_ToolLife(MC_STONE_PICK));
        boxes[count++]=(Footprint){cx,cz,h,h+6,3};
      } else {
        int cx=x-1,cz=z,floor=y-1;underground++;caches++;
        assert(w.chest_blocks[c][0].item==MC_IRON&&w.chest_blocks[c][0].count==2);
        assert(w.chest_blocks[c][1].item==MC_COAL&&w.chest_blocks[c][1].count==6);
        assert(w.chest_blocks[c][2].item==MC_BREAD&&w.chest_blocks[c][2].count==2);
        assert(w.chest_blocks[c][3].item==MC_ARROW&&w.chest_blocks[c][3].count==8);
        assert(MC_Get(&terrain,cx,floor+1,cz-3)==MC_AIR&&MC_Get(&terrain,cx,floor+2,cz-3)==MC_AIR);
        assert(MC_IsSolid(MC_Get(&terrain,cx,floor,cz-3)));
        for(float p=-2.5f;p<=.5f;p+=.2f)assert(MC_BodyClear(&w,cx+.5f,floor+1.01f,cz+p));
        assert(MC_Get(&w,cx,floor+4,cz)==MC_BRICK&&MC_Get(&w,cx-1,floor+1,cz+1)==MC_TORCH);
        boxes[count++]=(Footprint){cx,cz,floor,floor+4,2};
        /* The chest is genuinely reachable and opens the registered contents. */
        float px=w.x,py=w.y,pz=w.z;
        w.x=cx+.5f;w.y=floor+1.01f;w.z=cz+.5f;w.yaw=1.570796327f;w.pitch=-.8f;
        MC_Hit hit;assert(MC_Pick(&w,400,240,&hit)&&hit.block==MC_CHEST);
        assert(MC_SceneTap(&w,400,240)&&w.bag&&w.open_chest==c);MC_CloseBag(&w);
        w.x=px;w.y=py;w.z=pz;
      }
    }
    assert(surface==2&&underground<=1&&count<=3);
    int dx=sx[0]-sx[1],dz=sz[0]-sz[1];assert(dx*dx+dz*dz>=64*64);
    /* No changes outside the small structure footprints, including bedrock,
       the dry natural entrance, water, and the rest of the enlarged map. */
    for(unsigned at=0;at<MC_MAP_BYTES;at++)if(w.blocks[at]!=terrain.blocks[at]) {
      int x=(int)(at%MC_MAP_X),z=(int)(at/MC_MAP_X%MC_MAP_Z),y=(int)(at/(MC_MAP_X*MC_MAP_Z)),allowed=0;
      for(unsigned b=0;b<count;b++)if(abs(x-boxes[b].x)<=boxes[b].radius&&abs(z-boxes[b].z)<=boxes[b].radius&&y>=boxes[b].lo&&y<=boxes[b].hi)allowed=1;
      assert(allowed&&y>0);
    }
    uint32_t crc=MC_Crc32(w.blocks,MC_MAP_BYTES);
    MC_SaveEncode(&w,3,save);assert(MC_SaveDecode(&terrain,save,MC_SAVE_SIZE));
    assert(crc==MC_Crc32(terrain.blocks,MC_MAP_BYTES));
    assert(!memcmp(w.chest_map,terrain.chest_map,sizeof(w.chest_map))&&!memcmp(w.chest_blocks,terrain.chest_blocks,sizeof(w.chest_blocks)));
    MC_Generate(&terrain,seeds[seed]);assert(crc==MC_Crc32(terrain.blocks,MC_MAP_BYTES));
    assert(!memcmp(w.chest_map,terrain.chest_map,sizeof(w.chest_map))&&!memcmp(w.chest_blocks,terrain.chest_blocks,sizeof(w.chest_blocks)));
    if(seed==3) {
      unsigned c=0;while(c<MC_CHESTS&&w.chest_blocks[c][0].item!=MC_IRON)c++;assert(c<MC_CHESTS);
      assert(MC_MoveStack(&w.chest_blocks[c][0],&w.slots[0],0));MC_SyncInventory(&w);
      MC_SaveEncode(&w,4,save);assert(MC_SaveDecode(&terrain,save,MC_SAVE_SIZE));
      assert(!terrain.chest_blocks[c][0].count&&terrain.items[MC_IRON]==2);
      assert(crc==MC_Crc32(terrain.blocks,MC_MAP_BYTES));
    }
    printf("exploration seed %lu: 2 separated ruins, %u connected cave cache\n",(unsigned long)seeds[seed],underground);
  }
  assert(regions==15&&caches>=4);
  free(save);free(w.blocks);free(terrain.blocks);
  puts("minicraft exploration: full-map distribution / dry cave access / loot / reserved storage / bounded edits / deterministic saves: OK");return 0;
}
