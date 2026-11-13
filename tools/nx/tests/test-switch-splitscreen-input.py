"""Exercise the native libnx-to-XInput bridge with independent controllers."""
from pathlib import Path
import argparse,re,subprocess
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--compiler',default='/c/msys64/ucrt64/bin/g++.exe');args=p.parse_args()
root=Path(__file__).resolve().parents[3];out=root/'build-nx/switch-splitscreen-input-tests';out.mkdir(parents=True,exist_ok=True)
def clean(s):return re.sub(r'^\s*#include.*\n','',s,flags=re.M)
source=clean((root/'src/nx/nx_xinput.cpp').read_text().split('extern "C" void NX_GameWindowSize',1)[0])
code=r"""
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
using DWORD=uint32_t;using WORD=uint16_t;using BYTE=uint8_t;using SHORT=int16_t;using BOOL=int;
using u32=uint32_t;using s32=int32_t;using u64=uint64_t;
#define ERROR_SUCCESS 0
struct HidAnalogStickState{s32 x=0,y=0;};
struct PadState{u64 mask=0,buttons=0;bool connected=false;HidAnalogStickState sticks[2];};
PadState hardware[9];int configured=0;
constexpr int HidNpadIdType_No2=1,HidNpadStyleSet_NpadStandard=1;
void padConfigureInput(unsigned n,int){configured=n;}
void padInitializeDefault(PadState*p){p->mask=(1ull<<0)|(1ull<<8);}
void padInitialize(PadState*p,int id){p->mask=1ull<<id;}
void padUpdate(PadState*p){p->buttons=0;p->connected=false;p->sticks[0]={};p->sticks[1]={};
 for(int i=0;i<9;++i)if((p->mask&(1ull<<i))&&hardware[i].connected){p->connected=true;p->buttons|=hardware[i].buttons;p->sticks[0]=hardware[i].sticks[0];p->sticks[1]=hardware[i].sticks[1];}}
bool padIsConnected(PadState*p){return p->connected;}
u64 padGetButtons(PadState*p){return p->buttons;}
HidAnalogStickState padGetStickPos(PadState*p,int n){return p->sticks[n];}
"""
for i,name in enumerate(dict.fromkeys(re.findall(r'HidNpadButton_\w+',source))):code+=f'constexpr u64 {name}=1ull<<{i};\n'
code+=clean((root/'src/nx/compat/XInput.h').read_text())+'\n'+source
code+=r"""
int main(){
 hardware[0].connected=hardware[1].connected=true;
 hardware[0].buttons=HidNpadButton_B;hardware[0].sticks[0].x=12000;
 hardware[1].buttons=HidNpadButton_A|HidNpadButton_ZR;hardware[1].sticks[0].x=-23000;
 XINPUT_STATE p1{},p2{};XINPUT_CAPABILITIES caps{};
 assert(XInputGetState(0,&p1)==0&&p1.Gamepad.wButtons==XINPUT_GAMEPAD_A&&p1.Gamepad.sThumbLX==12000);
#ifdef KISAK_EXPERIMENTAL_SPLITSCREEN
 assert(configured==2&&s_pads[0].mask!=s_pads[1].mask);
 assert(XInputGetCapabilities(1,0,&caps)==0);
 assert(XInputGetState(1,&p2)==0&&p2.Gamepad.wButtons==XINPUT_GAMEPAD_B&&p2.Gamepad.bRightTrigger==255&&p2.Gamepad.sThumbLX==-23000);
 assert(p1.dwPacketNumber==1&&p2.dwPacketNumber==1);
 assert(XInputGetState(1,&p2)==0&&p2.dwPacketNumber==2);
 assert(XInputGetState(0,&p1)==0&&p1.dwPacketNumber==2);
 hardware[1].connected=false;assert(XInputGetCapabilities(1,0,&caps)==1167&&XInputGetState(1,&p2)==1167);
 assert(XInputGetState(0,&p1)==0);hardware[1].connected=true;assert(XInputGetState(1,&p2)==0);
#else
 assert(configured==1&&XInputGetState(1,&p2)==1167&&XInputGetCapabilities(1,0,&caps)==1167);
#endif
 assert(XInputGetState(2,&p2)==1167&&XInputGetState(999,&p2)==1167&&XInputGetState(0,nullptr)==1167);
 hardware[0].connected=false;hardware[8].connected=true;hardware[8].buttons=HidNpadButton_Y;
 assert(XInputGetState(0,&p1)==0&&p1.Gamepad.wButtons==XINPUT_GAMEPAD_X);
 puts("PASS: native Switch controller bridge: independent buttons/sticks/packets, disconnect/reconnect, handheld and regular-build limits");
}
"""
for mode in ('regular','split'):
 cpp=out/(mode+'.cpp');exe=cpp.with_suffix('.exe');cpp.write_text(code)
 subprocess.run([args.compiler,'-std=c++17',*(['-DKISAK_EXPERIMENTAL_SPLITSCREEN=1'] if mode=='split' else []),str(cpp),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
