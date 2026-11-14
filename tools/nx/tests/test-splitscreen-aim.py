"""Exercise native assist with shared settings, separate guest targets, actual visibility and input handoff."""
from pathlib import Path
import argparse,re,subprocess
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--compiler',default='/c/msys64/ucrt64/bin/g++.exe');args=p.parse_args()
root=Path(__file__).resolve().parents[3];out=root/'build-nx/splitscreen-aim-tests';out.mkdir(parents=True,exist_ok=True)
def read(n):return (root/n).read_text()
def clean(s):
 s=re.sub(r'^\s*#(?:include|pragma).*\n','',s,flags=re.M)
 return re.sub(r'__declspec\(align\(\d+\)\)','',s).replace('__int8','char').replace('__int16','short').replace('__int32','int')
def function(n,sig):
 s=read(n);a=s.index(sig);b=s.index('{',a)+1;d=1
 while d:d+=(s[b]=='{')-(s[b]=='}');b+=1
 return clean(s[a:b])
code=r"""
#include <cassert>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <map>
#include <string>
#include <algorithm>
#define __cdecl
#define MAX_LOCAL_CLIENTS 2
#define KISAK_EXPERIMENTAL_SPLITSCREEN 1
#define KISAK_SP 1
#define __debugbreak() assert(false)
#define iassert(x) assert(x)
#define PROF_SCOPED(x)
#define LODWORD(x) (*reinterpret_cast<int*>(&x))
using _QWORD=unsigned long long;
template<int N>struct bitarray{unsigned array[2]{};bool testBit(unsigned b)const{return (array[b/32]&(0x80000000u>>(b%32)))!=0;}void setBit(unsigned b){array[b/32]|=0x80000000u>>(b%32);}};
struct dvar_s{struct{float value=0;int integer=0;bool enabled=false;}current;};
std::map<std::string,dvar_s>vars;
const dvar_s*_Dvar_RegisterBool(const char*n,int v,unsigned,const char*){auto&d=vars[n];d.current.enabled=v;return &d;}
const dvar_s*_Dvar_RegisterFloat(const char*n,float v,float,float,unsigned,const char*){auto&d=vars[n];d.current.value=v;d.current.integer=(int)v;return &d;}
const dvar_s*_Dvar_RegisterInt(const char*n,int v,int,int,unsigned,const char*){auto&d=vars[n];d.current.integer=v;return &d;}
float Dvar_GetFloat(const char*){return 1;}template<class...T>bool Assert_MyHandler(T...){return false;}
template<class...T>void Com_Printf(T...){}
const char*va(const char*f,...){return f;}
float ScrPlace_HiResGetScale(){return 1;}
struct{float realViewportSize[2]{1280,360},scaleVirtualToReal[2]{2,0.75f};}scrPlaceView[2];
bool Demo_IsPlaying(){return false;}
float AngleNormalize180(float x){x=fmod(x+180,360);if(x<0)x+=360;return x-180;}
float DiffTrackAngle(float to,float from,float speed,float dt){return from+AngleNormalize180(to-from)*std::min(speed*dt,1.f);}
float LinearTrack(float to,float from,float speed,float dt){return from+std::clamp(to-from,-speed*dt,speed*dt);}
float vectoyaw(const float*v){return atan2(v[1],v[0])*180/3.14159265f;}
void vectoangles(const float*v,float*a){a[0]=-atan2(v[2],sqrt(v[0]*v[0]+v[1]*v[1]))*180/3.14159265f;a[1]=vectoyaw(v);a[2]=0;}
void AnglesToAxis(const float*a,float m[3][3]){float y=a[1]*3.14159265f/180,p=a[0]*3.14159265f/180;m[0][0]=cos(y)*cos(p);m[0][1]=sin(y)*cos(p);m[0][2]=-sin(p);m[1][0]=-sin(y);m[1][1]=cos(y);m[1][2]=0;m[2][0]=cos(y)*sin(p);m[2][1]=sin(y)*sin(p);m[2][2]=cos(p);}
float Vec3DistanceSq(const float*a,const float*b){float n=0;for(int i=0;i<3;++i)n+=(a[i]-b[i])*(a[i]-b[i]);return n;}
void Vec3Lerp(const float*a,const float*b,float t,float*c){for(int i=0;i<3;++i)c[i]=a[i]+t*(b[i]-a[i]);}
struct playerState_s{int weapon=1,clientNum=1,pm_type=0,weaponstate=0,viewlocked_entNum=1023,throwBackGrenadeTimeLeft=0;unsigned otherFlags=0,eFlags=0,linkFlags=0,pm_flags=0,weapFlags=0;float origin[3]{5000,0,0},viewangles[3]{0,90,0},delta_angles[3]{5,30,0},velocity[3]{},fWeaponPosFrac=0,viewHeightCurrent=60;};
struct trajectory_t{float trBase[3]{},trDelta[3]{};};
void BG_EvaluateTrajectory(const trajectory_t*t,int time,float*out){for(int i=0;i<3;++i)out[i]=t->trBase[i]+t->trDelta[i]*time*.001f;}
struct centity_s{int clientFlags=2;struct{int number=0,eType=17,clientNum=0,solid=0,weapon=1;struct{int brushmodel=0;}index;struct{unsigned eFlags=0x800,eFlags2=0;struct{struct{int actorNum=0,team=2;}actor;}u;trajectory_t pos;}lerp;}nextState;struct{trajectory_t pos;}currentState;struct{int localClientNum=0,eType=17;float origin[3]{},angles[3]{};}pose;};
struct snapshot_s{int serverTime=1000,numEntities=0;playerState_s ps;struct{int number=0;}entities[64];};
using team_t=int;struct clientInfo_t{int team=1;bool infoValid=true;char model[4]{120,0};};
struct cg_s{int localClientNum=0,clientNum=0,time=1000,cameraMode=0,useAlternateAimParams=0;playerState_s predictedPlayerState;snapshot_s *snap=nullptr,*nextSnap=nullptr;struct{float vieworg[3]{},viewaxis[3][3]{},tanHalfFovX=1,tanHalfFovY=.75f;}refdef;struct{clientInfo_t clientinfo[32];struct{int team=2;}actorinfo[64];}bgs;}games[2];
cg_s*CG_GetLocalClientGlobals(int n){assert(n<2);return &games[n];}
centity_s entities[64];centity_s*CG_GetEntity(int n,int e){assert(n==0&&e>=0&&e<64);return &entities[e];}
struct{struct{int serverTime=1000;}snap;}clients[2];
struct usercmd_s{int forwardmove=0,rightmove=0;float meleeChargeYaw=0;unsigned char meleeChargeDist=0;bitarray<51>button_bits;};
bool guestView=true;bool CL_IsSplitGuest(int n){return n==1;}bool CL_SplitGuestHasView(){return guestView;}
constexpr int ET_ACTOR=17,MISSILE_GUIDANCE_TVGUIDED=1,IMPACT_TYPE_BLADE=1;
struct WeaponDef{bool bDualWield=false,requireLockonToFire=false;int guidedMissileType=0,overlayReticle=0,impactType=0;float aimAssistRange=1500,autoAimRange=1500,meleeChargeRange=64;}weap;
struct WeaponVariantDef{WeaponDef*weapDef=&weap;float fAimAssistRangeAds=1500;}variant;
const WeaponDef*BG_GetWeaponDef(unsigned){return &weap;}const WeaponVariantDef*BG_GetWeaponVariantDef(unsigned){return &variant;}
int BG_GetNumWeapons(){return 2;}int BG_GetViewmodelWeaponIndex(const playerState_s*ps){return ps->weapon;}int CG_GetPlayerMeleeWeapon(const playerState_s*){return 1;}int BG_WeaponAmmo(const playerState_s*,unsigned){return 8;}bool BG_IsAimDownSightWeapon(unsigned){return true;}bool BG_IsBayonetWeapon(unsigned){return false;}
struct GraphFloat{}aaInputGraph[4];float GraphFloat_GetValue(GraphFloat*,float f){return f;}
dvar_s zombie,fov,topDown,gunOffset;auto zombiemode=&zombie;auto cg_fov=&fov;auto player_topDownCamMode=&topDown;auto bg_gunXOffset=&gunOffset;
"""
code+=clean(read('src/aim_assist/aim_assist.h'))+'\n'+clean(read('src/aim_assist/aim_target.h'))+'\n'
code+='AimAssistGlobals aaGlobArray[MAX_LOCAL_CLIENTS];AimTargetGlob atGlobArray[MAX_LOCAL_CLIENTS];\n'
s=read('src/aim_assist/aim_assist.cpp');a=s.index('const dvar_s *aim_aimAssistRangeScale;');b=s.index('void __cdecl AimAssist_SetADSWidthAndLerp',a);code+='\n'.join(re.findall(r'const dvar_s \*\w+;',s[a:b]))+'\n'
s=read('src/aim_assist/aim_target.cpp');a=s.index('const dvar_s *aim_target_sentient_radius');b=s.index('AimTargetGlob *__cdecl',a);code+=s[a:b]+'\n'
code+=r"""
void AimAssist_CreateScreenMatrix(AimAssistGlobals*,float,float){}
void AimTarget_GetTargetBounds(const centity_s*,float*mi,float*ma){mi[0]=mi[1]=-10;mi[2]=0;ma[0]=ma[1]=10;ma[2]=72;}
char AimAssist_ConvertToClipBounds(const AimAssistGlobals*aa,const float(*bounds)[3],const float(*m)[3],float(*clip)[3]){
 float delta[3]{m[3][0]-aa->viewOrigin[0],m[3][1]-aa->viewOrigin[1],m[3][2]+36-aa->viewOrigin[2]};float depth=0,x=0,y=0;
 for(int i=0;i<3;++i){depth+=delta[i]*aa->viewAxis[0][i];x+=delta[i]*aa->viewAxis[1][i];y+=delta[i]*aa->viewAxis[2][i];}
 if(depth<=0)return 0;x/=depth;y/=depth*.75f;if(fabs(x)>1.1f||fabs(y)>1.5f)return 0;
 clip[0][0]=x-.1f;clip[1][0]=x+.1f;clip[0][1]=y-.5f;clip[1][1]=y+.5f;return 1;
}
bool CG_PlayerInViewFrustum(int,const centity_s*){return false;}bool R_CullPoint(int,const float*,float){return true;}
float RadiusFromBounds(const float*,const float*){return 40;}int Com_GetClientDObj(int,int){return 1;}
int AimTarget_GetTagPos(int n,const centity_s*e,unsigned,float*p){assert(n==0);memcpy(p,e->pose.origin,12);p[2]+=60;return 1;}
void AimTarget_GetTagPos_0(const centity_s*e,unsigned,float*p){memcpy(p,e->pose.origin,12);p[2]+=60;}
struct{unsigned aim_vis_bone=1,aim_bone=2;}scr_const;
struct trace_t{float fraction=1;int hit=1023;};struct col_context_t{};float vec3_origin[3]{};
bool wall=false,smoke=false;int visibilityChecks=0;
void CG_TraceCapsule(trace_t*t,const float*start,const float*,const float*,const float*,unsigned char ignore,int,col_context_t*){assert(start[0]==5000&&ignore==1);++visibilityChecks;t->fraction=wall?.3f:1;}
int Trace_GetEntityHitId(const trace_t*t){return t->hit;}
bool CM_TransformedPointContents(const float*,int,const float*,const float*){return false;}
bool FX_ClientVisibilityTest(int n,const float*start,const float*){assert(n==0&&start[0]==5000);return !smoke;}
"""
for sig in ['void AimAssist_RegisterDvars(', 'void __cdecl AimAssist_Setup(', 'void __cdecl AimAssist_BackupPlayerState(', 'int AimAssist_EntityClientNum(', 'char __cdecl AimAssist_PlayerDisabledAutoAim(', 'void __cdecl AimAssist_UpdateScreenTargets(', 'void __cdecl AimAssist_FovScale(', 'double __cdecl AimAssist_GetCrosshairDistSqr(', 'void __cdecl AimAssist_AddToTargetList(', 'int __cdecl AimAssist_CompareTargets(', 'int __cdecl AimAssist_CalcAimPos(', 'void __cdecl AimAssist_UpdateGamePadInput(', 'void __cdecl AimAssist_UpdateTweakables(', 'void __cdecl AimAssist_UpdateAdsLerp(', 'void __cdecl AimAssist_ApplyTurnRates(', 'double __cdecl AimAssist_LerpDvars(', 'void __cdecl AimAssist_CalcAdjustedAxis(', 'void __cdecl AimAssist_CalcSlowdown(', 'double __cdecl AimAssist_GetAimAssistRange(', 'static bool __cdecl AimAssist_DoBoundsIntersectCenterBox(', 'const AimScreenTarget *__cdecl AimAssist_GetBestTarget(', 'bool __cdecl AimAssist_IsPlayerUsingOffhand(', 'bool __cdecl AimAssist_IsSlowdownActive(']:
 code+=function('src/aim_assist/aim_assist.cpp',sig)+'\n'
s=read('src/aim_assist/aim_assist.cpp');a=s.index('double __cdecl AimAssist_GetProfileSensitivity(');b=s.index('void __cdecl AimAssist_DrawDebugOverlay(',a);code+=clean(s[a:b])+'\n'
# Native SP trace implementation occurs first.
for sig in ['void AimTarget_RegisterDvars(', 'AimTargetGlob *__cdecl AimTarget_GetGlobArray(', 'void __cdecl AimTarget_ProcessEntityInternal(', 'bool __cdecl AimTarget_IsTargetValid(', 'double __cdecl AimTarget_GetTargetRadius(', 'void __cdecl AimTarget_GetTargetTop(', 'bool __cdecl AimTarget_IsTargetVisible(', 'void __cdecl AimTarget_GetTargetCenter(', 'void __cdecl AimTarget_CreateTarget(', 'void __cdecl AimTarget_AddTargetToList(', 'int __cdecl AimTarget_CompareTargets(', 'bool __cdecl AimTarget_PlayerInValidState(', 'void __cdecl AimTarget_UpdateClientTargets(', 'void __cdecl AimTarget_ClearTargetList(', 'void __cdecl AimTarget_ProcessEntity(', 'void __cdecl AimTarget_GetClientTargetList(']:
 code+=function('src/aim_assist/aim_target.cpp',sig)+'\n'
code+=clean(read('src/aim_assist/aim_splitscreen.cpp'))+'\n'
code+=r"""
int main(){
 AimAssist_RegisterDvars();AimTarget_RegisterDvars();zombie.current.enabled=true;fov.current.value=65;
 vars["aim_input_graph_enabled"].current.enabled=false;vars["aim_accel_turnrate_enabled"].current.enabled=false;vars["aim_scale_view_axis"].current.enabled=false;
 vars["aim_automelee_enabled"].current.enabled=false;
 assert(vars["nx_aimassist"].current.enabled&&!AimAssist_PlayerDisabledAutoAim());
 snapshot_s snap,next;snap.serverTime=950;next.serverTime=1000;next.numEntities=4;next.entities[0].number=0;next.entities[1].number=1;next.entities[2].number=10;next.entities[3].number=11;
 games[0].snap=&snap;games[0].nextSnap=&next;games[0].predictedPlayerState.clientNum=0;games[0].useAlternateAimParams=1;aaGlobArray[0].initialized=true;
 aaGlobArray[0].screenTargetCount=1;aaGlobArray[0].screenTargets[0].entIndex=10;aaGlobArray[0].lockOnTargetEnt=10;aaGlobArray[0].autoAimTargetEnt=10;
 for(int i:{0,1,10,11}){entities[i].nextState.number=i;entities[i].nextState.lerp.u.actor.actorNum=i;}
 entities[0].nextState.eType=entities[1].nextState.eType=1;entities[0].nextState.clientNum=0;entities[1].nextState.clientNum=1;
 entities[10].pose.origin[0]=100;entities[10].pose.origin[1]=3;entities[11].pose.origin[0]=5003;entities[11].pose.origin[1]=100;
 for(int i:{10,11}){memcpy(entities[i].nextState.lerp.pos.trBase,entities[i].pose.origin,12);memcpy(entities[i].currentState.pos.trBase,entities[i].pose.origin,12);}
 playerState_s player;usercmd_s command;float raw[3]{-5,60,0};
 assert(AimAssist_UpdateSplitGuestInput(&player,&command,.016f,0,.5f,raw,true));
 assert(aaGlobArray[1].aimSlowdownTargetEnt==11&&aaGlobArray[1].screenTargetCount==1&&visibilityChecks>0);
 assert(aaGlobArray[1].screenTargets[0].distSqr<11000&&aaGlobArray[0].lockOnTargetEnt==10&&aaGlobArray[0].autoAimTargetEnt==10);
 float slowed=AngleNormalize180(raw[1]-60);vars["nx_aimassist"].current.enabled=false;raw[0]=-5;raw[1]=60;
 assert(AimAssist_UpdateSplitGuestInput(&player,&command,.016f,0,.5f,raw,true));assert(raw[1]-60>slowed&&aaGlobArray[1].ps.targetAssistDisabled);
 assert(aaGlobArray[1].aimSlowdownTargetEnt==1023&&aaGlobArray[1].lockOnTargetEnt==1023);
 command.button_bits.setBit(11);raw[0]=-5;raw[1]=60;
 AimAssist_UpdateSplitGuestInput(&player,&command,.016f,0,0,raw,true);
 assert(raw[0]==-5&&raw[1]==60&&!aaGlobArray[1].autoAimActive); // alternate ZM params must honor master off
 vars["nx_aimassist"].current.enabled=true;raw[0]=-5;raw[1]=60;
 AimAssist_UpdateSplitGuestInput(&player,&command,.016f,0,0,raw,true);assert(aaGlobArray[1].autoAimActive&&aaGlobArray[1].autoAimTargetEnt==11&&raw[1]<60);
 command.button_bits={};wall=true;raw[0]=-5;raw[1]=60;AimAssist_UpdateSplitGuestInput(&player,&command,.016f,0,.5f,raw,true);assert(aaGlobArray[1].screenTargetCount==0);
 wall=false;smoke=true;AimAssist_UpdateSplitGuestInput(&player,&command,.016f,0,.5f,raw,true);assert(aaGlobArray[1].screenTargetCount==0);smoke=false;
 // Lock-on tracks the guest's moving target from its own position.
 entities[11].nextState.lerp.pos.trDelta[0]=20;entities[11].currentState.pos.trDelta[0]=20;raw[0]=-5;raw[1]=60;
 AimAssist_UpdateSplitGuestInput(&player,&command,.016f,0,.5f,raw,true);assert(aaGlobArray[1].lockOnTargetEnt==11);
 auto host=aaGlobArray[0];AimAssist_ResetSplitGuest();assert(!aaGlobArray[1].initialized&&atGlobArray[1].targetCount==0&&!memcmp(&host,&aaGlobArray[0],sizeof(host)));
 guestView=false;assert(!AimAssist_UpdateSplitGuestInput(&player,&command,.016f,0,0,raw,true));
 // Turn acceleration: on by default (the original ramp); off turns at the full rate at once.
 vars["aim_slowdown_enabled"].current.enabled=false;vars["aim_accel_turnrate_enabled"].current.enabled=true;
 assert(vars["nx_aimacceleration"].current.enabled);
 auto turn=[&](bool acceleration,int frames){
  vars["nx_aimacceleration"].current.enabled=acceleration;memset(&aaGlobArray[1],0,sizeof(aaGlobArray[1]));aaGlobArray[1].fovTurnRateScale=1;
  AimInput in{};in.localClientNum=1;in.deltaTime=.016f;in.yawAxis=1;AimOutput out{};
  for(int i=0;i<frames;++i)AimAssist_ApplyTurnRates(&in,&out);
  return out.yaw;};
 const float rampedFirst=turn(true,1),directFirst=turn(false,1);
 assert(rampedFirst>0&&directFirst>rampedFirst*2);         // full stick: the ramp starts slow, direct is at full rate
 assert(fabs(turn(false,10)-directFirst*10)<1e-3f);         // direct: the same turn every frame
 assert(turn(true,200)>turn(true,1)*200);                   // ramped: speeds up while the stick is held
 vars["aim_accel_turnrate_enabled"].current.enabled=false;assert(fabs(turn(true,1)-directFirst)<1e-4f); // dev dvar still disables the ramp
 vars["aim_accel_turnrate_enabled"].current.enabled=true;vars["nx_aimacceleration"].current.enabled=true;
 puts("PASS: native P2 slowdown/ADS snap/lock-on, shared live toggle, private distant targets, walls/smoke, cleanup and turn acceleration setting");
}
"""
for mode in ['sp','mp']:
 cpp=out/f'aim-{mode}.cpp';cpp.write_text(code if mode=='sp' else code.replace('#define KISAK_SP 1\n',''));exe=cpp.with_suffix('.exe')
 subprocess.run([args.compiler,'-std=c++17','-w',str(cpp),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
