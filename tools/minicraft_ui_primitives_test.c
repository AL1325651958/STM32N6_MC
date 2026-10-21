/* Exercise private raster primitives against an independent square-brush
   reference without exposing test-only entry points in the firmware API. */
#include "../Appli/Core/Src/mc_ui.c"
#include <assert.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif
static double now(void)
{
#ifdef _WIN32
  LARGE_INTEGER n,f;QueryPerformanceCounter(&n);QueryPerformanceFrequency(&f);return (double)n.QuadPart/f.QuadPart;
#else
  return (double)clock()/CLOCKS_PER_SEC;
#endif
}
static void reference(uint16_t *fb,int x0,int y0,int x1,int y1,int width,uint16_t color)
{
  if(width<=0)return;
  int dx=x1-x0,dy=y1-y0,n=abs(dx)>abs(dy)?abs(dx):abs(dy);
  for(int i=0;i<=n;i++) {
    int cx=x0+(n?dx*i/n:0),cy=y0+(n?dy*i/n:0);
    for(int y=cy-width/2;y<cy-width/2+width;y++)for(int x=cx-width/2;x<cx-width/2+width;x++)
      if(x>=0&&x<800&&y>=0&&y<480)fb[y*800+x]=color;
  }
}
int main(void)
{
  uint16_t *storage=malloc((800*480+2)*2),*expected=malloc(800*480*2);assert(storage&&expected);
  uint16_t *fb=storage+1;storage[0]=storage[800*480+1]=0xa55a;
  const int lines[][4]={{716,466,642,358},{713,462,642,356},{2,2,-20,-30},{790,470,830,510},
    {300,200,400,200},{300,200,300,330},{500,330,300,200},{300,330,500,200},{300,200,300,200},
    {-20,240,820,241},{10,-40,11,520},{-80,-60,-40,-30},{850,500,890,540}};
  const int widths[]={0,1,2,21,28};
  for(unsigned l=0;l<sizeof(lines)/sizeof(lines[0]);l++)for(unsigned w=0;w<sizeof(widths)/sizeof(widths[0]);w++) {
    for(unsigned p=0;p<800*480;p++)fb[p]=expected[p]=(uint16_t)p;
    const int *v=lines[l];reference(expected,v[0],v[1],v[2],v[3],widths[w],0xe6bd);
    stroke(fb,v[0],v[1],v[2],v[3],widths[w],0xe6bd);
    assert(!memcmp(fb,expected,800*480*2)&&storage[0]==0xa55a&&storage[800*480+1]==0xa55a);
  }
  double start=now();
  for(unsigned i=0;i<2000;i++)reference(expected,716,466,642,358,28,(uint16_t)i);
  double old=now()-start;start=now();
  for(unsigned i=0;i<2000;i++)stroke(fb,716,466,642,358,28,(uint16_t)i);
  double current=now()-start;assert(!memcmp(fb,expected,800*480*2));
  printf("minicraft UI stroke: 65 clipping/direction/width cases match; host brush %.3f ms -> spans %.3f ms (2000 strokes, not board FPS)\n",old*1000,current*1000);
  free(storage);free(expected);return 0;
}
