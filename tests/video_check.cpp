#include <cstdio>
#include <cstddef>
#include <vector>
#include <cassert>
#include "../src/media_format.hpp"
#define PL_MPEG_IMPLEMENTATION
#include "../src/pl_mpeg.h"
int main(int argc,char**argv){
 for(int i=1;i<argc;i++){
  auto*b=plm_buffer_create_with_filename(argv[i]);assert(b);auto*v=plm_video_create_with_buffer(b,1);assert(v);
  std::vector<unsigned char> pixels(640*360*4,0);int frames=0;bool rgbChanged=false;
  while(auto*f=plm_video_decode(v)){
   assert(f->width==640&&f->height==360);plm_frame_to_rgba(f,pixels.data(),640*4);
   unsigned char r=pixels[0],g=pixels[1],b=pixels[2];
   bjb::makeVideoOpaque(pixels.data(),pixels.size());assert(pixels[0]==r&&pixels[1]==g&&pixels[2]==b);
   for(size_t j=0;j<pixels.size();j+=4){assert(pixels[j+3]==255);rgbChanged|=pixels[j]!=0||pixels[j+1]!=0||pixels[j+2]!=0;}
   ++frames;
  }
  assert(frames>0&&rgbChanged&&plm_video_has_ended(v));plm_video_destroy(v);
  printf("PASS %s: %d opaque frames, valid EOF\n",argv[i],frames);
 }
}
