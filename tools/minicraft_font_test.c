#include "mc_ui.h"
#include "mc_zh.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void utf8(unsigned cp,char s[4])
{s[0]=(char)(0xe0|(cp>>12));s[1]=(char)(0x80|((cp>>6)&63));s[2]=(char)(0x80|(cp&63));s[3]=0;}
static void ppm(const uint16_t *fb,const char *directory,unsigned page)
{
  char path[1024];snprintf(path,sizeof(path),"%s/glyphs-%02u.ppm",directory,page);
  FILE *f=fopen(path,"wb");assert(f);fputs("P6\n800 480\n255\n",f);
  for(unsigned i=0;i<800*480;i++){unsigned c=fb[i];fputc(((c>>11)&31)*255/31,f);fputc(((c>>5)&63)*255/63,f);fputc((c&31)*255/31,f);}
  assert(!ferror(f));fclose(f);
}
int main(int argc,char **argv)
{
  unsigned count=sizeof(mc_hanzi)/sizeof(mc_hanzi[0]);
  uint16_t *storage=malloc((800*480+2)*2),*sheet=calloc(800*480,2);assert(storage&&sheet);
  storage[0]=storage[800*480+1]=0xa55a;uint16_t *fb=storage+1;
  for(unsigned i=0;i<count;i++) {
    const MC_Hanzi *g=&mc_hanzi[i];assert(g->codepoint>=0x800);
    assert(!i || mc_hanzi[i-1].codepoint<g->codepoint);
    for(unsigned other=0;other<i;other++)assert(memcmp(g->large,mc_hanzi[other].large,sizeof(g->large)) || memcmp(g->small,mc_hanzi[other].small,sizeof(g->small)));
    char s[4];utf8(g->codepoint,s);
    if(i%48==0)for(unsigned p=0;p<800*480;p++)sheet[p]=0x1084;
    int cx=(i%8)*96,cy=((i%48)/8)*80;char label[12];snprintf(label,sizeof(label),"%04X",g->codepoint);
    MC_UIText(sheet,cx+4,cy+4,label,0xffc0,1);
    for(unsigned compact=0;compact<2;compact++) {
      unsigned edge=compact?12:16,ink=0;const uint16_t *rows=compact?g->small:g->large;
      for(unsigned p=0;p<800*480;p++)fb[p]=0x4a69;
      MC_UIText(fb,16,16,s,0xffff,(int)compact);
      for(unsigned y=0;y<edge;y++) {
        assert(!(rows[y]>>edge));
        for(unsigned x=0;x<edge;x++) {
          unsigned on=(rows[y]>>(edge-1-x))&1;ink+=on;
          assert(fb[(16+y)*800+16+x]==(on?0xffff:0x4a69));
          if(on)for(int dy=0;dy<2;dy++)for(int dx=0;dx<2;dx++)sheet[(cy+20+(int)y*2+dy)*800+cx+4+(int)compact*40+(int)x*2+dx]=0xffff;
        }
      }
      assert(ink);
      for(unsigned y=0;y<480;y++)for(unsigned x=0;x<800;x++)
        if(x<16||x>=16+edge||y<16||y>=16+edge)assert(fb[y*800+x]==0x4a69);
      MC_UIText(sheet,cx+4+(int)compact*40,cy+60,s,0xffff,(int)compact);
      MC_UIText(fb,795,475,s,0xffff,(int)compact);assert(storage[0]==0xa55a&&storage[800*480+1]==0xa55a);
    }
    if(argc==2 && (i%48==47 || i+1==count))ppm(sheet,argv[1],i/48+1);
  }
  free(storage);free(sheet);printf("minicraft font: %u characters / %u native-size glyphs, UTF-8/pixels/clipping/unique shapes: OK\n",count,count*2);
  return 0;
}
