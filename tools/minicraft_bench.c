#include "minicraft.h"
#include "mc_ui.h"
#include "mc_view.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif
static double seconds_now(void)
{
#ifdef _WIN32
  LARGE_INTEGER value,frequency;QueryPerformanceCounter(&value);QueryPerformanceFrequency(&frequency);
  return (double)value.QuadPart/(double)frequency.QuadPart;
#else
  struct timespec t;timespec_get(&t,TIME_UTC);return t.tv_sec+t.tv_nsec*1e-9;
#endif
}

/* Same camera sequence in both binaries. CRC every rendered frame, outside
   the timed interval; include HUD/minimap to measure the complete CPU path. */
static void render_scene(MC_View *view,const MC_World *world)
{
#ifdef MC_BENCH_VIEW_CACHE
  MC_ViewUpdate(view,world);
#else
  MC_Render(world,view->pixels,MC_VIEW_W,MC_VIEW_H);
#endif
}
int main(void) {
  MC_World w={.blocks=calloc(1,MC_MAP_BYTES)};
  MC_View *scene_view=calloc(1,sizeof(*scene_view));uint16_t *fb=malloc(800*480*2);
  if(!w.blocks||!scene_view||!fb)return 1;
  uint16_t *view=scene_view->pixels;
  const char *names[]={"terrain","water","cave_lights","entities","lava","underwater","cutouts","lava_distant","ray_boundaries"};
  for(unsigned scene=0;scene<9;scene++) {
    /* Stable terrain and camera make this a rendering benchmark even when
       new-world spawn selection or optional structures change. */
    MC_GenerateTerrain(&w,0x20261020u);
    w.x=w.z=128.5f;w.y=40.01f;w.yaw=.7f;w.pitch=-.12f;
    float yaw=w.yaw;
    if(scene==1 || scene==4 || scene==5) {
      for(int z=0;z<MC_MAP_Z;z++)for(int x=0;x<MC_MAP_X;x++)
        for(int y=1;y<20;y++)MC_Set(&w,x,y,z,y<16?MC_STONE:y==16?(scene==4?MC_LAVA:MC_WATER):MC_AIR);
      w.x=64.5f;w.y=18;w.z=64.5f;w.pitch=-.6f;
      if(scene==5)w.y=14.5f;
    }
    if(scene==2 || scene==3 || scene==6 || scene==7 || scene==8) {
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
      if(scene==6)for(int x=4;x<25;x++)for(int y=2;y<6;y++) {
        MC_Set(&w,x,y,14,MC_GLASS);
        MC_Set(&w,x,y,17,MC_LEAVES);
        MC_Set(&w,x,y,20,MC_BRICK);
      }
      if(scene==7)MC_Set(&w,10,2,23,MC_LAVA);
      if(scene==8) {
        MC_Set(&w,10,3,13,MC_GLASS);MC_Set(&w,10,4,13,MC_LEAVES);
        MC_Set(&w,13,3,10,MC_TORCH);MC_Set(&w,10,3,7,MC_SLAB_TOP);
        MC_Set(&w,7,3,10,MC_DOOR);MC_Set(&w,7,4,10,MC_DOOR_TOP);
      }
    }
    double seconds=0;uint32_t hash=0,frame_crc[32];
    for(unsigned frame=0;frame<32;frame++) {
      w.yaw=yaw+frame*.018f;w.time+=.067f;
      if(scene==8) {
        /* Exact cell planes, cardinal axes and tiny camera offsets exercise
           DDA ties and near-zero direction components against shaped blocks. */
        w.x=10+(frame%3)*.000001f;w.z=10;w.y=2.38f;
        w.yaw=(frame%4)*1.570796327f;w.pitch=frame%2?0:-.2f;
      }
      double start=seconds_now();
      render_scene(scene_view,&w);
      for(unsigned y=0;y<480;y++)for(unsigned x=0;x<800;x++)fb[y*800+x]=view[y/5*MC_VIEW_W+x/5];
      MC_UIDraw(fb,&w,0,15,67,"BENCHMARK",1,0,0);
      seconds+=seconds_now()-start;
      hash=(hash<<1)|(hash>>31);frame_crc[frame]=MC_Crc32(fb,800*480*2);hash^=frame_crc[frame];
    }
    printf("%s %.3f %08lx",names[scene],seconds*1000/32,(unsigned long)hash);
    for(unsigned frame=0;frame<32;frame++)printf(" %08lx",(unsigned long)frame_crc[frame]);
    putchar('\n');
  }
  const char *systems[]={"systems_idle","systems_edit","systems_move"};
  for(unsigned scenario=0;scenario<3;scenario++) {
    memset(w.blocks,0,MC_MAP_BYTES);MC_NewPlayer(&w,0);
    for(int z=0;z<MC_MAP_Z;z++)for(int x=0;x<MC_MAP_X;x++) {
      w.blocks[z*MC_MAP_X+x]=MC_BEDROCK;MC_Set(&w,x,1,z,MC_DIRT);
    }
    w.x=w.z=64.5f;w.y=2;
    MC_Set(&w,64,2,65,MC_LEVER_ON);MC_Set(&w,65,2,65,MC_DUST);MC_Set(&w,66,2,65,MC_LAMP);
    double seconds=0;uint32_t hash=0,frame_crc[32];
    for(unsigned group=0;group<32;group++) {
      double start=seconds_now();
      for(unsigned tick=0;tick<64;tick++) {
        if(scenario==1 && tick%16==0)MC_Set(&w,64,2,65,tick%32?MC_LEVER_ON:MC_LEVER);
        if(scenario==2)w.x=64.5f+((group*64+tick)/4%2);
        MC_SystemsTick(&w,.1f);
      }
      seconds+=seconds_now()-start;
      frame_crc[group]=MC_Crc32(w.blocks,MC_MAP_BYTES);
      hash=(hash<<1)|(hash>>31);hash^=frame_crc[group];
    }
    /* Per simulated tick, rather than per 64-tick sample. */
    printf("%s %.6f %08lx",systems[scenario],seconds*1000/(32*64),(unsigned long)hash);
    for(unsigned group=0;group<32;group++)printf(" %08lx",(unsigned long)frame_crc[group]);
    putchar('\n');
  }
  const char *physics[]={"physics_ground","physics_water"};
  for(unsigned scene=0;scene<2;scene++) {
    memset(w.blocks,0,MC_MAP_BYTES);MC_NewPlayer(&w,0);
    for(int z=4;z<20;z++)for(int x=4;x<20;x++) {
      MC_Set(&w,x,1,z,MC_STONE);
      if(scene){MC_Set(&w,x,2,z,MC_WATER);MC_Set(&w,x,3,z,MC_WATER);}
    }
    w.x=w.z=10.5f;w.y=2.01f;w.yaw=0;double seconds=0;uint32_t hash=0,checks[32];
    for(unsigned group=0;group<32;group++) {
      double start=seconds_now();
      for(unsigned tick=0;tick<64;tick++) {
        w.x=10.5f+(tick%16)*.005f;w.y=2.01f;w.z=10.5f;w.vy=0;w.on_ground=1;
        MC_Step(&w,1,.5f,.1f,0);
      }
      seconds+=seconds_now()-start;
      float pose[]={w.x,w.y,w.z,w.vy,(float)w.on_ground};checks[group]=MC_Crc32(pose,sizeof(pose));hash=(hash<<1)|(hash>>31);hash^=checks[group];
    }
    printf("%s %.6f %08lx",physics[scene],seconds*1000/(32*64),(unsigned long)hash);
    for(unsigned group=0;group<32;group++)printf(" %08lx",(unsigned long)checks[group]);
    putchar('\n');
  }
  const char *inventory[]={"inventory_static","inventory_chest_edits","inventory_furnace_entities"};
  for(unsigned scene=0;scene<3;scene++) {
    memset(w.blocks,0,MC_MAP_BYTES);MC_NewPlayer(&w,0);
    for(int z=4;z<25;z++)for(int x=4;x<25;x++)MC_Set(&w,x,1,z,MC_STONE);
    w.x=10.5f;w.y=2.01f;w.z=8.5f;w.yaw=w.pitch=0;w.bag=1;MC_Notify(&w,NULL);
    MC_Give(&w,MC_PLANKS,24);MC_Give(&w,MC_TORCH,8);MC_Give(&w,MC_STONE_PICK,1);
    if(scene==1) {
      MC_Set(&w,10,2,13,MC_CHEST);w.open_chest=0;w.chest_blocks[0][0]=(MC_Stack){MC_APPLE,4,0};
    }
    if(scene==2) {
      MC_Set(&w,10,2,13,MC_FURNACE);w.open_furnace=0;
      w.furnaces[0]=(MC_Furnace){11,3,14,{MC_IRON_ORE,8,0},{MC_COAL,4,0},{MC_IRON,1,0},60,0};
      w.drops[0]=(MC_Drop){10.5f,2.5f,11.5f,0,0,0,MC_APPLE,1};
      w.mobs[0]=(MC_Mob){11.5f,2.01f,15,0,0,0,8,1,0,0};
    }
    MC_ViewInvalidate(scene_view);double seconds=0;uint32_t hash=0,checks[32];
    for(unsigned frame=0;frame<32;frame++) {
      w.selected_slot=(uint8_t)(frame%MC_SLOTS);
      if(scene==1 && (frame==8||frame==16))MC_Set(&w,11,2,12,frame==8?MC_TORCH:MC_AIR);
      if(scene==2) {
        w.furnaces[0].progress=(frame%25)*.2f;w.furnaces[0].burn=60-frame;
        w.drops[0].x=10.5f+(frame/8)*.2f;w.mobs[0].hurt=frame%8<4?.3f:0;
      }
      double start=seconds_now();render_scene(scene_view,&w);
      for(unsigned y=0;y<480;y++)for(unsigned x=0;x<800;x++)fb[y*800+x]=view[y/5*MC_VIEW_W+x/5];
      MC_UIDraw(fb,&w,0,15,67,"BENCHMARK",1,0,0);seconds+=seconds_now()-start;
      checks[frame]=MC_Crc32(fb,800*480*2);hash=(hash<<1)|(hash>>31);hash^=checks[frame];
    }
    printf("%s %.3f %08lx",inventory[scene],seconds*1000/32,(unsigned long)hash);
    for(unsigned frame=0;frame<32;frame++)printf(" %08lx",(unsigned long)checks[frame]);
    putchar('\n');
  }
  free(fb);free(scene_view);free(w.blocks);return 0;
}
