"""Exercise real multi-port XInput polling, host routing and guest usercmd construction."""
from pathlib import Path
import argparse, re, subprocess
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler',default='/c/msys64/ucrt64/bin/g++.exe')
args=parser.parse_args();root=Path(__file__).resolve().parents[3]
out=root/'build-nx/splitscreen-input-tests';out.mkdir(parents=True,exist_ok=True)
def read(path):return (root/path).read_text(encoding='utf-8')
def function(path,signature):
 s=read(path);start=s.index(signature);end=s.index('{',start)+1;depth=1
 while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
 return s[start:end]
def clean(s):return re.sub(r'^\s*#(?:include|pragma).*\n','',s,flags=re.M).replace('__int16','short').replace('__int32','int')
code=r"""
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <deque>
#include <map>
#include <string>
#include <vector>
#include <stdexcept>
#ifndef __cdecl
#define __cdecl
#endif
#define KISAK_EXPERIMENTAL_SPLITSCREEN 1
bool CL_SplitGuestMenuBlocksInput(){return false;} bool CL_SplitGuestOwnsMenu(){return false;} void CL_SplitGuestMenuFrame(){}
#define KISAK_SP 1
#define XUSER_MAX_COUNT 4
#define __debugbreak() throw std::runtime_error("bad gamepad port")
using DWORD=unsigned long;
struct _XINPUT_GAMEPAD {unsigned short wButtons=0;unsigned char bLeftTrigger=0,bRightTrigger=0;short sThumbLX=0,sThumbLY=0,sThumbRX=0,sThumbRY=0;};
struct _XINPUT_STATE {DWORD dwPacketNumber=0;_XINPUT_GAMEPAD Gamepad;};
struct _XINPUT_VIBRATION {unsigned short wLeftMotorSpeed=0,wRightMotorSpeed=0;};
struct _XINPUT_CAPABILITIES {};
_XINPUT_STATE hardware[4];bool plugged[4]{true,true,true,true};int polls[4]{},checks[4]{},now=1000;
int XInputGetState(int port,_XINPUT_STATE *state){assert(port>=0&&port<4);++polls[port];*state=hardware[port];return plugged[port]?0:1;}
int XInputGetCapabilities(int port,unsigned,_XINPUT_CAPABILITIES *){assert(port>=0&&port<4);++checks[port];return plugged[port]?0:1;}
int XInputSetState(int port,_XINPUT_VIBRATION *){assert(port>=0&&port<4);return 0;}
float Vec2Normalize(float *v){float n=std::sqrt(v[0]*v[0]+v[1]*v[1]);if(n){v[0]/=n;v[1]/=n;}return n;}
template<class... T> bool Assert_MyHandler(T...){return false;}
struct dvar_t {union Value {bool enabled;int integer;float value;unsigned char color[4];const char *string;Value():integer(0){}} current;};
using dvar_s=dvar_t;std::map<std::string,dvar_t> dvars;
const dvar_t *_Dvar_RegisterInt(const char *s,int v,int,int,unsigned,const char *){auto &d=dvars[s];d.current.integer=v;return &d;}
const dvar_t *_Dvar_RegisterFloat(const char *s,float v,float,float,unsigned,const char *){auto &d=dvars[s];d.current.value=v;return &d;}
const dvar_t *_Dvar_RegisterBool(const char *s,bool v,unsigned,const char *){auto &d=dvars[s];d.current.enabled=v;return &d;}
const dvar_t *_Dvar_RegisterString(const char *s,const char *v,unsigned,const char *){auto &d=dvars[s];d.current.string=v;return &d;}
float Dvar_GetFloat(const char *s){return dvars[s].current.value;}
int Dvar_GetInt(const char *s){return dvars[s].current.integer;}
bool Dvar_GetBool(const char *s){return dvars[s].current.enabled;}
void Dvar_SetBool(dvar_t *d,bool v){d->current.enabled=v;}
int diagnosticEvents=0;void CL_SplitGuestLog(const char *){++diagnosticEvents;}
bool guestExists=true,foreground=true,menu=false,catcher=false;
bool CL_IsSplitGuest(int n){return n==1&&guestExists;}
bool IN_IsForegroundWindow(){return foreground;}
bool UI_IsFullscreen(int n){assert(n==0);return menu;}
bool Key_IsCatcherActive(int n,int){assert(n==0);return catcher;}
struct playerState_s {int commandTime=0,clientNum=1,pm_type=0,weapon=1,weaponstate=0,weaponDelay=0,waterlevel=0,offHandIndex=0,cursorHintEntIndex=1023,throwBackGrenadeOwner=0;unsigned otherFlags=4,pm_flags=0,eFlags=0,eFlags2=0,weapFlags=0;int stats[5]{};float viewangles[3]{},delta_angles[3]{};};
struct usercmd_s {int serverTime=0,angles[3]{};unsigned short weapon=1,offHandIndex=0;char forwardmove=0,rightmove=0,upmove=0;short gunPitch=0,gunYaw=0;struct {unsigned array[2]{};void setBit(unsigned b){array[b/32]|=0x80000000u>>(b%32);}bool testBit(unsigned b)const{return (array[b/32]&(0x80000000u>>(b%32)))!=0;}} button_bits;};
constexpr int WEAPINVENTORY_PRIMARY=0,OFFHAND_SLOT_LETHAL_GRENADE=1,OFFHAND_SLOT_TACTICAL_GRENADE=2;
unsigned NextWeapInCycle(int n,const playerState_s *,int,unsigned w,bool,bool,bool){assert(n==1);return w==1?2:1;}
bool grenadeAmmo=true;int BG_GetFirstAvailableOffhand(const playerState_s *,int slot){return grenadeAmmo?10+slot:0;}
struct Event {int port,key,down;};std::vector<Event> events,axes;
int Com_LocalClient_GetControllerIndex(int n){assert(n==0);return 0;}
int Sys_Milliseconds(){return now;}
void CL_GamepadEvent(int port,unsigned axis,int value){assert(port==0);axes.push_back({port,(int)axis,value});}
"""
enums=read('src/client/cl_gamepad.h')
for name in ['GamepadPhysicalAxis','GamepadVirtualAxis','GamepadMapping','GamePadStickDir','GamePadButton','GamePadStick']:
 start=enums.index('enum '+name+' :');end=enums.index('};',start)+2;code+=clean(enums[start:end])+'\n'
for name in ['GamepadVirtualAxisMapping','GpadAxesGlob']:
 start=enums.index('struct '+name);end=enums.index('};',start)+2;code+=clean(enums[start:end])+'\n'
keys=read('src/ui/keycodes.h');start=keys.index('enum keyNum_t');end=keys.index('};',start)+2;code+=clean(keys[start:end])+'\n'
code+=r"""
GpadAxesGlob gaGlobs[2];
std::map<int,std::string> regularBindings,secondaryBindings;
const char*Key_GetBinding(int n,unsigned key,int bind){assert(n==0);return (bind?secondaryBindings:regularBindings)[key].c_str();}
int I_strnicmp(const char*a,const char*b,int n){return strncasecmp(a,b,n);}
bool DevGui_IsActive(){return false;}
void initializeBindings(){
 regularBindings={{K_BUTTON_A,"+gostand"},{K_BUTTON_B,"+stance"},{K_BUTTON_X,"+usereload"},{K_BUTTON_Y,"weapnext"},
  {K_BUTTON_LSTICK,"+breath_sprint"},{K_BUTTON_RSTICK,"+melee"},{K_BUTTON_LSHLDR,"+smoke"},{K_BUTTON_RSHLDR,"+frag"},
  {K_BUTTON_LTRIG,"+speed_throw"},{K_BUTTON_RTRIG,"+attack"}};
 gaGlobs[0].virtualAxes[GPAD_VIRTAXIS_SIDE]={GPAD_PHYSAXIS_LSTICK_X,GPAD_MAP_SQUARED};
 gaGlobs[0].virtualAxes[GPAD_VIRTAXIS_FORWARD]={GPAD_PHYSAXIS_LSTICK_Y,GPAD_MAP_SQUARED};
 gaGlobs[0].virtualAxes[GPAD_VIRTAXIS_UP]={GPAD_PHYSAXIS_NONE,GPAD_MAP_LINEAR};
 gaGlobs[0].virtualAxes[GPAD_VIRTAXIS_YAW]={GPAD_PHYSAXIS_RSTICK_X,GPAD_MAP_SQUARED};
 gaGlobs[0].virtualAxes[GPAD_VIRTAXIS_PITCH]={GPAD_PHYSAXIS_RSTICK_Y,GPAD_MAP_SQUARED};
 gaGlobs[0].virtualAxes[GPAD_VIRTAXIS_ATTACK]={GPAD_PHYSAXIS_RTRIGGER,GPAD_MAP_LINEAR};
}
"""
axes=read('src/client/cl_gamepad.cpp');start=axes.index('GamepadPhysicalAxis axisSameStick');end=axes.index('};',start)+2;code+=axes[start:end]+'\n'
code+=function('src/client/cl_gamepad.cpp','double __cdecl CL_GamepadAxisValue(')+'\n'
code+=clean(read('src/win32/win_gamepad.h'))+'\n'
code+=r"""
void CL_GamepadButtonEventForPort(int port,GamePadButton key,int down,unsigned long,GamePadButton){assert(port==0);events.push_back({port,(int)key,down});}
"""
code+=clean(read('src/win32/win_gamepad.cpp'))+'\n'
normal_code=code.replace('#define KISAK_EXPERIMENTAL_SPLITSCREEN 1\n','')
code+=r"""
using OffhandSlot=int;constexpr int OFFHAND_CLASS_NONE=0,OFFHAND_CLASS_FRAG_GRENADE=1,WEAPTYPE_GRENADE=1,WEAPTYPE_MINE=2,WEAPON_OFFHAND_INIT=20,ERR_DROP=1;
struct pmove_t {playerState_s *ps;usercmd_s cmd,oldcmd;};
struct WeaponDef {int offhandClass=OFFHAND_CLASS_FRAG_GRENADE,weapType=WEAPTYPE_GRENADE;};WeaponDef grenadeDef;
const WeaponDef *BG_GetWeaponDef(unsigned){return &grenadeDef;}
bool BG_UsingVehicleWeapon(const playerState_s *){return false;}
bool PM_Weapon_IsHoldingGrenade(pmove_t *){return false;}
bool inventory[64];int BG_GetNumWeapons(){return 64;}
bool BG_PlayerHasWeapon(const playerState_s *,unsigned w){return w>0&&w<64&&inventory[w];}
const char *BG_WeaponName(unsigned){return "grenade";}const char *va(const char *,...){return "assert";}
void Com_Error(int,const char *,...){assert(false);}void PM_ExitAimDownSight(playerState_s *){}
void PM_Weapon_OffHandPrepare(pmove_t *){assert(false);}
void PM_SendEmtpyOffhandEvent(playerState_s *,OffhandSlot){assert(false);}
int grenadeEvents=0;void BG_AddPredictableEventToPlayerstate(unsigned event,unsigned,playerState_s *){assert(event==43);++grenadeEvents;}
"""
code+=function('src/bgame/bg_weapons.cpp','void __cdecl PM_Weapon_OffHandInit(')+'\n'
code+=function('src/bgame/bg_weapons.cpp','void __cdecl PM_Weapon_CheckForOffHand(')+'\n'
code+=r"""
int aimCalls=0,aimResets=0;bool aimHandoff=false;
void AimAssist_ResetSplitGuest(){++aimResets;}
bool AimAssist_UpdateSplitGuestInput(const playerState_s *,usercmd_s *command,float seconds,float pitch,float yaw,float *raw,bool){
 assert(seconds>=0&&seconds<=.05f&&pitch>=-1&&pitch<=1&&yaw>=-1&&yaw<=1);
 assert(!command->button_bits.testBit(11)||command->button_bits.array[0]);++aimCalls;if(aimHandoff){assert(command->button_bits.testBit(11));raw[0]=0;raw[1]+=2;return true;}return false;
}
"""
code+=clean(read('src/client/splitscreen_input.cpp'))+'\n'
s=read('src/win32/win_input.cpp');start=s.index('GamePadButton buttonList[32]');end=s.index('};',start)+2
code+=s[start:end]+'\n'+function('src/win32/win_input.cpp','void IN_GamepadsMove()')+'\n'
code+=r"""
usercmd_s frame(playerState_s &ps,int dt=16,bool enabled=true){now+=dt;GPad_UpdateAll();usercmd_s cmd;cmd.serverTime=now;cmd.weapon=ps.weapon;CL_SplitGuestBuildInput(&cmd,&ps,now,dt,enabled);return cmd;}
bool near(float a,float b){return std::fabs(a-b)<0.03f;}
int main(){
 initializeBindings();
 for(bool &owned:inventory)owned=true;
 GPad_InitAll();CL_SplitGuestRegisterInputDvars();dvars["gpad_enabled"].current.enabled=true;
 dvars["input_viewSensitivity"].current.value=1;playerState_s ps;ps.viewangles[1]=90;ps.delta_angles[1]=70;
 // Real XInput polling/storage and all ten port-bound accessors accept slots 0..3.
 hardware[0].Gamepad.sThumbLY=-32767;hardware[0].Gamepad.bRightTrigger=255;
 hardware[1].Gamepad.sThumbLX=32767;hardware[1].Gamepad.sThumbRX=32767;
 for(int i=0;i<4;++i)hardware[i].Gamepad.wButtons=0x1000;
 auto cmd=frame(ps);assert(cmd.rightmove==127&&cmd.forwardmove==0&&!cmd.button_bits.testBit(0));
 assert(cmd.button_bits.testBit(10));float view[3];assert(CL_SplitGuestGetViewAngles(view));assert(near(view[1],87.12f));
 assert(near(cmd.gunYaw*360.0f/65536.0f,view[1])); // absolute SP bullets, not raw delta-relative yaw
 assert(near((cmd.angles[1]*360.0f/65536.0f)+70,view[1]));
 for(int i=0;i<4;++i){assert(polls[i]&&checks[i]&&GPad_IsActive(i));GPad_GetButton(i,GPAD_A);GPad_GetStick(i,GPAD_LX);GPad_IsButtonPressed(i,GPAD_A);GPad_IsButtonReleased(i,GPAD_A);GPad_IsStickPressed(i,GPAD_LX,GPAD_STICK_POS);GPad_IsStickReleased(i,GPAD_LX,GPAD_STICK_POS);}
 bool rejected=false;try{GPad_IsActive(4);}catch(const std::runtime_error&){rejected=true;}assert(rejected);
 // Host only receives the first pad, even with four active devices.
 IN_GamepadsMove();assert(axes.size()==6);for(auto e:events)assert(e.port==0);
 events.clear();axes.clear();dvars["splitguest_controller"].current.integer=0;IN_GamepadsMove();
 assert(axes.size()==6&&events.size()==2);for(auto e:axes)assert(e.down==0);for(auto e:events)assert(e.down==0);
 events.clear();axes.clear();IN_GamepadsMove();assert(events.empty()&&axes.empty());
 cmd=frame(ps);assert(cmd.forwardmove==-127&&cmd.rightmove==0&&cmd.button_bits.testBit(0));
 dvars["splitguest_controller"].current.integer=1;hardware[1].Gamepad={};frame(ps);
 // Hold use/revive, release, triggers, melee and sprint are private guest bits.
 hardware[1].Gamepad.wButtons=0x4000|0x40|0x80;hardware[1].Gamepad.bLeftTrigger=255;hardware[1].Gamepad.bRightTrigger=255;
 cmd=frame(ps);for(int b:{0,1,2,5,11,13})assert(cmd.button_bits.testBit(b));
 hardware[1].Gamepad={};cmd=frame(ps);assert(!cmd.button_bits.array[0]&&!cmd.button_bits.array[1]);
 hardware[1].Gamepad.wButtons=0x8000;cmd=frame(ps);assert(cmd.weapon==2);
 hardware[1].Gamepad.wButtons=0;cmd=frame(ps);assert(cmd.weapon==2); // repeated until server acknowledges
 ps.weapon=2;cmd=frame(ps);assert(cmd.weapon==2);
 hardware[1].Gamepad.wButtons=0x200;cmd=frame(ps);assert(cmd.offHandIndex==11&&cmd.button_bits.testBit(14));
 // Native server accepts the offhand and temporarily holsters primary.
 ps.weaponstate=21;ps.weapFlags=2;ps.weapon=0;cmd=frame(ps);assert(cmd.weapon==2&&cmd.button_bits.testBit(14));
 grenadeAmmo=false;cmd=frame(ps);assert(cmd.button_bits.testBit(14)); // last grenade remains held/cookable
 hardware[1].Gamepad={};cmd=frame(ps);assert(!cmd.button_bits.testBit(14)&&cmd.weapon==2); // release throws
 ps.weaponstate=25;cmd=frame(ps);assert(cmd.weapon==2);ps.weaponstate=0;ps.weapFlags=0;ps.weapon=2;grenadeAmmo=true;frame(ps);
 hardware[1].Gamepad.wButtons=0x100;cmd=frame(ps);assert(cmd.offHandIndex==12&&cmd.button_bits.testBit(15));
 ps.weaponstate=21;cmd=frame(ps);hardware[1].Gamepad={};cmd=frame(ps);assert(!cmd.button_bits.testBit(15));ps.weaponstate=0;
 // A bumper tap rejected while sprinting is retained, released and retried once eligible.
 ps.pm_flags=0x8000;hardware[1].Gamepad.wButtons=0x200;cmd=frame(ps);assert(cmd.button_bits.testBit(14));
 pmove_t pm{&ps};pm.cmd=cmd;PM_Weapon_CheckForOffHand(&pm);assert(!grenadeEvents&&ps.weaponstate==0);
 ps.commandTime=cmd.serverTime;hardware[1].Gamepad={};pm.oldcmd=pm.cmd;cmd=frame(ps);pm.cmd=cmd;assert(!cmd.button_bits.testBit(14));PM_Weapon_CheckForOffHand(&pm);assert(!grenadeEvents);
 // No snapshot acknowledgement yet: a dropped release must stay released.
 cmd=frame(ps);assert(!cmd.button_bits.testBit(14));
 ps.commandTime=cmd.serverTime;ps.pm_flags=0;pm.oldcmd=pm.cmd;cmd=frame(ps);pm.cmd=cmd;assert(cmd.button_bits.testBit(14));
 PM_Weapon_CheckForOffHand(&pm);assert(grenadeEvents==1&&ps.weaponstate==20&&(ps.weapFlags&0x20000));
 cmd=frame(ps);assert(!cmd.button_bits.testBit(14)&&!s_guestInput.pendingGrenade);ps.weaponstate=0;ps.weapFlags=0;
 hardware[1].Gamepad.wButtons=0x2000;cmd=frame(ps);assert(cmd.button_bits.testBit(9));
 cmd=frame(ps,400);assert(cmd.button_bits.testBit(8));
 // Jump cancels a long-press even while B is still held; it must not reassert prone.
 ps.pm_flags=1;hardware[1].Gamepad.wButtons=0x3000;cmd=frame(ps);assert(!cmd.button_bits.testBit(8)&&!cmd.button_bits.testBit(9));
 hardware[1].Gamepad.wButtons=0x2000;cmd=frame(ps,500);assert(!cmd.button_bits.testBit(8)&&s_guestInput.stance==0);
 ps.pm_flags=0;hardware[1].Gamepad={};frame(ps);
 hardware[1].Gamepad.wButtons=0x1000;cmd=frame(ps);assert(!cmd.button_bits.testBit(8));
 // Focus/menu suppression also requires releasing held action buttons before reuse.
 hardware[1].Gamepad={};frame(ps);hardware[1].Gamepad.bRightTrigger=255;foreground=false;
 cmd=frame(ps);assert(!cmd.button_bits.array[0]&&cmd.forwardmove==0);foreground=true;
 cmd=frame(ps);assert(!cmd.button_bits.testBit(0));hardware[1].Gamepad={};frame(ps);
 hardware[1].Gamepad.bRightTrigger=255;cmd=frame(ps);assert(cmd.button_bits.testBit(0));
 catcher=true;cmd=frame(ps);assert(!cmd.button_bits.testBit(0));catcher=false;hardware[1].Gamepad={};frame(ps);
 menu=true;cmd=frame(ps);assert(!s_guestInput.active);menu=false;
 // Disconnect resets stale hardware state even without a removal callback.
 plugged[1]=false;cmd=frame(ps);assert(!s_guestInput.active&&cmd.forwardmove==0&&!cmd.button_bits.array[0]);
 assert(s_gamePads[1].digitals==0&&s_gamePads[1].analogs[1]==0);plugged[1]=true;hardware[1].Gamepad={};frame(ps);
 // View survives delayed snapshots; script delta changes, respawn and pitch clamp work.
 CL_SplitGuestResetInput();ps.viewangles[0]=0;ps.viewangles[1]=90;ps.delta_angles[1]=70;
 hardware[1].Gamepad.sThumbRX=32767;frame(ps);frame(ps);CL_SplitGuestGetViewAngles(view);assert(near(view[1],84.24f));
 ps.delta_angles[1]+=20;frame(ps);CL_SplitGuestGetViewAngles(view);assert(near(view[1],101.36f));
 // Server SetClientViewAngle derives its new delta from the existing raw
 // command, not from a fresh raw zero or a lagging snapshot viewangles value.
 float raw=s_guestInput.rawAngles[1];ps.eFlags^=2;ps.viewangles[1]=10;ps.delta_angles[1]=10-raw;
 hardware[1].Gamepad={};frame(ps);CL_SplitGuestGetViewAngles(view);assert(near(view[1],10));
 // A later spawn may retain the same pm_type/teleport bit and stale snapshot
 // viewangles. Its native spawn counter still resets actions, preserving raw.
 raw=s_guestInput.rawAngles[1];ps.stats[4]++;ps.viewangles[1]=-90;ps.delta_angles[1]=150-raw;
 frame(ps);CL_SplitGuestGetViewAngles(view);assert(near(view[1],150));
 auto spawnCmd=frame(ps);float serverYaw=SplitGuestNormalizeAngle(spawnCmd.angles[1]*360.0f/65536.0f+ps.delta_angles[1]);
 assert(near(serverYaw,view[1]));
 hardware[1].Gamepad.sThumbRY=32767;frame(ps);CL_SplitGuestGetViewAngles(view);assert(view[0]<0);
 dvars["input_invertPitch"].current.enabled=true;frame(ps);CL_SplitGuestGetViewAngles(view);assert(near(view[0],0));
 for(int i=0;i<60;++i)frame(ps,50);
 CL_SplitGuestGetViewAngles(view);assert(near(view[0],85));
 ps.pm_type=6;hardware[1].Gamepad={};hardware[1].Gamepad.bRightTrigger=255;cmd=frame(ps);assert(cmd.button_bits.testBit(0));
 ps.pm_type=4;cmd=frame(ps);assert(!cmd.button_bits.testBit(0));
 ps.pm_type=0;ps.pm_flags=0x800;cmd=frame(ps);assert(!cmd.button_bits.array[0]);
 ps.pm_flags=0;hardware[1].Gamepad={};ps.weaponstate=0;ps.weapon=2;CL_SplitGuestResetInput();frame(ps);
 CL_SplitGuestSelectWeapon(3);cmd=frame(ps);assert(cmd.weapon==3);for(int i=0;i<4;++i){cmd=frame(ps);assert(cmd.weapon==3);}
 ps.weapon=3;cmd=frame(ps);assert(cmd.weapon==3&&!s_guestWeaponRequest.valid);
 // Purchase command arrives before the inventory update and old gun is removed.
 inventory[4]=false;CL_SplitGuestSelectWeapon(4);inventory[3]=false;cmd=frame(ps);assert(cmd.weapon!=3&&s_guestWeaponRequest.valid);
 inventory[4]=true;ps.weapon=0;cmd=frame(ps);assert(cmd.weapon==4);ps.weapon=4;frame(ps);assert(!s_guestWeaponRequest.valid);
 // Revive selection survives the transition reset, including a weaponless PS.
 CL_SplitGuestSelectWeapon(5);ps.pm_type=6;ps.weapon=0;frame(ps);ps.pm_type=0;cmd=frame(ps);assert(cmd.weapon==5);
 ps.weapon=5;frame(ps);CL_SplitGuestSelectWeapon(0);cmd=frame(ps);assert(!cmd.weapon);ps.weapon=0;cmd=frame(ps);assert(!cmd.weapon);
 cmd=frame(ps);assert(!cmd.weapon);hardware[1].Gamepad.wButtons=0x8000;cmd=frame(ps);assert(cmd.weapon);hardware[1].Gamepad={};
 CL_SplitGuestSelectWeapon(999);assert(!s_guestWeaponRequest.valid);
 // Following uses target camera angles; sticks do not move or turn that view.
 ps.otherFlags=2;ps.clientNum=0;ps.viewangles[1]=133;ps.weaponstate=35;ps.pm_flags=0x800;
 hardware[1].Gamepad.sThumbRX=32767;hardware[1].Gamepad.sThumbLY=32767;hardware[1].Gamepad.bRightTrigger=255;
 cmd=frame(ps);CL_SplitGuestGetViewAngles(view);assert(near(view[1],133)&&!cmd.forwardmove&&cmd.button_bits.testBit(0));
 float followRaw=s_guestInput.rawAngles[1];ps.otherFlags=4;ps.clientNum=1;ps.pm_flags=0;ps.weaponstate=0;ps.weapon=2;ps.delta_angles[1]=80-followRaw;ps.stats[4]++;
 hardware[1].Gamepad={};cmd=frame(ps);CL_SplitGuestGetViewAngles(view);assert(near(view[1],80));
 // Native output takes over camera and encoded SP shot angles, with ADS
 // already present in the command (not delayed until the following frame).
 aimHandoff=true;hardware[1].Gamepad={};hardware[1].Gamepad.bLeftTrigger=255;
 float beforeNative=s_guestInput.rawAngles[1];cmd=frame(ps);CL_SplitGuestGetViewAngles(view);
 assert(near(s_guestInput.rawAngles[1],SplitGuestNormalizeAngle(beforeNative+2)));
 assert(near(SplitGuestNormalizeAngle((unsigned short)cmd.gunYaw*360.0f/65536),view[1]));
 assert(aimCalls>0); CL_SplitGuestResetInput();assert(!CL_SplitGuestGetViewAngles(view));

 // Read profile changes live: swap jump/melee, remap a grenade, secondary
 // bindings, and lefty sticks. P1's axes/bindings must remain untouched.
 initializeBindings();aimHandoff=false;CL_SplitGuestResetInput();hardware[1].Gamepad={};playerState_s remapped;frame(remapped);
 regularBindings[K_BUTTON_A]="+melee";regularBindings[K_BUTTON_RSTICK]="+gostand";
 hardware[1].Gamepad.wButtons=0x1000;cmd=frame(remapped);assert(cmd.button_bits.testBit(2)&&!cmd.button_bits.testBit(10));
 hardware[1].Gamepad={};frame(remapped);hardware[1].Gamepad.wButtons=0x80;cmd=frame(remapped);assert(cmd.button_bits.testBit(10)&&!cmd.button_bits.testBit(2));
 hardware[1].Gamepad={};frame(remapped);regularBindings[K_BUTTON_RSHLDR]="";regularBindings[K_BUTTON_LSHLDR]="+frag";
 hardware[1].Gamepad.wButtons=0x100;cmd=frame(remapped);assert(cmd.button_bits.testBit(14)&&!cmd.button_bits.testBit(15));
 hardware[1].Gamepad={};frame(remapped);secondaryBindings[K_DPAD_UP]=" +reload ; +melee";
 hardware[1].Gamepad.wButtons=1;cmd=frame(remapped);assert(cmd.button_bits.testBit(5)&&cmd.button_bits.testBit(2));
 hardware[1].Gamepad={};frame(remapped);gaGlobs[0].virtualAxes[GPAD_VIRTAXIS_FORWARD]={GPAD_PHYSAXIS_RSTICK_Y,GPAD_MAP_LINEAR};
 gaGlobs[0].virtualAxes[GPAD_VIRTAXIS_PITCH]={GPAD_PHYSAXIS_LSTICK_Y,GPAD_MAP_LINEAR};
 gaGlobs[0].axesValues[GPAD_PHYSAXIS_RSTICK_Y]=123;
 hardware[1].Gamepad.sThumbRY=32767;cmd=frame(remapped);assert(cmd.forwardmove==127&&gaGlobs[0].axesValues[GPAD_PHYSAXIS_RSTICK_Y]==123);
 hardware[1].Gamepad={};frame(remapped);gaGlobs[0].virtualAxes[GPAD_VIRTAXIS_FORWARD].physicalAxis=GPAD_PHYSAXIS_NONE;
 hardware[1].Gamepad.sThumbRY=32767;cmd=frame(remapped);assert(cmd.forwardmove==0);
 assert(regularBindings[K_BUTTON_A]=="+melee"&&secondaryBindings[K_DPAD_UP]==" +reload ; +melee");
 puts("PASS: P2 honors regular primary/secondary controller remaps and native axis layouts, with no changes to P1 bindings/axis state");
 puts("PASS: four XInput slots, host/guest routing, releases, command actions, SP bullet angles, snapshot-independent look, respawn and focus/disconnect safety");
}
"""
cpp=out/'input.cpp';exe=out/'input.exe';cpp.write_text(code,encoding='utf-8')
subprocess.run([args.compiler,'-std=c++17','-O0','-Wall',str(cpp),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)

normal_cpp=out/'normal.cpp';normal_exe=out/'normal.exe'
normal_cpp.write_text(normal_code+r'''
int main(){GPad_InitAll();GPad_UpdateAll();assert(GPAD_PORT_COUNT==1);assert(polls[0]==1&&checks[0]==1);for(int i=1;i<4;++i)assert(!polls[i]&&!checks[i]);puts("PASS: regular builds retain one physical input port");}
''',encoding='utf-8')
subprocess.run([args.compiler,'-std=c++17','-O0',str(normal_cpp),'-o',str(normal_exe)],check=True)
subprocess.run([str(normal_exe)],check=True)

# Compile a real MP command layout too; SP-only gunPitch/gunYaw must not leak
# into this executable, and dead/spectator/follow input must still request spawn.
mp_code=code[:code.index('int main(){')].replace('#define KISAK_SP 1\n','').replace('short gunPitch=0,gunYaw=0;','')
mp_code+=r"""
int main(){
 initializeBindings();
 for(bool &owned:inventory)owned=true;
 GPad_InitAll();CL_SplitGuestRegisterInputDvars();dvars["gpad_enabled"].current.enabled=true;
 dvars["input_viewSensitivity"].current.value=1;playerState_s ps;ps.viewangles[1]=90;ps.delta_angles[1]=70;
 hardware[1].Gamepad.sThumbRX=32767;auto cmd=frame(ps);assert(cmd.angles[1]!=0);
 for(int mode:{4,9,10}){
  ps.pm_type=mode;hardware[1].Gamepad={};frame(ps);float before=s_guestInput.rawAngles[1];
  hardware[1].Gamepad.wButtons=0x4000;hardware[1].Gamepad.sThumbRX=32767;cmd=frame(ps);
  assert(cmd.button_bits.testBit(5)&&!cmd.forwardmove&&!cmd.rightmove&&s_guestInput.rawAngles[1]==before);
 }
 ps.pm_type=0;ps.otherFlags=2;ps.clientNum=0;ps.viewangles[1]=133;
 hardware[1].Gamepad={};hardware[1].Gamepad.wButtons=0x4000;hardware[1].Gamepad.bRightTrigger=255;cmd=frame(ps);
 assert(cmd.button_bits.testBit(5)&&cmd.button_bits.testBit(0)&&!cmd.forwardmove);
 menu=true;cmd=frame(ps);assert(!cmd.button_bits.array[0]);menu=false;cmd=frame(ps);assert(!cmd.button_bits.testBit(5));
 hardware[1].Gamepad={};frame(ps);hardware[1].Gamepad.wButtons=0x4000;cmd=frame(ps);assert(cmd.button_bits.testBit(5));
 puts("PASS: MP command layout, dead/spectator/follow Use-to-respawn and paused-input release safety");
}
"""
mp_cpp=out/'mp-input.cpp';mp_exe=out/'mp-input.exe';mp_cpp.write_text(mp_code)
subprocess.run([args.compiler,'-std=c++17','-w',str(mp_cpp),'-o',str(mp_exe)],check=True);subprocess.run([str(mp_exe)],check=True)
