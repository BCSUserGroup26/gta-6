// SPDX-License-Identifier: GPL-3.0-or-later
#include "demo_renderer.hpp"
#include "media_format.hpp"
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
int sceAudioOutSetVolume(int,int,const int*);
int sceAudioOutClose(int);
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
static int user=0xff,pad=-1;
static std::atomic<bool> audioReady{false};
static void log(const char* stage,int result=0){char line[192];std::snprintf(line,sizeof line,"%s: 0x%08X\n",stage,unsigned(result));std::fprintf(stderr,"%s",line);FILE*f=std::fopen("/download0/bjb.log","ab");if(f){std::fputs(line,f);std::fclose(f);}}
static void audioError(const char* stage,int code){log(stage,code);char msg[192];std::snprintf(msg,sizeof msg,"Audio unavailable (%s: 0x%08X)",stage,unsigned(code));notify(msg);}
static bool music=true;
static void setAudio(int track){audioTrack=track; audioGeneration.fetch_add(1);}
static const char* tracks[]={"","menu","loading","loop","rickroll"};
static void* audioWorker(void*) {
    int init=sceAudioOutInit(); log("AudioOutInit",init);
    // A port may still open if the service was initialized earlier.
    int h=sceAudioOutOpen(0xff,0,0,bjb::audioFrames,bjb::audioRate,bjb::audioStereoS16);
    log("AudioOutOpen system stereo",h);
    if(h<0&&user!=0xff){h=sceAudioOutOpen(user,0,0,bjb::audioFrames,bjb::audioRate,bjb::audioStereoS16);log("AudioOutOpen signed-in stereo",h);}
    if(h<0){audioError("open",h);return nullptr;}
    std::array<int,8> channelVolume{};channelVolume.fill(0x8000);
    int vr=sceAudioOutSetVolume(h,3,channelVolume.data());log("AudioOutSetVolume",vr);
    if(vr<0){audioError("volume",vr);sceAudioOutClose(h);return nullptr;}
    audioReady=true;
    FILE* f=nullptr; int generation=-1,track=0; std::array<int16_t,bjb::audioSamples> pcm{};
    for(;;){
        int gen=audioGeneration.load(); if(gen!=generation){if(f)std::fclose(f);f=nullptr;generation=gen;track=audioTrack.load();if(track){char path[128];std::snprintf(path,sizeof path,"/app0/assets/%s.pcm",tracks[track]);f=std::fopen(path,"rb");if(!f){log(path,-1);notify("Audio media missing from application assets");}}}
        pcm.fill(0);
        if(f){size_t n=std::fread(pcm.data(),sizeof(int16_t),pcm.size(),f);if(n<pcm.size()&&track==1){std::rewind(f);size_t remaining=std::fread(pcm.data()+n,sizeof(int16_t),pcm.size()-n,f);(void)remaining;}}
        float v=volume.load();for(auto& sample:pcm)sample=int16_t(sample*v);
        int rc=sceAudioOutOutput(h,pcm.data());if(rc<0){audioReady=false;audioError("output",rc);if(f)std::fclose(f);sceAudioOutClose(h);return nullptr;}
    }
}
struct Image { unsigned char* bytes=nullptr; int w=0,h=0; bool load(const char* n,int width,int height){w=width;h=height;char p[128];std::snprintf(p,sizeof p,"/app0/assets/%s.rgba",n);FILE*f=std::fopen(p,"rb");if(!f)return false;bytes=(unsigned char*)std::malloc(w*h*4);bool ok=bytes&&std::fread(bytes,1,w*h*4,f)==size_t(w*h*4);std::fclose(f);if(!ok){std::free(bytes);bytes=nullptr;}return ok;}
 void draw(Canvas& c,int cx,int cy,int width,float alpha=1,bool yellow=false){c.image(bytes,w,h,cx-width/2,cy-width*h/w/2,width,width*h/w,alpha,yellow);}
};
static Image bg,logo,newText,settingsText,thanksText,backText,soundText,errorText,shade,hints;
static plm_video_t* video=nullptr;
static std::array<unsigned char,640*360*4> pixels{};
static bool haveFrame=false,clipFailed=false;
static double clipStart=0;
static unsigned decoded=0;
static bool openClip(const char* n,int track){
    if(video) plm_video_destroy(video);
    video=nullptr;haveFrame=false;clipFailed=false;decoded=0;
    char p[128];std::snprintf(p,sizeof p,"/app0/assets/%s.m1v",n);
    auto*b=plm_buffer_create_with_filename(p);
    if(!b){log(p,-1);return false;}
    video=plm_video_create_with_buffer(b,1);
    if(!video){plm_buffer_destroy(b);log("Video decoder creation",-1);return false;}
    log(p);clipStart=now();setAudio(track);return true;
}
static bool drawClip(Canvas& c){
    if(!video){clipFailed=true;return false;}
    unsigned due=unsigned((now()-clipStart)*25)+1;
    while(decoded<due){
        auto*f=plm_video_decode(video);
        if(!f){
            clipFailed=!haveFrame||!plm_video_has_ended(video);
            if(clipFailed)log("Video decode failed",-1);
            if(haveFrame)c.image(pixels.data(),640,360,0,0,1920,1080);
            return false;
        }
        if(f->width!=640||f->height!=360){clipFailed=true;log("Unexpected video dimensions",-1);return false;}
        plm_frame_to_rgba(f,pixels.data(),640*4);
        bjb::makeVideoOpaque(pixels.data(),pixels.size());
        haveFrame=true;++decoded;
    }
    if(haveFrame)c.image(pixels.data(),640,360,0,0,1920,1080);
    return true;
}
enum class State {Thanks,Intro,Menu,Settings,MenuExit,Loading,Fade,Reveal,Final,Error};
static State state=State::Thanks;
static double entered=0,lastNav=0;
static int selected=0,repeat=0,held=0;
static float optionScale[2]={1.25f,1.0f}, scaleFrom[2]={1.25f,1.0f};
static double selectionChanged=0;
static void updateScales(){float a=clamp((now()-selectionChanged)/0.18);a=a*a*(3-2*a);for(int i=0;i<2;i++){float target=i==selected?1.25f:1.0f;optionScale[i]=scaleFrom[i]+(target-scaleFrom[i])*a;}}
static void selectOption(int index){updateScales();for(int i=0;i<2;i++)scaleFrom[i]=optionScale[i];selected=index;selectionChanged=now();}
static void resetSelection(){selected=0;optionScale[0]=scaleFrom[0]=1.25f;optionScale[1]=scaleFrom[1]=1.0f;selectionChanged=now()-1;}

static uint32_t buttonsBefore=0;
static void enter(State s){state=s;entered=now();buttonsBefore=0;held=0;lastNav=entered;}
static void fail(){setAudio(0);enter(State::Error);log("Media failure",-1);notify("Video unavailable — check application assets");}
static void input(int& direction,bool& confirm,bool& back){direction=0;confirm=false;back=false;if(pad<0)return;std::array<PadSample,64> data{};int count=scePadRead(pad,data.data(),64);if(count<=0)return;const auto&s=data[count-1];if(!s.connected){buttonsBefore=0;held=0;return;}uint32_t b=s.buttons;int dir=((b&0x90)||s.sticks[0]<70||s.sticks[1]<70||s.sticks[2]<70||s.sticks[3]<70)?-1:((b&0x60)||s.sticks[0]>185||s.sticks[1]>185||s.sticks[2]>185||s.sticks[3]>185)?1:0;
 if(dir&&(dir!=held||now()-lastNav>0.25)){direction=dir;lastNav=now();}held=dir;confirm=(b&0x4000)&&!(buttonsBefore&0x4000);back=(b&0x2000)&&!(buttonsBefore&0x2000);buttonsBefore=b;}
static void draw(Canvas& c) noexcept {
 c.clear(static_cast<Color>(0xff000000));double t=now()-entered;int dir;bool confirm,back;input(dir,confirm,back);
 if(state==State::Thanks){thanksText.draw(c,960,540,1450);if(t>=2){enter(State::Intro);if(!openClip("loading",2))fail();}}
 else if(state==State::Intro){if(!drawClip(c)){if(clipFailed){fail();return;}setAudio(0);enter(State::Menu);resetSelection();}}
 else if(state==State::Menu||state==State::Settings){
    bg.draw(c,960,540,1920,clamp(t));
    // The background settles at 1s; after a 1s delay logo and music begin at 2s.
    shade.draw(c,960,930,1920);
    logo.draw(c,1640,230,320,clamp(t-2));
    if(t>=2&&audioTrack.load()==0&&music){volume=1;setAudio(1);}
    if(t>=3){
        if(dir)selectOption((selected+dir+2)%2);
        updateScales();
        Image* items[2]={state==State::Menu?&newText:&soundText,state==State::Menu?&settingsText:&backText};
        float menuAlpha=clamp((t-3)/0.35);
        for(int i=0;i<2;i++)items[i]->draw(c,735+i*450,922,int(560*optionScale[i]),menuAlpha,selected==i);
        unsigned char line[]={255,235,150,255};
        float slide=clamp((now()-selectionChanged)/0.18);slide=slide*slide*(3-2*slide);
        float start=(scaleFrom[1]-1.0f)/0.25f;int lineX=int(735+450*(start+(selected-start)*slide));
        c.image(line,1,1,lineX-58,975,116,3,menuAlpha);
        hints.draw(c,960,1030,850,menuAlpha);
        if(state==State::Settings&&back){enter(State::Menu);resetSelection();}
        else if(confirm){if(state==State::Menu){if(selected==0){enter(State::MenuExit);}else {state=State::Settings;resetSelection();}}
            else if(selected==0){music=!music;setAudio(music?1:0);}else{state=State::Menu;resetSelection();}}
    }
 }
 else if(state==State::MenuExit){float a=1-clamp(t/0.6);bg.draw(c,960,540,1920,a);shade.draw(c,960,930,1920,a);logo.draw(c,1640,230,320,a);for(int i=0;i<2;i++)(i==0?newText:settingsText).draw(c,735+i*450,922,int(560*optionScale[i]),a,i==selected);hints.draw(c,960,1030,850,a);volume=a;if(t>=0.6){volume=1;enter(State::Loading);repeat=0;if(!openClip("loop",3))fail();}}
 else if(state==State::Loading){if(!drawClip(c)){if(clipFailed){fail();return;}if(++repeat<2){if(!openClip("loop",3))fail();}else enter(State::Fade);}}
 else if(state==State::Fade){if(haveFrame)c.image(pixels.data(),640,360,0,0,1920,1080,1-clamp(t/2));volume=1-clamp(t/2);if(t>=2){setAudio(0);volume=1;enter(State::Reveal);}}
 else if(state==State::Reveal){float a=clamp(t/5);float ease=a*a*(3-2*a);logo.draw(c,960,540,int(650+350*ease),a);if(t>=6){enter(State::Final);if(!openClip("rickroll",4))fail();}}
 else if(state==State::Final){if(!drawClip(c)){if(clipFailed){fail();return;}setAudio(0);enter(State::Menu);resetSelection();}}
 else errorText.draw(c,960,540,1450);
}
int main(){
 notify("License Killer Started for PPSA99991 : \"Grand Theft Auto 6\"");sceKernelUsleep(2000000);
 notify("License Killer, killed Verification for PPSA99991 : \"Grand Theft Auto 6\"");sceKernelUsleep(2000000);
 log("UserServiceInitialize",sceUserServiceInitialize(nullptr));int ur=sceUserServiceGetInitialUser(&user);log("GetInitialUser",ur);if(ur<0)user=0xff;if(scePadInit()>=0)pad=scePadOpen(user,0,0,nullptr);
 {void*thread=nullptr;int rc=scePthreadCreate(&thread,nullptr,audioWorker,nullptr,"bjb-audio");log("Audio thread",rc);if(rc<0)audioError("thread",rc);}
 bool ok=shade.load("shade",960,150)&&hints.load("hints",1000,100);ok=bg.load("menu",1920,1080)&&logo.load("logo",720,540)&&ok;ok=newText.load("new",1000,100)&&ok;ok=settingsText.load("settings",1000,100)&&ok;ok=thanksText.load("thanks",1000,100)&&ok;ok=backText.load("back",1000,100)&&ok;ok=soundText.load("sound",1000,100)&&ok;errorText.load("error",1000,100);
 entered=now();if(!ok)fail();ps5::demo::run_frames(draw,"");
}
