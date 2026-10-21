/* Real FileX and production SD wrappers over an isolated in-memory FAT disk. */
#include "app_filex.h"
#include "mc_save.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define SECTORS 16384U
static UCHAR disk[SECTORS*512U];
static UCHAR format_cache[4096];
static UCHAR large_save[MC_SAVE_SIZE];
static unsigned fail_flush,write_protect;
void fx_stm32_sd_driver(FX_MEDIA *m)
{
  unsigned request=m->fx_media_driver_request;
  m->fx_media_driver_status=FX_SUCCESS;
  if(request==FX_DRIVER_INIT){m->fx_media_driver_write_protect=write_protect;return;}
  if(request==FX_DRIVER_FLUSH){if(fail_flush)m->fx_media_driver_status=FX_IO_ERROR;return;}
  if(request==FX_DRIVER_UNINIT || request==FX_DRIVER_ABORT)return;
  if(request==FX_DRIVER_READ || request==FX_DRIVER_WRITE || request==FX_DRIVER_BOOT_READ || request==FX_DRIVER_BOOT_WRITE) {
    ULONG start=(request==FX_DRIVER_BOOT_READ || request==FX_DRIVER_BOOT_WRITE)?0:m->fx_media_driver_logical_sector;
    ULONG count=m->fx_media_driver_sectors;
    if(start>=SECTORS || count>SECTORS-start){m->fx_media_driver_status=FX_IO_ERROR;return;}
    if(request==FX_DRIVER_READ || request==FX_DRIVER_BOOT_READ)memcpy(m->fx_media_driver_buffer,disk+start*512,count*512);
    else if(write_protect)m->fx_media_driver_status=FX_WRITE_PROTECT;
    else memcpy(disk+start*512,m->fx_media_driver_buffer,count*512);
    return;
  }
  m->fx_media_driver_status=FX_IO_ERROR;
}
/* Include the wrapper to inspect its private media without test hooks in firmware. */
#include "../Appli/FileX/App/app_filex.c"
static int remove_file(void *ctx,const char *name)
{(void)ctx;UINT r=SD_File_Delete(name);return r==FX_SUCCESS||r==FX_NOT_FOUND?0:-1;}
static int sync_files(void *ctx){(void)ctx;return SD_Card_Flush()==FX_SUCCESS?0:-1;}
static int absent(void *ctx,const char *name){(void)ctx;return SD_File_Exists(name)==FX_NOT_FOUND;}
static void remount(void){assert(SD_Card_DeInit()==FX_SUCCESS);assert(SD_Card_Init()==FX_SUCCESS);}
static void check_file(const char *name,const char *value)
{char data[32];ULONG n;assert(SD_File_Read(name,data,sizeof(data),&n)==FX_SUCCESS&&n==strlen(value)&&!memcmp(data,value,n));}
int main(void)
{
  FX_MEDIA format_media={0};MC_Storage storage={0,remove_file,sync_files,absent};
  fx_system_initialize();
  assert(fx_media_format(&format_media,fx_stm32_sd_driver,0,format_cache,sizeof(format_cache),
      "MC TEST",2,512,0,SECTORS,512,1,1,1)==FX_SUCCESS);
  assert(SD_Card_Init()==FX_SUCCESS);
  for(unsigned bank=0;bank<2;bank++)for(unsigned slot=0;slot<2;slot++)
    assert(SD_File_Write(MC_WorldFile(bank,slot),"world",5)==FX_SUCCESS);
  assert(SD_File_Write("KEEP.TXT","unrelated",9)==FX_SUCCESS);
  memset(large_save,0x5a,sizeof(large_save));
  assert(SD_File_Write("MC0.SAV",large_save,sizeof(large_save))==FX_SUCCESS);
  ULONG got=0;UCHAR prefix[17];
  assert(SD_File_Read("MC0.SAV",prefix,sizeof(prefix),&got)==FX_SUCCESS&&got==sizeof(prefix));
  assert(prefix[0]==0x5a&&prefix[16]==0x5a);
  /* Read-only on the file is distinct from write protection of the media. */
  assert(fx_file_attributes_set(&sd_media,"MC0.SAV",FX_READ_ONLY|FX_HIDDEN|FX_SYSTEM|FX_ARCHIVE)==FX_SUCCESS);
  assert(fx_file_attributes_set(&sd_media,"MC1.SAV",FX_READ_ONLY|FX_ARCHIVE)==FX_SUCCESS);
  remount();assert(MC_DeleteWorld(0,&storage));remount();
  assert(absent(0,"MC0.SAV")&&absent(0,"MC1.SAV"));
  check_file("MC2.SAV","world");check_file("MC3.SAV","world");check_file("KEEP.TXT","unrelated");
  assert(MC_DeleteWorld(0,&storage));
  /* An open handle must remain an error; no forced close or false success. */
  assert(fx_file_attributes_set(&sd_media,"MC2.SAV",FX_READ_ONLY|FX_HIDDEN)==FX_SUCCESS);
  FX_FILE held={0};assert(fx_file_open(&sd_media,&held,"MC2.SAV",FX_OPEN_FOR_READ)==FX_SUCCESS);
  assert(!MC_DeleteWorld(1,&storage));assert(fx_file_close(&held)==FX_SUCCESS);
  UINT attributes=0;assert(fx_file_attributes_read(&sd_media,"MC2.SAV",&attributes)==FX_SUCCESS);
  assert(attributes==(FX_READ_ONLY|FX_HIDDEN));
  remount();check_file("MC2.SAV","world");check_file("MC3.SAV","world");
  assert(SD_Card_DeInit()==FX_SUCCESS);write_protect=1;assert(SD_Card_Init()==FX_SUCCESS);
  assert(!MC_DeleteWorld(1,&storage));check_file("MC2.SAV","world");check_file("MC3.SAV","world");
  assert(SD_Card_DeInit()==FX_SUCCESS);write_protect=0;assert(SD_Card_Init()==FX_SUCCESS);
  fail_flush=1;assert(!MC_DeleteWorld(1,&storage));fail_flush=0;
  assert(MC_DeleteWorld(1,&storage));remount();assert(absent(0,"MC2.SAV")&&absent(0,"MC3.SAV"));
  check_file("KEEP.TXT","unrelated");
  assert(SD_File_Delete(0)==FX_PTR_ERROR);assert(SD_Card_DeInit()==FX_SUCCESS);
  assert(SD_File_Delete("MC0.SAV")==FX_NOT_OPEN);
  puts("minicraft real FileX deletion: read-only attributes / remount / bank isolation / open handles / protected media / flush errors: OK");
  return 0;
}
