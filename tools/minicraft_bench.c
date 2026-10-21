#include "minicraft.h"
#include "mc_ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Same camera sequence in both binaries. CRC every rendered frame, outside
   the timed interval; include HUD/minimap to measure the complete CPU path. */
int main(void) {
  MC_World w={.blocks=calloc(1,MC_MAP_BYTES)};
  uint16_t *view=malloc(MC_VIEW_W*MC_VIEW_H*2),*fb=malloc(800*480*2);
  if(!w.blocks||!view||!fb)return 1;
  const char *names[]={"terrain","water","cave_lights","entities"};
  for(unsigned scene=0;scene<4;scene++) {
    MC_Generate(&w,0x20261020u);
    float yaw=w.yaw;
    if(scene==1) {
      for(int z=0;z<MC_MAP_Z;z++)for(int x=0;x<MC_MAP_X;x++)
        for(int y=1;y<20;y++)MC_Set(&w,x,y,z,y<16?MC_STONE:y==16?MC_WATER:MC_AIR);
      w.x=64.5f;w.y=18;w.z=64.5f;w.pitch=-.6f;
    }
    if(scene>=2) {
      memset(w.blocks,0,MC_MAP_BYTES);MC_NewPlayer(&w,0);
      for(int z=4;z<25;z++)for(int x=4;x<25;x++) {
        MC_Set(&w,x,1,z,MC_STONE);
        if(scene==2){MC_Set(&w,x,6,z,MC_STONE);if(x==4||x==24||z==4||z==24)for(int y=2;y<6;y++)MC_Set(&w,x,y,z,MC_BRICK);}
      }
      w.x=10.5f;w.y=2;w.z=10.5f;w.pitch=0;w.time=435;yaw=0;
      for(int i=0;i<8;i++)MC_Set(&w,6+i*2,2,16,MC_TORCH);
      if(scene==3)for(unsigned i=0;i<MC_MOBS;i++) {
        w.mobs[i]=(MC_Mob){7.5f+i,2.01f,14+(float)(i%3),0,0,0,8,1,(uint8_t)(i%2),0};
        w.drops[i]=(MC_Drop){7.5f+i,2.4f,13,0,0,0,MC_APPLE,1};
        w.projectiles[i]=(MC_Projectile){7.5f+i,3,17,0,0,18,0,1};
      }
    }
    double seconds=0;uint32_t hash=0,frame_crc[32];
    for(unsigned frame=0;frame<32;frame++) {
      w.yaw=yaw+frame*.018f;w.time+=.067f;
      clock_t start=clock();
      MC_Render(&w,view,MC_VIEW_W,MC_VIEW_H);
      for(unsigned y=0;y<480;y++)for(unsigned x=0;x<800;x++)fb[y*800+x]=view[y/5*MC_VIEW_W+x/5];
      MC_UIDraw(fb,&w,0,15,67,"BENCHMARK",1,0,0);
      seconds+=(double)(clock()-start)/CLOCKS_PER_SEC;
      hash=(hash<<1)|(hash>>31);frame_crc[frame]=MC_Crc32(fb,800*480*2);hash^=frame_crc[frame];
    }
    printf("%s %.3f %08lx",names[scene],seconds*1000/32,(unsigned long)hash);
    for(unsigned frame=0;frame<32;frame++)printf(" %08lx",(unsigned long)frame_crc[frame]);
    putchar('\n');
  }
  free(fb);free(view);free(w.blocks);return 0;
}
