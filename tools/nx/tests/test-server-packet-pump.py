"""Exercise Switch server UDP polling and loopback fairness without sockets."""
from pathlib import Path
import argparse
import subprocess

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler',default='/c/msys64/ucrt64/bin/g++.exe')
args=parser.parse_args()
root=Path(__file__).resolve().parents[3]
out=root/'build-nx/server-packet-tests'
out.mkdir(parents=True,exist_ok=True)
source=(root/'src/qcommon/common.cpp').read_text()
start=source.index('void __cdecl Com_ServerPacketEvent()')
end=source.index('void __cdecl Com_EventLoop()',start)
body=source[start:end]
code=r'''
#include <cassert>
#include <cstdio>
#ifndef __cdecl
#define __cdecl
#endif
#define PROF_SCOPED(name) do {} while(0)
constexpr int NS_SERVER=1;
struct netadr_t { int kind=0; };
struct msg_t { unsigned char *data=nullptr; int maxsize=0,cursize=0; };
struct LargeLocal { unsigned char data[65536]; explicit LargeLocal(int size){assert(size==65536);} void *GetBuf(){return data;} };
struct { struct { bool enabled=true; } current; } running;
auto com_sv_running=&running;
int remoteRemaining=0,loopRemaining=0,reads=0,remoteEvents=0,loopEvents=0;
void MSG_Init(msg_t *msg,unsigned char *data,int size){msg->data=data;msg->maxsize=size;}
bool NET_GetClientPacket(netadr_t *adr,msg_t *msg){++reads;if(remoteRemaining==0)return false;if(remoteRemaining>0)--remoteRemaining;adr->kind=1;msg->cursize=4;return true;}
bool NET_GetLoopPacket(int source,netadr_t *adr,msg_t *msg){assert(source==NS_SERVER);if(loopRemaining==0)return false;--loopRemaining;adr->kind=2;msg->cursize=4;return true;}
void SV_PacketEvent(netadr_t adr,msg_t *msg){assert(msg->cursize==4);if(adr.kind==1)++remoteEvents;else {assert(adr.kind==2);++loopEvents;}}
'''+body+r'''
int main(){
    remoteRemaining=70;loopRemaining=3;
    Com_ServerPacketEvent();
#ifdef KISAK_NX
    assert(remoteEvents==64 && remoteRemaining==6 && reads==64);
#else
    assert(remoteEvents==70 && remoteRemaining==0 && reads==71);
#endif
    assert(loopEvents==3 && loopRemaining==0);
    Com_ServerPacketEvent();assert(remoteEvents==70 && remoteRemaining==0);
    running.current.enabled=false;reads=0;remoteRemaining=4;loopRemaining=2;
    Com_ServerPacketEvent();
    assert(reads==0 && remoteRemaining==4 && loopRemaining==0 && loopEvents==3);
    running.current.enabled=true;remoteRemaining=0;reads=0;
    Com_ServerPacketEvent();assert(reads==1);
#ifdef KISAK_NX
    // Endless remote input still leaves a turn for local loopback traffic.
    remoteRemaining=-1;loopRemaining=2;reads=0;remoteEvents=0;
    Com_ServerPacketEvent();assert(reads==64 && remoteEvents==64 && loopEvents==5);
#endif
    puts("PASS: UDP reads, empty queues, receive budget, loopback fairness and stopped server");
}
'''
cpp=out/'pump.cpp';cpp.write_text(code,encoding='utf-8')
for name,definitions in [
 ('nx-solo-sp',['-DKISAK_NX','-DKISAK_SP']),
 ('nx-direct-sp',['-DKISAK_NX','-DKISAK_SP','-DOPENBLOPS_NO_STEAM_AUTH']),
 ('nx-mp',['-DKISAK_NX','-DKISAK_MP']),
 ('nx-direct-mp',['-DKISAK_NX','-DKISAK_MP','-DOPENBLOPS_NO_STEAM_AUTH']),
 ('pc-sp',['-DWIN32','-DKISAK_SP'])
]:
 exe=out/(name+'.exe')
 subprocess.run([args.compiler,'-std=c++20',*definitions,str(cpp),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
 print(name+': passed',flush=True)
