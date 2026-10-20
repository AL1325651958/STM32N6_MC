#include "voxel_game.h"
#include "minicraft.h"
#include "mc_save.h"
#include "mc_ui.h"
#include "main.h"
#include "ltdc.h"
#include "cacheaxi.h"
#include "gt911.h"
#include "i2c.h"
#include "sdmmc.h"
#include "app_filex.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

#define FB_BYTES (800U * 480U * 2U)
extern uint8_t lcd_bg_buffer[800 * 480 * 4];
__attribute__((section(".game_ram"), aligned(32))) static uint8_t world_mem[MC_MAP_BYTES];
__attribute__((section(".game_ram"), aligned(32))) static uint8_t save_blob[MC_SAVE_SIZE+32];
static uint16_t view[MC_VIEW_W*MC_VIEW_H];
static MC_World world;
static uint32_t generation;
static int active_slot=-1,sd_ready,save_locked,dirty;
static const char *status="NEW WORLD";
static const char *files[2][2]={{"MC0.SAV","MC1.SAV"},{"MC2.SAV","MC3.SAV"}};
static unsigned world_bank,max_frame_ms;
static char saved_status[48];

static void clean_buffer(uint16_t *fb)
{
  SCB_CleanDCache_by_Addr((uint32_t *)fb,FB_BYTES);
  __DSB();
  if(HAL_CACHEAXI_IsEnabled(&hcacheaxi) && HAL_CACHEAXI_CleanByAddr(&hcacheaxi,(const uint32_t *)fb,FB_BYTES)!=HAL_OK) Error_Handler();
  __DSB();
}
static void lcd_init(void)
{
  memset(lcd_bg_buffer,0,FB_BYTES*2);
  clean_buffer((uint16_t *)(void *)lcd_bg_buffer);
  clean_buffer((uint16_t *)(void *)(lcd_bg_buffer+FB_BYTES));
  LTDC_LayerCfgTypeDef cfg={0};
  cfg.WindowX1=800;cfg.WindowY1=480;cfg.PixelFormat=LTDC_PIXEL_FORMAT_RGB565;cfg.Alpha=255;
  cfg.BlendingFactor1=LTDC_BLENDING_FACTOR1_CA;cfg.BlendingFactor2=LTDC_BLENDING_FACTOR2_CA;
  cfg.FBStartAdress=(uint32_t)lcd_bg_buffer;cfg.ImageWidth=800;cfg.ImageHeight=480;
  if(HAL_LTDC_ConfigLayer(&hltdc,&cfg,0)!=HAL_OK) Error_Handler();
}
static void display(unsigned back,unsigned selected,unsigned fps,int touch,float f,float s)
{
  uint16_t *fb=(uint16_t *)(void *)(lcd_bg_buffer+back*FB_BYTES);
  MC_Render(&world,view,MC_VIEW_W,MC_VIEW_H);
  for(unsigned y=0;y<MC_VIEW_H;y++) {
    uint16_t *row=fb+y*5*800;
    for(unsigned x=0;x<MC_VIEW_W;x++) for(unsigned k=0;k<5;k++) row[x*5+k]=view[y*MC_VIEW_W+x];
    for(unsigned k=1;k<5;k++) memcpy(row+k*800,row,1600);
  }
  MC_UIDraw(fb,&world,selected,fps,max_frame_ms,status,touch,f,s);clean_buffer(fb);
  if(HAL_LTDC_SetAddress_NoReload(&hltdc,(uint32_t)fb,0)!=HAL_OK) Error_Handler();
  if(HAL_LTDC_Reload(&hltdc,LTDC_RELOAD_VERTICAL_BLANKING)!=HAL_OK) Error_Handler();
  /* Do not reuse the previous front buffer until hardware completes the swap. */
  uint32_t start=HAL_GetTick();
  while(READ_BIT(hltdc.Instance->SRCR,LTDC_SRCR_VBR)) {
    if(HAL_GetTick()-start>100) {HAL_LTDC_Reload(&hltdc,LTDC_RELOAD_IMMEDIATE);break;}
  }
}
static void load_world(void)
{
  int found_bad=0,found_unknown=0;generation=0;active_slot=-1;save_locked=0;
  sd_ready=HAL_SD_GetState(&hsd2)==HAL_SD_STATE_READY && SD_Card_Init()==FX_SUCCESS;
  if(sd_ready) for(int slot=0;slot<2;slot++) {
    ULONG got=0;UINT ret=SD_File_Read(files[world_bank][slot],save_blob,sizeof(save_blob),&got);
    if(ret==FX_SUCCESS && got>=8 && !memcmp(save_blob,"N6MC",4) && (save_blob[4]>4 || save_blob[5] || save_blob[6] || save_blob[7]))found_unknown=1;
    uint32_t gen=ret==FX_SUCCESS?MC_SaveValidate(save_blob,got):0;
    if(gen && (active_slot<0 || (int32_t)(gen-generation)>0)) {
      MC_SaveDecode(&world,save_blob,got);generation=gen;active_slot=slot;
    } else if(!gen && ret!=FX_NOT_FOUND) found_bad=1;
  }
  if(active_slot<0) {
    MC_Generate(&world,0x20261020u+world_bank);dirty=1;
    save_locked=found_bad;
    status=!sd_ready?"NO SD  VOLATILE WORLD":save_locked?"BAD SAVE  CHECK SD ON PC":"NEW WORLD  SD READY";
  } else {dirty=0;save_locked=found_unknown;status=found_unknown?"NEWER SAVE  READ ONLY":world_bank?"WORLD 2 LOADED":"WORLD 1 LOADED";}
}
static void save_world(void)
{
  if(!sd_ready) {status="NO SD  VOLATILE WORLD";return;}
  if(save_locked) {status="BAD SAVE  CHECK SD ON PC";return;}
  uint32_t start=HAL_GetTick();
  int target=active_slot==0?1:0;
  uint32_t next=generation+1;if(!next) next=1;
  MC_SaveEncode(&world,next,save_blob);
  if(SD_File_Write(files[world_bank][target],save_blob,MC_SAVE_SIZE)!=FX_SUCCESS) {status="SD WRITE FAILED";sd_ready=0;return;}
  ULONG got=0;
  if(SD_File_Read(files[world_bank][target],save_blob,sizeof(save_blob),&got)!=FX_SUCCESS || MC_SaveValidate(save_blob,got)!=next) {
    status="SD VERIFY FAILED";sd_ready=0;return;
  }
  generation=next;active_slot=target;dirty=0;snprintf(saved_status,sizeof(saved_status),"SAVED TO SD  %lu MS",(unsigned long)(HAL_GetTick()-start));status=saved_status;
}

typedef struct {uint8_t active,id,role;uint16_t x,y;uint32_t action;MC_Hold hold;} Contact;
static Contact contacts[GT911_MAX_TOUCHES];

/* Assign a role when a finger first lands; GT911 track IDs keep the two hands independent. */
static void update_contacts(const GT911_StateTypeDef *touch,uint32_t now,unsigned *selected,int *save)
{
  for(unsigned i=0;i<GT911_MAX_TOUCHES;i++) if(contacts[i].active) {
    int found=0;for(unsigned j=0;j<touch->touch_count;j++) if(touch->points[j].track_id==contacts[i].id) found=1;
    if(!found) contacts[i].active=0;
  }
  for(unsigned j=0;j<touch->touch_count;j++) {
    const GT911_PointTypeDef *p=&touch->points[j];Contact *c=NULL;
    for(unsigned i=0;i<GT911_MAX_TOUCHES;i++) if(contacts[i].active && contacts[i].id==p->track_id) c=&contacts[i];
    if(!c) {
      for(unsigned i=0;i<GT911_MAX_TOUCHES;i++) if(!contacts[i].active) {c=&contacts[i];break;}
      if(!c) continue;
      memset(c,0,sizeof(*c));c->active=1;c->id=p->track_id;
      c->role=(uint8_t)MC_UIRole(p->x,p->y);c->x=p->x;c->y=p->y;c->action=now-300;
      MC_HoldStart(&c->hold,p->x,p->y,now);
      if(world.bag) {
        if(c->role==MC_TOUCH_SAVE)*save=1;
        else MC_UIBagTap(&world,p->x,p->y);
        c->role=MC_TOUCH_NONE;continue;
      }
      if(c->role==MC_TOUCH_BAG) {MC_CloseBag(&world);world.bag=1;for(unsigned k=0;k<GT911_MAX_TOUCHES;k++)contacts[k].role=MC_TOUCH_NONE;break;}
      /* Only one finger controls each analog axis. */
      if(c->role==MC_TOUCH_LOOK || c->role==MC_TOUCH_MOVE) for(unsigned i=0;i<GT911_MAX_TOUCHES;i++)
        if(&contacts[i]!=c && contacts[i].active && contacts[i].role==c->role) c->role=MC_TOUCH_NONE;
      if(c->role==MC_TOUCH_SAVE) *save=1;
      if(c->role==MC_TOUCH_SELECT) {
        world.slot=(p->x-240)/50;*selected=world.hotbar[world.slot];
        if(MC_ToolIndex(*selected)>=0)MC_SelectItem(&world,*selected);
      }
      /* A scene tap feeds a pointed animal, fires a bow, or attacks with a
         held tool. Mining still begins only after a long press. */
      if(c->role==MC_TOUCH_LOOK) {
        if(MC_MobFeed(&world,p->x,p->y))dirty=1;
        else if(world.hotbar[world.slot]==MC_BOW ? MC_Shoot(&world,p->x,p->y) :
                (MC_ToolIndex(world.hotbar[world.slot])>=0 && MC_Attack(&world,p->x,p->y)))dirty=1;
      }
    }
    if(c->role==MC_TOUCH_LOOK && !world.bag) {
      MC_HoldMove(&c->hold,p->x,p->y,now);
      world.yaw+=(int)(p->x-c->x)*.006f;world.pitch-=(int)(p->y-c->y)*.005f;
      world.pitch=fminf(1.3f,fmaxf(-1.3f,world.pitch));world.yaw=remainderf(world.yaw,6.2831853f);
    }
    c->x=p->x;c->y=p->y;
  }
}
void VoxelGame_Run(void)
{
  world.blocks=world_mem;lcd_init();load_world();
  int touch_ready=GT911_Init(&hi2c2)==GT911_OK;
  if(!touch_ready) touch_ready=GT911_Init(&hi2c1)==GT911_OK;
  GT911_StateTypeDef touch={0};
  uint32_t previous=HAL_GetTick(),last_report=previous,last_save=previous,fps_time=previous,frames=0;
  unsigned selected=world.hotbar[world.slot],fps=0,back=1;
  for(;;) {
    uint32_t now=HAL_GetTick();if(now-previous>max_frame_ms)max_frame_ms=now-previous;float dt=(now-previous)*.001f;previous=now;int request_save=0,jump=0;
    float f=0,s=0;int holding=0,sneak=0;float mine_x=400,mine_y=240;
    if(touch_ready) {
      GT911_StatusTypeDef ret=GT911_ReadTouches(&touch);
      if(ret==GT911_OK) {last_report=now;update_contacts(&touch,now,&selected,&request_save);}
      else if(ret!=GT911_NO_DATA || now-last_report>250) memset(contacts,0,sizeof(contacts));
    }
    for(unsigned i=0;i<GT911_MAX_TOUCHES;i++) if(contacts[i].active && !world.bag) {
      Contact *c=&contacts[i];
      if(c->role==MC_TOUCH_MOVE) { f=(350.0f-c->y)/65;s=(c->x-110.0f)/65; }
      else if(c->role==MC_TOUCH_JUMP) jump=1;
      else if(c->role==MC_TOUCH_SNEAK) sneak=1;
      else if(c->role==MC_TOUCH_LOOK && MC_HoldReady(&c->hold,now)) {
        holding=1;mine_x=c->x;mine_y=c->y;
      }
      else if(c->role==MC_TOUCH_PLACE && now-c->action>=250) {
        if(MC_Use(&world)) dirty=1;
        c->action=now;
        if(world.bag){for(unsigned k=0;k<GT911_MAX_TOUCHES;k++)contacts[k].role=MC_TOUCH_NONE;f=s=0;holding=jump=0;break;}
      }
    }
    if(f || s || jump) dirty=1;
    world.sneak=(uint8_t)sneak;
    MC_SystemsTick(&world,dt);
    if(!world.bag) {
      MC_Step(&world,f,s,dt,jump);
      MC_PlayTick(&world,dt,holding,mine_x,mine_y);dirty=1;
    } else {world.mining=0;world.target_valid=0;dirty=1;}
    selected=world.hotbar[world.slot];
    /* Save on request, and once per 30s when the player changed the world or position. */
    int do_save=request_save || (dirty && sd_ready && !save_locked && now-last_save>=30000);
    if(do_save && sd_ready && !save_locked) status="SAVING SD";
    display(back,selected,fps,touch_ready,f,s);back^=1;
    if(world.change_world) {
      world.change_world=0;
      if(!sd_ready || save_locked)world.notice="WORLD SWITCH NEEDS WRITABLE SD";
      else {
        /* Save the current bank before loading another; separate filenames preserve builds. */
        save_world();
        if(sd_ready) {
          world_bank^=1;load_world();
          world.notice=world_bank?"WORLD 2  ORES AND CAVES":"WORLD 1  ORIGINAL WORLD";
          if(dirty && !save_locked)save_world();
          memset(contacts,0,sizeof(contacts));
        }
      }
      last_save=HAL_GetTick();previous=last_save;
    } else if(do_save) {save_world();last_save=HAL_GetTick();previous=last_save;for(unsigned k=0;k<GT911_MAX_TOUCHES;k++)contacts[k].role=MC_TOUCH_NONE;}
    frames++;
    if(now-fps_time>=1000) {fps=(unsigned)(frames*1000/(now-fps_time));frames=0;fps_time=now;HAL_GPIO_TogglePin(LED1_GPIO_Port,LED1_Pin);}
  }
}
