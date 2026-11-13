"""Exercise guest command replay, correction smoothing and MP/SP presentation isolation."""
from pathlib import Path
import argparse
import os
import re
import subprocess
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--compiler',default='C:/msys64/ucrt64/bin/g++.exe');a=p.parse_args()
r=Path(__file__).resolve().parents[3];out=r/'build-nx/splitscreen-prediction-tests';out.mkdir(parents=True,exist_ok=True)
def function(path,sig):
 s=(r/path).read_text();start=s.index(sig);end=s.index('{',start)+1;d=1
 while d:d+=(s[end]=='{')-(s[end]=='}');end+=1
 return s[start:end]
code=r'''
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#ifndef __cdecl
#define __cdecl
#endif
#define PROF_SCOPED(x)
#define ZoneTextF(...)
#define __debugbreak() assert(false)
constexpr int CA_ACTIVE=10;
struct Bits {unsigned array[2]{};};
struct usercmd_s {int serverTime=0;int forwardmove=0,rightmove=0;unsigned weapon=1;float angles[3]{};Bits button_bits;};
struct playerState_s {
 int clientNum=1,pm_type=0,otherFlags=0,linkFlags=0,eFlags=0,eFlags2=0,vehiclePos=0;
 int commandTime=1000,groundEntityNum=1022,stats[5]{},bobCycle=0,movementDir=0;
 int weapon=1,weaponstate=0,weaponTime=0,events=0;float fWeaponPosFrac=0;
 float origin[3]{},velocity[3]{},viewangles[3]{},delta_angles[3]{},viewHeightCurrent=60;
};
struct snapshot_s {bool valid=true;int snapFlags=0,serverTime=1000;playerState_s ps;};
struct clientActive_t {snapshot_s snap;usercmd_s cmds[128];int cmdNumber=0,serverTime=1000;
 float cgameOrigin[3]{},cgameVelocity[3]{},cgameViewangles[3]{};int cgamePredictedDataServerTime=0,cgameBobCycle=0,cgameMovementDir=0;};
clientActive_t clientStore[2];clientActive_t *clients=clientStore;
struct clientConnection_t {int clientNum=1;} connections[2];clientConnection_t *clientConnections=connections;
struct clientInfo_t {int conditions=77,turn=11;void *tree=reinterpret_cast<void *>(0x1234);};
struct bgs_t {void *animData=reinterpret_cast<void *>(1);int proneTime[32]{},time=123,frametime=456;clientInfo_t clientinfo[32];} foreign;
thread_local bgs_t *bgs=&foreign;
struct cg_s {snapshot_s *snap=nullptr,*nextSnap=nullptr;bgs_t bgs;playerState_s predictedPlayerState;} games[2];
cg_s *CG_GetLocalClientGlobals(int n){return &games[n];}
struct {int realtime=10,serverId=42,frametime=16;} cls;
bool connected=true;int connectionState=CA_ACTIVE;bool throwMove=false;
bool CL_IsSplitGuest(int n){return n==1&&connected;}
int CL_GetLocalClientConnectionState(int n){assert(n==1);return connectionState;}
std::map<std::string,bool> flags{{"splitguest_predict",true}};
std::map<std::string,float> floats{{"cg_errorDecay",100},{"cg_viewZSmoothingTime",.1f},{"cg_viewZSmoothingMin",1},{"cg_viewZSmoothingMax",32}};
bool Dvar_GetBool(const char *n){return flags[n];}
float Dvar_GetFloat(const char *n){return floats[n];}
float liveYaw=70;bool CL_SplitGuestGetViewAngles(float *a){a[0]=10;a[1]=liveYaw;a[2]=0;return true;}
struct Visitor {const unsigned cookie=0x12345678;int resets=0;void reset(){assert(cookie==0x12345678);++resets;}};
struct pmove_t {playerState_s *ps=nullptr;usercmd_s cmd,oldcmd;int handler=0,localClientNum=0,tracemask=0;float viewChange=0;int viewChangeTime=0;void *m_gjkcc_input=nullptr;
 Visitor proximity_data;float currentPitch=0,averagePitch=0,pitchHistory[30]{};int nextPitch=0,numtouch=0,numGlassTouch=0;};
bool CL_SplitGuestPredictionRunning();
std::vector<int> replayTimes;
float wall=1000,stepAt=1000;int nativeSubsteps=0;
void PmoveSingle(pmove_t *pm){
 assert(pm->handler==0&&pm->localClientNum==0);assert(games[0].snap&&games[0].nextSnap);assert(CL_SplitGuestPredictionRunning());assert(bgs==&games[0].bgs);
#ifdef KISAK_SP
 assert(pm->tracemask==0x281c011);
#else
 assert(pm->tracemask==0x2818011);
#endif
 if(throwMove)throw std::runtime_error("test unwind");
 ++nativeSubsteps;auto &ps=*pm->ps;assert(pm->cmd.serverTime>ps.commandTime);assert(pm->oldcmd.serverTime<=pm->cmd.serverTime);
 const int ms=pm->cmd.serverTime-ps.commandTime;const float speed=100.0f*pm->cmd.forwardmove/127.0f;
 const float previous=ps.origin[0];ps.origin[0]=(std::min)(wall,previous+speed*ms*.001f);ps.velocity[0]=ps.origin[0]>=wall?0:speed;
 if(previous<stepAt&&ps.origin[0]>=stepAt){ps.origin[2]+=18;pm->viewChange+=18;pm->viewChangeTime=pm->cmd.serverTime;}
 ps.commandTime=pm->cmd.serverTime;std::memcpy(ps.viewangles,pm->cmd.angles,12);ps.weapon=pm->cmd.weapon;
 if(pm->cmd.button_bits.array[0]&1){ps.weaponstate=16;ps.weaponTime=100;++ps.events;}else ps.weaponTime=(std::max)(0,ps.weaponTime-ms);
 ps.fWeaponPosFrac=(std::min)(1.0f,ps.fWeaponPosFrac+(pm->cmd.button_bits.array[0]&2?.01f:-.01f)*ms);
 if(pm->cmd.button_bits.array[0]&4)ps.weaponstate=10;
 ps.bobCycle+=ms;ps.movementDir=pm->cmd.forwardmove?1:0;
 bgs->clientinfo[ps.clientNum].conditions=999;bgs->clientinfo[ps.clientNum].turn=888;bgs->proneTime[ps.clientNum]=777;
}
int Assert_MyHandler(const char *,int,int,const char *,...){return 1;}
'''
code+=function('src/bgame/bg_pmove.cpp','void __cdecl Pmove_1(')
code+=r'''
void Pmove(pmove_t *pm){replayTimes.push_back(pm->cmd.serverTime);Pmove_1(pm);}
void CG_AdjustPositionForMover(int n,const float *in,int ground,int from,int to,float *out,float *angles){assert(n==0);float copy[3];std::memcpy(copy,in,12);std::memcpy(out,copy,12);std::memset(angles,0,12);if(ground==17)out[1]+=(to-from)*.001f;}
'''
code+=re.sub(r'^#include.*\n','',(r/'src/client/splitscreen_prediction.cpp').read_text(),flags=re.M)
code+=r'''
bool near(float a,float b){return std::fabs(a-b)<.001f;}
void cmd(int time,int bits=0,int movement=127,int weapon=1){auto &cl=clients[1];usercmd_s c;c.serverTime=time;c.forwardmove=movement;c.weapon=weapon;c.button_bits.array[0]=bits;c.angles[1]=70;cl.cmds[++cl.cmdNumber&127]=c;cl.serverTime=time;}
void run(int now){cls.realtime=now;const cg_s savedHost=games[0];bgs_t *saved=bgs;replayTimes.clear();CL_SplitGuestUpdatePrediction();assert(bgs==saved&&!CL_SplitGuestPredictionRunning());assert(!std::memcmp(&savedHost,&games[0],sizeof(savedHost)));}
void setup(){liveYaw=70;CL_SplitGuestResetPrediction();clientStore[1]=clientActive_t{};games[0].snap=games[0].nextSnap=&clients[0].snap;flags["splitguest_predict"]=true;flags["cg_nopredict"]=false;flags["cg_synchronousClients"]=false;connected=true;connectionState=CA_ACTIVE;wall=stepAt=1000;cls.serverId=42;}
int main(){
 setup();const playerState_s original=clients[1].snap.ps;cmd(1016,3);run(10);
 const auto *ps=CL_SplitGuestPredictedState();assert(ps&&near(ps->origin[0],1.6f)&&ps->commandTime==1016&&ps->weaponstate==16&&near(ps->fWeaponPosFrac,.16f));
 assert(ps->viewangles[1]==70&&replayTimes==std::vector<int>{1016});assert(!std::memcmp(&original,&clients[1].snap.ps,sizeof(original)));
 assert(near(clients[1].cgameOrigin[0],1.6f)&&clients[1].cgamePredictedDataServerTime==1016&&games[1].predictedPlayerState.events==1);
 float eye[3],angles[3];assert(CL_SplitGuestPredictedCamera(eye,angles)&&near(eye[0],1.6f)&&eye[2]==60&&angles[1]==70);
 liveYaw=75;run(11);assert(CL_SplitGuestPredictedCamera(eye,angles)&&angles[1]==75);liveYaw=70;
 cmd(1032,4,127,2);run(20);ps=CL_SplitGuestPredictedState();assert(ps&&near(ps->origin[0],3.2f)&&ps->weapon==2&&ps->weaponstate==10);
 assert(ps->events==1&&replayTimes==std::vector<int>({1016,1032}));
 // A small authoritative correction preserves continuity, then decays once.
 auto &snap=clients[1].snap;snap.serverTime=1016;snap.ps.commandTime=1016;snap.ps.origin[0]=1.4f;snap.ps.weapon=1;snap.ps.events=1;
 cmd(1048);run(30);assert(near(CL_SplitGuestPredictedState()->origin[0],4.6f));assert(CL_SplitGuestPredictedCamera(eye,angles)&&near(eye[0],4.8f));
 cmd(1064);run(50);assert(CL_SplitGuestPredictedCamera(eye,angles)&&near(eye[0],6.36f));
 run(130);assert(CL_SplitGuestPredictedCamera(eye,angles)&&near(eye[0],6.2f));
 // No stale result across frames, changed snapshots, commands, or server IDs.
 cls.realtime++;assert(!CL_SplitGuestPredictedState());run(132);cls.serverId++;assert(!CL_SplitGuestPredictedState());run(133);
 flags["splitguest_predict"]=false;run(134);assert(!CL_SplitGuestPredictedState()&&near(clients[1].cgameOrigin[0],1.4f));
 flags["splitguest_predict"]=true;run(135);
 for(int mode:{1,4,5,9,10}){snap.ps.pm_type=mode;run(136);assert(!CL_SplitGuestPredictedState());}
 for(int mode:{0,2,3,6,7}){snap.ps.pm_type=mode;run(137);assert(CL_SplitGuestPredictedState());}
 snap.ps.pm_type=0;
 for(int mask:{0x4000,0x100,0x200}){snap.ps.eFlags=mask;run(138);assert(!CL_SplitGuestPredictedState());}snap.ps.eFlags=0;
 snap.ps.otherFlags=2;run(139);assert(!CL_SplitGuestPredictedState());snap.ps.otherFlags=0;
 snap.ps.linkFlags=4;run(140);assert(!CL_SplitGuestPredictedState());snap.ps.linkFlags=0;
 snap.ps.vehiclePos=1;run(141);assert(!CL_SplitGuestPredictedState());snap.ps.vehiclePos=-1;run(142);assert(CL_SplitGuestPredictedState());snap.ps.vehiclePos=0;
 // Respawn/teleport clears correction and private collision/pitch history.
 snap.ps.origin[0]=200;snap.ps.eFlags=2;snap.ps.stats[4]++;run(143);assert(CL_SplitGuestPredictedCamera(eye,angles)&&near(eye[0],204.8f));
 snap.ps.pm_type=6;run(144);assert(CL_SplitGuestPredictedState()->pm_type==6);snap.ps.pm_type=0;
 // Real Pmove_1 subdivision and a deterministic collision fixture: never drift
 // through a wall, including replaying the same unacknowledged commands.
 setup();wall=2;cmd(1100,1);run(10);assert(nativeSubsteps>1&&near(CL_SplitGuestPredictedState()->origin[0],2));run(11);assert(near(CL_SplitGuestPredictedState()->origin[0],2));
 // Stair height is predicted, with only the displayed eye smoothed.
 setup();cmd(1016);run(10);stepAt=2;cmd(1032);run(20);assert(CL_SplitGuestPredictedState()->origin[2]==18);
 assert(CL_SplitGuestPredictedCamera(eye,angles)&&near(eye[2],60));cmd(1132);run(120);assert(CL_SplitGuestPredictedCamera(eye,angles)&&near(eye[2],78));
 // Missing future world snapshot must not disable prediction between arrivals.
 setup();games[0].nextSnap=nullptr;cmd(1016);run(10);
 assert(CL_SplitGuestPredictedState()&&games[0].nextSnap==nullptr);
 // The authoritative snapshot may be in the future of the rendered command clock.
 clients[1].snap.serverTime=1050;cmd(1032);run(26);
 assert(CL_SplitGuestPredictedState()&&near(CL_SplitGuestPredictedState()->origin[0],3.2f)&&games[0].nextSnap==nullptr);
 // Missing history and stale clock never replay an arbitrary gap.
 setup();for(int i=1;i<=130;++i)cmd(1000+i);run(10);assert(!CL_SplitGuestPredictedState()&&replayTimes.empty());
 setup();cmd(1300);run(10);assert(!CL_SplitGuestPredictedState());
 setup();cmd(1016);games[0].bgs.animData=nullptr;run(10);assert(!CL_SplitGuestPredictedState());games[0].bgs.animData=reinterpret_cast<void *>(1);
 // An ordinary C++ unwind restores animation TLS/body metadata as well.
 const cg_s before=games[0];bgs_t *saved=bgs;throwMove=true;bool threw=false;try{run(11);}catch(const std::runtime_error &){threw=true;}throwMove=false;assert(threw&&bgs==saved&&!CL_SplitGuestPredictionRunning()&&!std::memcmp(&before,&games[0],sizeof(before)));
 bool otherThread=false;{SplitGuestPredictionContext context(games[0].bgs,1,1016);std::thread t([&]{otherThread=CL_SplitGuestPredictionRunning();});t.join();assert(!otherThread&&CL_SplitGuestPredictionRunning());}
 CL_SplitGuestResetPrediction();assert(!CL_SplitGuestPredictedState());
 puts("PASS: pending-command replay, native time subdivision, predicted movement/gun/ADS, corrections/steps, collision fixture, resets/fallbacks, authoritative snapshots, host and TLS isolation");
}
'''
env=dict(os.environ);env['PATH']=str(Path(a.compiler).parent)+os.pathsep+env.get('PATH','')
for label,defs in {'pc-mp':['KISAK_MP'],'pc-zombies':['KISAK_SP'],'nx-mp':['KISAK_MP','KISAK_NX'],'nx-zombies':['KISAK_SP','KISAK_NX']}.items():
 cpp=out/(label+'.cpp');exe=out/(label+'.exe');cpp.write_text(code)
 subprocess.run([a.compiler,'-std=c++17','-O0',*[f'-D{x}' for x in defs],str(cpp),'-o',str(exe)],check=True,env=env)
 print(label,flush=True);subprocess.run([str(exe)],check=True,env=env)
source=(r/'src/client/splitscreen.cpp').read_text();frame=function('src/client/splitscreen.cpp','void CL_SplitGuestFrame(')
assert frame.index('CL_SplitGuestBuildInput(')<frame.index('CL_SplitGuestUpdatePrediction()')<frame.index('CL_SendCmd(1)')
assert 'command.serverTime > guest->cmds[guest->cmdNumber & 0x7F].serverTime' in frame
assert 'CL_SplitGuestPredictedCamera' in function('src/client/splitscreen.cpp','void CL_SplitGuestRender(')
assert 'CL_SplitGuestPredictedCamera' in function('src/client/splitscreen.cpp','bool CL_SplitGuestGetWorldCamera(')
weapon=function('src/cgame/cg_weapons.cpp','bool CG_SplitGuestAddViewmodel(')
assert 'CL_SplitGuestPredictedState()' in weapon and 'snapshot.ps = clients[1].snap.ps;' in weapon
assert 'CL_SplitGuestPredictionRunning()' in function('src/bgame/bg_animation.cpp','int __cdecl BG_ExecuteCommand(')
print('PASS: per-frame command ordering, prediction camera/gun consumers and replay-sound guard are wired',flush=True)
# Exercise the actual animation-command sound gate. Replaying a command must
# not emit sounds repeatedly; the authoritative path and other threads retain it.
sound=r'''
#include <cassert>
#include <cstdio>
#include <thread>
#ifndef __cdecl
#define __cdecl
#endif
using animBodyPart_t=int;
struct playerState_s {int clientNum=1;};
struct animScriptCommand_t {int bodyPart[2]{},animIndex[2]{},animDuration[2]{};const char *soundAlias="test";};
int sounds=0;void play(int client,const char *){assert(client==1);++sounds;}
struct {struct {struct {int flags=0,duration=0;}animations[2];void (*playSoundAlias)(int,const char *)=play;}animScriptData;} data;
struct {decltype(data) *animData=&data;} storage;auto bgs=&storage;
thread_local bool predicting=false;
bool CL_SplitGuestPredictionRunning(){return predicting;}
bool Demo_IsPlaying(){return false;}
int BG_PlayAnim(playerState_s *,int,animBodyPart_t,int,int,int,int){return 1;}
'''+function('src/bgame/bg_animation.cpp','int __cdecl BG_ExecuteCommand(')+r'''
int main(){playerState_s ps;animScriptCommand_t cmd;
 BG_ExecuteCommand(&ps,&cmd,0,1,0);assert(sounds==1);
 predicting=true;BG_ExecuteCommand(&ps,&cmd,0,1,0);assert(sounds==1);
 std::thread t([&]{BG_ExecuteCommand(&ps,&cmd,0,1,0);});t.join();assert(sounds==2);
 predicting=false;BG_ExecuteCommand(&ps,&cmd,0,1,0);assert(sounds==3);
 puts("PASS: native animation sounds suppressed only for guest replay on its thread; authoritative sound path retained");}
'''
for mode in ['KISAK_MP','KISAK_SP']:
 cpp=out/(mode+'-sounds.cpp');exe=cpp.with_suffix('.exe');cpp.write_text(sound)
 subprocess.run([a.compiler,'-std=c++17','-O0','-DKISAK_EXPERIMENTAL_SPLITSCREEN',f'-D{mode}',str(cpp),'-o',str(exe)],check=True,env=env)
 subprocess.run([str(exe)],check=True,env=env)
