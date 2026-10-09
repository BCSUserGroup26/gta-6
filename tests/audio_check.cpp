#define main unused_app_main
#include "../src/main.cpp"
#undef main
#include <cassert>
static int openCalls,closeCalls,outputs,caseNumber;
extern "C" int sceAudioOutInit(){return caseNumber==1?-1:0;}
extern "C" int sceAudioOutOpen(int u,int type,int index,uint32_t grain,uint32_t rate,uint32_t fmt){
 assert(type==0&&index==0&&grain==256&&rate==48000&&fmt==1);++openCalls;
 if(caseNumber==2&&openCalls==1){assert(u==0xff);return -1;}
 assert(u==(caseNumber==2?7:0xff));return 10;
}
extern "C" int sceAudioOutSetVolume(int h,int mask,const int* levels){assert(h==10&&mask==3);for(int i=0;i<8;i++)assert(levels[i]==0x8000);return 0;}
extern "C" int sceAudioOutOutput(int h,const void*p){assert(h==10);auto*s=static_cast<const int16_t*>(p);for(int i=0;i<512;i++)assert(s[i]==0);++outputs;return -1;}
extern "C" int sceAudioOutClose(int h){assert(h==10);++closeCalls;return 0;}
extern "C" int sceKernelSendNotificationRequest(uint32_t,void*,size_t,int){return 0;}
int main(){for(caseNumber=0;caseNumber<3;caseNumber++){openCalls=closeCalls=outputs=0;user=7;audioReady=false;audioTrack=0;audioGeneration=0;audioWorker(nullptr);assert(openCalls==(caseNumber==2?2:1));assert(closeCalls==1&&outputs==1&&!audioReady);}puts("PASS stereo format, service reinitialization, system-user fallback, channel volume, output failure cleanup");}
