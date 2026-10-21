#include "mc_save.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct {int files[4],calls,syncs,fail_remove,fail_sync,fail_probe,ignore_remove;} Card;
static unsigned index_of(const char *name)
{
  assert(strlen(name)==7&&!strncmp(name,"MC",2)&&!strcmp(name+3,".SAV"));
  assert(name[2]>='0'&&name[2]<='3');return (unsigned)(name[2]-'0');
}
static int remove_file(void *ctx,const char *name)
{
  Card *c=ctx;unsigned i=index_of(name);c->calls++;
  if(c->fail_remove==c->calls)return -1;
  if(!c->ignore_remove)c->files[i]=0;
  return 0;
}
static int sync_card(void *ctx){Card *c=ctx;c->syncs++;return c->fail_sync?-1:0;}
static int absent(void *ctx,const char *name){Card *c=ctx;return !c->fail_probe&&!c->files[index_of(name)];}
int main(void)
{
  Card c={{1,1,1,1},0,0,0,0,0,0};MC_Storage s={&c,remove_file,sync_card,absent};
  assert(!MC_WorldFile(2,0)&&!MC_WorldFile(0,2));
  assert(!MC_DeleteWorld(2,&s)&&!c.calls&&!MC_DeleteWorld(0,0));
  assert(MC_DeleteWorld(0,&s)&&!c.files[0]&&!c.files[1]&&c.files[2]&&c.files[3]&&c.calls==2&&c.syncs==1);
  assert(MC_DeleteWorld(0,&s));
  assert(MC_DeleteWorld(1,&s)&&!c.files[2]&&!c.files[3]);
  for(int failure=1;failure<=2;failure++) {
    c=(Card){{1,1,1,1},0,0,failure,0,0,0};
    assert(!MC_DeleteWorld(0,&s)&&c.files[2]&&c.files[3]);
    assert(c.calls==failure&&c.syncs==1&&c.files[failure-1]);
  }
  c=(Card){{1,1,1,1},0,0,0,1,0,0};assert(!MC_DeleteWorld(1,&s)&&c.files[0]&&c.files[1]);
  c=(Card){{1,1,1,1},0,0,0,0,1,0};assert(!MC_DeleteWorld(0,&s));
  c=(Card){{1,1,1,1},0,0,0,0,0,1};assert(!MC_DeleteWorld(0,&s)&&c.files[0]&&c.files[1]);
  puts("minicraft world deletion: both slots / bank isolation / absent files / partial errors / flush / verification: OK");return 0;
}
