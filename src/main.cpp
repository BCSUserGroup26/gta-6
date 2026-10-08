// SPDX-License-Identifier: GPL-3.0-or-later
#include "demo_renderer.hpp"
#include <cstdio>
#include <cstddef>
#define PL_MPEG_IMPLEMENTATION
#include "pl_mpeg.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <array>
extern "C" {
uint64_t sceKernelGetProcessTime();
int sceKernelUsleep(uint32_t);
int sceKernelSendNotificationRequest(uint32_t,void*,size_t,int);
int sceUserServiceInitialize(const void*);
int sceUserServiceGetInitialUser(int*);
int scePadInit();
int scePadOpen(int,int,int,const void*);
struct PadSample { uint32_t buttons; uint8_t sticks[6]; uint8_t r0[66]; int32_t connected; uint64_t time; uint8_t r1[32]; };
int scePadRead(int,PadSample*,int);
int sceAudioOutInit();
int sceAudioOutOpen(int,int,int,uint32_t,uint32_t,uint32_t);
int sceAudioOutOutput(int,const void*);
int scePthreadCreate(void**,const void*,void*(*)(void*),void*,const char*);
}
static_assert(sizeof(PadSample)==120);
using ps5::demo::Canvas;
using ps5::demo::Color;
static double now(){return sceKernelGetProcessTime()/1000000.0;}
static float clamp(double n){return n<0?0:n>1?1:float(n);}
static void notify(const char* msg){struct {uint8_t reserved[45];char message[3075];} n{};std::snprintf(n.message,sizeof n.message,"%s",msg);sceKernelSendNotificationRequest(0,&n,sizeof n,0);}
static std::atomic<int> audioTrack{0},audioGeneration{0};
static std::atomic<float> volume{1};
static int user=0,pad=-1;
static bool music=true;
static void setAudio(int track){audioTrack=track; audioGeneration.fetch_add(1);}
static const char* tracks[]={"","menu","loading","loop","rickroll"};
static void* audioWorker(void*) {
    int h=sceAudioOutOpen(user,0,0,256,48000,2);
    if(h<0){notify("Audio output unavailable");return nullptr;}
    FILE* f=nullptr; int generation=-1,track=0; std::array<int16_t,512> pcm{};
    for(;;){
        int gen=audioGeneration.load(); if(gen!=generation){if(f)std::fclose(f);f=nullptr;generation=gen;track=audioTrack.load();if(track){char path[128];std::snprintf(path,sizeof path,"/app0/assets/%s.pcm",tracks[track]);f=std::fopen(path,"rb");}}
        pcm.fill(0);
        if(f){size_t n=std::fread(pcm.data(),sizeof(int16_t),pcm.size(),f);if(n<pcm.size()&&track==1){std::rewind(f);std::fread(pcm.data()+n,sizeof(int16_t),pcm.size()-n,f);}}
        float v=volume.load();for(auto& sample:pcm)sample=int16_t(sample*v);
        if(sceAudioOutOutput(h,pcm.data())<0)sceKernelUsleep(5333);
    }
}
struct Image { unsigned char* bytes=nullptr; int w=0,h=0; bool load(const char* n,int width,int height){w=width;h=height;char p[128];std::snprintf(p,sizeof p,"/app0/assets/%s.rgba",n);FILE*f=std::fopen(p,"rb");if(!f)return false;bytes=(unsigned char*)std::malloc(w*h*4);bool ok=bytes&&std::fread(bytes,1,w*h*4,f)==size_t(w*h*4);std::fclose(f);if(!ok){std::free(bytes);bytes=nullptr;}return ok;}
 void draw(Canvas& c,int cx,int cy,int width,float alpha=1,bool yellow=false){c.image(bytes,w,h,cx-width/2,cy-width*h/w/2,width,width*h/w,alpha,yellow);}
};
static Image bg,logo,newText,settingsText,thanksText,backText,soundText,errorText;
static plm_video_t* video=nullptr;
static std::array<unsigned char,640*360*4> pixels{};
static bool haveFrame=false;
static double clipStart=0;
static unsigned decoded=0;
static bool openClip(const char* n,int track){if(video)plm_video_destroy(video);video=nullptr;haveFrame=false;decoded=0;char p[128];std::snprintf(p,sizeof p,"/app0/assets/%s.m1v",n);auto*b=plm_buffer_create_with_filename(p);if(!b)return false;video=plm_video_create_with_buffer(b,1);if(!video)return false;clipStart=now();setAudio(track);return true;}
static bool drawClip(Canvas& c){if(!video)return false;unsigned due=unsigned((now()-clipStart)*25)+1;while(decoded<due){auto*f=plm_video_decode(video);if(!f){if(haveFrame)c.image(pixels.data(),640,360,0,0,1920,1080);return false;}if(f->width!=640||f->height!=360)return false;plm_frame_to_rgba(f,pixels.data(),640*4);haveFrame=true;++decoded;}if(haveFrame)c.image(pixels.data(),640,360,0,0,1920,1080);return true;}
enum class State {Thanks,Intro,Menu,Settings,MenuExit,Loading,Fade,Reveal,Final,Error};
static State state=State::Thanks;
static double entered=0,lastNav=0;
static int selected=0,repeat=0,held=0;
static uint32_t buttonsBefore=0;
static void enter(State s){state=s;entered=now();buttonsBefore=0;held=0;lastNav=entered;}
static void fail(){setAudio(0);enter(State::Error);notify("Media could not be opened");}
static void input(int& direction,bool& confirm,bool& back){direction=0;confirm=false;back=false;if(pad<0)return;std::array<PadSample,64> data{};int count=scePadRead(pad,data.data(),64);if(count<=0)return;const auto&s=data[count-1];if(!s.connected){buttonsBefore=0;held=0;return;}uint32_t b=s.buttons;int dir=((b&0x10)||s.sticks[1]<70||s.sticks[3]<70)?-1:((b&0x40)||s.sticks[1]>185||s.sticks[3]>185)?1:0;
 if(dir&&(dir!=held||now()-lastNav>0.25)){direction=dir;lastNav=now();}held=dir;confirm=(b&0x4000)&&!(buttonsBefore&0x4000);back=(b&0x2000)&&!(buttonsBefore&0x2000);buttonsBefore=b;}
static void draw(Canvas& c) noexcept {
 c.clear(static_cast<Color>(0xff000000));double t=now()-entered;int dir;bool confirm,back;input(dir,confirm,back);
 if(state==State::Thanks){thanksText.draw(c,960,540,1450);if(t>=2){enter(State::Intro);if(!openClip("loading",2))fail();}}
 else if(state==State::Intro){if(!drawClip(c)){setAudio(0);enter(State::Menu);selected=0;}}
 else if(state==State::Menu||state==State::Settings){
    bg.draw(c,960,540,1920,clamp(t));
    // The background settles at 1s; after a 1s delay logo and music begin at 2s.
    logo.draw(c,1440,530,680,clamp(t-2));
    if(t>=2&&audioTrack.load()==0&&music){volume=1;setAudio(1);}
    if(t>=3){
        c.rectangle(130,300,580,400,static_cast<Color>(0xff201327));
        if(dir)selected=(selected+dir+2)%2;
        Image* items[2]={state==State::Menu?&newText:&soundText,state==State::Menu?&settingsText:&backText};
        for(int i=0;i<2;i++)items[i]->draw(c,420,420+i*160,selected==i?640:520,1,selected==i);
        if(state==State::Settings&&back){enter(State::Menu);selected=0;}
        else if(confirm){if(state==State::Menu){if(selected==0){enter(State::MenuExit);}else {state=State::Settings;selected=0;}}
            else if(selected==0){music=!music;setAudio(music?1:0);}else{state=State::Menu;selected=0;}}
    }
 }
 else if(state==State::MenuExit){float a=1-clamp(t/0.6);bg.draw(c,960,540,1920,a);logo.draw(c,1440,530,680,a);volume=a;if(t>=0.6){volume=1;enter(State::Loading);repeat=0;if(!openClip("loop",3))fail();}}
 else if(state==State::Loading){if(!drawClip(c)){if(++repeat<2){if(!openClip("loop",3))fail();}else enter(State::Fade);}}
 else if(state==State::Fade){if(haveFrame)c.image(pixels.data(),640,360,0,0,1920,1080,1-clamp(t/2));volume=1-clamp(t/2);if(t>=2){setAudio(0);volume=1;enter(State::Reveal);}}
 else if(state==State::Reveal){float a=clamp(t/5);float ease=a*a*(3-2*a);logo.draw(c,960,540,int(650+350*ease),a);if(t>=6){enter(State::Final);if(!openClip("rickroll",4))fail();}}
 else if(state==State::Final){if(!drawClip(c)){setAudio(0);enter(State::Menu);selected=0;}}
 else errorText.draw(c,960,540,1450);
}
int main(){
 notify("License Killer Started for PPSA99991 : \"Grand Theft Auto 6\"");sceKernelUsleep(2000000);
 notify("License Killer, killed Verification for PPSA99991 : \"Grand Theft Auto 6\"");sceKernelUsleep(2000000);
 sceUserServiceInitialize(nullptr);sceUserServiceGetInitialUser(&user);if(scePadInit()>=0)pad=scePadOpen(user,0,0,nullptr);
 if(sceAudioOutInit()>=0){void*thread=nullptr;if(scePthreadCreate(&thread,nullptr,audioWorker,nullptr,"bjb-audio")<0)notify("Audio thread unavailable");}
 bool ok=bg.load("menu",960,540)&&logo.load("logo",720,540);ok=newText.load("new",1000,100)&&ok;ok=settingsText.load("settings",1000,100)&&ok;ok=thanksText.load("thanks",1000,100)&&ok;ok=backText.load("back",1000,100)&&ok;ok=soundText.load("sound",1000,100)&&ok;errorText.load("error",1000,100);
 entered=now();if(!ok)fail();ps5::demo::run_frames(draw,"");
}
