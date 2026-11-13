"""Exercise guest overhead-name selection, shared model/identity and private camera/fade state."""
from pathlib import Path
import argparse,subprocess
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--compiler',default='/c/msys64/ucrt64/bin/g++.exe');args=p.parse_args()
root=Path(__file__).resolve().parents[3];out=root/'build-nx/splitscreen-name-tests';out.mkdir(parents=True,exist_ok=True)
s=(root/'src/cgame/cg_draw_names.cpp').read_text()
def function(sig):
 a=s.index(sig);b=s.index('{',a)+1;depth=1
 while depth:depth+=(s[b]=='{')-(s[b]=='}');b+=1
 return s[a:b]
code=r"""
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#define KISAK_SP 1
#define KISAK_EXPERIMENTAL_SPLITSCREEN 1
#ifndef __cdecl
#define __cdecl
#endif
constexpr int ET_PLAYER=1,TEAM_SPECTATOR=3;
struct OverheadFade{int nextCheckTime=0,lastTime=0,startTime=0;bool visible=false;};
struct PlayerDetails{char gamerTag[32]{},fullDisplayName[38]{},clanDisplayName[7]{};int gamerTagDisplayLength=0,fullDisplayNameLength=0,clanDisplayLength=0;bool inView=false;}playerDetails[2][32];
struct centity_s{unsigned clientFlags=2;struct{int number=0,clientNum=0,eType=ET_PLAYER;struct{unsigned eFlags=0;}lerp;}nextState;struct{float origin[3]{100,0,0};}pose;struct Link{int linkEnt=0;}*linkInfo=nullptr;};
struct snapshot_s{struct{int clientNum=1;}ps;int numClients=2;struct{int clientIndex=0;}clients[32];};
struct refdef_s{float vieworg[3]{},viewaxis[3][3]{{1,0,0},{0,1,0},{0,0,1}},tanHalfFovX=1,tanHalfFovY=1;};
struct ScreenPlacement{float realViewportSize[2]{1280,360},realViewportBase[2]{0,0};}scrPlaceView[2];
struct cg_s{snapshot_s*nextSnap=nullptr;struct{struct{int team=2;bool infoValid=true;}clientinfo[32];}bgs;struct{int clientNum=0;}predictedPlayerState;centity_s predictedPlayerEntity;int time=100;refdef_s refdef;}games[2];
cg_s *CG_GetLocalClientGlobals(int n){return &games[n];}bool CL_IsSplitGuest(int n){return n==1;}
centity_s *CG_GetEntity(int n,int i){assert(n==0&&i==0);return &games[0].predictedPlayerEntity;}
struct Dvar{struct{bool enabled=false;float value=10000;int integer=100;}current;}enabled,walls,range,fadeIn,fadeOut;
auto cg_drawFriendlyNames=&enabled;auto cg_drawThroughWalls=&walls;auto cg_overheadNamesMaxDist=&range;auto cg_friendlyNameFadeIn=&fadeIn;auto cg_friendlyNameFadeOut=&fadeOut;
bool blocked=false,flash=false;bool CG_Flashbanged(int n){assert(n==1);return flash;}bool CG_Flared(int){return false;}
struct DObj{}obj;DObj *Com_GetClientDObj(int entity,int n){assert(n==0&&entity==0);return &obj;}
struct{int j_head=1;}scr_const;
bool CG_DObjGetWorldTagPos(const decltype(centity_s::pose)*pose,DObj*,int,float *out){memcpy(out,pose->origin,12);out[2]+=50;return true;}
struct trace_t{float fraction=1;unsigned short hit=0;};
void CG_LocationalTrace(trace_t*t,const float*,const float*,int skip,int,int,void*){assert(skip==1);t->hit=0;t->fraction=1;}
unsigned short Trace_GetEntityHitId(const trace_t*t){return t->hit;}
bool FX_ClientVisibilityTest(int n,const float *origin,const float*){assert(n==0&&origin==games[1].refdef.vieworg);return !blocked;}
bool CL_GetClientName(int n,int client,char*out,int size,bool full){assert(n==0&&client==0);snprintf(out,size,"%s",full?"host-full":"host-name");return true;}
bool CL_GetClientClanName(int n,int client,char*out,int){assert(n==0&&client==0);strcpy(out,"TAG");return true;}
void Com_PrintError(int,const char*,...){assert(false);}
void CG_GetViewAxisProjections(const refdef_s *r,const float *p,float *out){for(int i=0;i<3;++i){out[i]=0;for(int j=0;j<3;++j)out[i]+=(p[j]-r->vieworg[j])*r->viewaxis[i][j];}}
"""
for sig in ['static float CG_NameDot_SP(', 'static int CG_NameTimeDifference_SP(', 'static int CG_NameTimeAdd_SP(', 'static double CG_NameFade_SP(', 'static bool CG_NameTraceIsEntity_SP(', 'static bool CG_CanSeeNameHead_SP(', 'void __cdecl CG_InitPlayerDetails(', 'char __cdecl CG_CalcNamePosition(']:
 code+=function(sig)+'\n'
code+=r"""
std::vector<int> submitted;
void CG_DrawOverheadNames(int n,const centity_s *cent,float alpha,int mode){assert(n==1&&mode==0);if(alpha<=.001f)return;CG_InitPlayerDetails(n,cent->nextState.number);assert(!strcmp(playerDetails[1][0].gamerTag,"host-name"));float point[3]{100,0,50},x,y;assert(CG_CalcNamePosition(n,point,&x,&y)&&y>=360&&y<720);submitted.push_back(cent->nextState.number);}
"""
a=s.index('static OverheadFade s_guestNameFade[32];');b=s.index('\n#endif',a);code+=s[a:b]+'\n'
code+=r"""
int main(){snapshot_s host,guest;host.clients[0].clientIndex=0;host.clients[1].clientIndex=1;guest.numClients=0;guest.ps.clientNum=1;games[0].nextSnap=&host;games[1].nextSnap=&guest;games[1].bgs.clientinfo[0].infoValid=false;scrPlaceView[1].realViewportBase[1]=360;enabled.current.enabled=true;
 const auto world=games[0];CG_SplitGuestDrawPlayerNames_SP();assert(submitted.empty());games[1].time+=200;CG_SplitGuestDrawPlayerNames_SP();assert(submitted.size()==1&&submitted[0]==0);
 assert(!memcmp(&world,&games[0],sizeof(world)));assert(playerDetails[0][0].gamerTag[0]==0);
 blocked=true;games[1].time+=1000;submitted.clear();CG_SplitGuestDrawPlayerNames_SP();assert(submitted.empty());
 blocked=false;CG_SplitGuestResetNames();games[1].time+=200;CG_SplitGuestDrawPlayerNames_SP();games[1].time+=200;submitted.clear();CG_SplitGuestDrawPlayerNames_SP();assert(submitted.size()==1);
 games[0].predictedPlayerEntity.nextState.lerp.eFlags=0x20;submitted.clear();CG_SplitGuestDrawPlayerNames_SP();assert(submitted.empty());
 games[0].predictedPlayerEntity.nextState.lerp.eFlags=0;games[0].bgs.clientinfo[0].team=1;CG_SplitGuestDrawPlayerNames_SP();assert(submitted.empty());
 CG_SplitGuestResetNames();assert(!s_guestNameFade[0].visible&&!playerDetails[1][0].gamerTag[0]);puts("PASS: guest host nametag, private fades/details, host model/FX/identity lookup, guest viewport projection, visibility and team/death gates");}
"""
cpp=out/'names.cpp';exe=out/'names.exe';cpp.write_text(code)
subprocess.run([args.compiler,'-std=c++17','-O0',str(cpp),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
