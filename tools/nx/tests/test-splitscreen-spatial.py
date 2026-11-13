"""Exercise actual shared model LOD, two-listener distance/priority and listener lifetime."""
from pathlib import Path
import argparse, subprocess
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--compiler',default='/c/msys64/ucrt64/bin/g++.exe');args=p.parse_args()
root=Path(__file__).resolve().parents[3];out=root/'build-nx/splitscreen-spatial-tests';out.mkdir(parents=True,exist_ok=True)
def function(n,sig):
 s=(root/n).read_text();a=s.index(sig);b=s.index('{',a)+1;depth=1
 while depth:depth+=(s[b]=='{')-(s[b]=='}');b+=1
 return s[a:b]
code=r"""
#include <cassert>
#include <cmath>
#include <cfloat>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <cstdarg>
#define __cdecl
#define __debugbreak() assert(false)
#define iassert(x) assert(x)
#define bcassert(x,n) assert(unsigned(x)<unsigned(n))
#define nanassertvec3(v) assert(std::isfinite((v)[0])&&std::isfinite((v)[1])&&std::isfinite((v)[2]))
#define IS_NAN(x) (!std::isfinite(x))
#define LODWORD(x) (*reinterpret_cast<unsigned*>(&x))
#define SND_EPSILON .0000152879f
constexpr int SND_MAX_VOICES=4;using team_t=int;constexpr int TEAM_FREE=0;
template<class...T>bool Assert_MyHandler(T...){return false;}
const char*va(const char*f,...){return f;}
float Vec3DistanceSq(const float*a,const float*b){float d=0;for(int i=0;i<3;++i)d+=(a[i]-b[i])*(a[i]-b[i]);return d;}
float Vec3Distance(const float*a,const float*b){return std::sqrt(Vec3DistanceSq(a,b));}
void Vec3Copy(const float*a,float*b){std::memcpy(b,a,12);}void Vec3Clear(float*a){std::memset(a,0,12);}
float Vec3LengthSq(const float*a){return a[0]*a[0]+a[1]*a[1]+a[2]*a[2];}
float Vec3Normalize(float*a){float n=std::sqrt(Vec3LengthSq(a));if(n)for(int i=0;i<3;++i)a[i]/=n;return n;}
void AxisClear(float a[3][3]){std::memset(a,0,36);for(int i=0;i<3;++i)a[i][i]=1;}
void AxisCopy(const float a[3][3],float b[3][3]){std::memcpy(b,a,36);}
struct snd_alias_t{unsigned flags=2;int distReverbMax=300;};
struct snd_listener{struct{float origin[3]{},axis[3][3]{};}orient;int clientNum=0;bool active=false;team_t team=0;};
struct{snd_listener listeners[MAX_LOCAL_CLIENTS];unsigned voiceAliasHash[SND_MAX_VOICES]{};struct{snd_alias_t *alias=nullptr;}voice[SND_MAX_VOICES];}g_snd;
int stops=0;void SND_StopVoice(int n){++stops;g_snd.voiceAliasHash[n]=0;}
"""
for sig in ['unsigned int __cdecl SND_ActiveListenerCount(', 'int __cdecl SND_GetListenerIndexNearestToOrigin(', 'void __cdecl SND_GetNearestListenerPosition(', 'double __cdecl Snd_GetGlobalPriorityVolume(']:
 code+=function('src/sound/snd.cpp',sig)+'\n'
for sig in ['void __cdecl SNDL_DisconnectListener(', 'void __cdecl SNDL_SetListener(']:code+=function('src/sound/snd_local.cpp',sig)+'\n'
code+=r"""
struct playerState_s{int clientNum=1;float origin[3]{3000,0,0},viewHeightCurrent=60,viewangles[3]{0,90,0};};
struct{struct{playerState_s ps;}snap;}clients[2];
struct cg_s{struct{struct{team_t team=1;}clientinfo[32];}bgs;}world;
auto CG_GetLocalClientGlobals(int){return &world;}
bool guestVisible=true,liveLook=true;bool CL_SplitGuestHasView(){return guestVisible;}
bool CL_SplitGuestGetViewAngles(float*a){a[0]=0;a[1]=180;a[2]=0;return liveLook;}
bool usePrediction=false;
bool CL_SplitGuestPredictedCamera(float *o,float *a){if(!usePrediction)return false;o[0]=3100;o[1]=0;o[2]=65;a[0]=0;a[1]=180;a[2]=0;return true;}
void AnglesToAxis(const float*a,float axis[3][3]){float y=a[1]*3.14159265f/180;AxisClear(axis);axis[0][0]=cos(y);axis[0][1]=sin(y);axis[1][0]=-sin(y);axis[1][1]=cos(y);}
void SND_SetListener(unsigned n,int client,team_t team,const float*p,const float(*a)[3]){SNDL_SetListener(n,client,team,p,a);}
void SND_DisconnectListener(unsigned n){SNDL_DisconnectListener(n);}
"""
for sig in ['bool CL_SplitGuestGetWorldCamera(', 'void CL_SplitGuestUpdateSound(']:code+=function('src/client/splitscreen.cpp',sig)+'\n'
code+=r"""
struct{struct{float value=1;bool enabled=false;}current;bool modified=false;}lock,offscreenScale,warm;
auto r_lockPvs=&lock;auto r_offscreenCasterLodScale=&offscreenScale;auto r_warm_dobj=&warm;
struct GfxLodParms{float origin[3]{},extraCamOrigin[3]{};bool extraCamOriginValid=false,valid=true;struct{float scale=1,bias=0;}ramp[2];};
struct{GfxLodParms lodParms;}rg;
struct refdef_s{float vieworg[3]{},extraCamPos[3]{};bool extraCamPosValid=false;int time=1000,localClientNum=0;};
struct{struct{float viewOffset[3]{},floatTime=0;int time=0;}def;}scene;
struct{}lockPvsViewParms;void Dvar_ClearModified(decltype(r_lockPvs)){}
void R_SetViewParmsForScene(const refdef_s*,decltype(lockPvsViewParms)*){}
void R_UpdateLodParms(const refdef_s*r,GfxLodParms*p){Vec3Copy(r->vieworg,p->origin);Vec3Copy(r->extraCamPos,p->extraCamOrigin);p->extraCamOriginValid=r->extraCamPosValid;}
void R_UpdateFrameFog(int){}void R_UpdateFrameSun(){}
using XModelLodRampType=int;struct XModelLodInfo{float dist=0;};
struct XModel{const char*name="test";int numLods=3;XModelLodInfo lodInfo[3]{{100},{500},{2000}};};
struct DObj{int numModels=1;XModel *localModels[1];};
int DObjGetNumModels(const DObj*d){return d->numModels;}XModel *DObjGetModel(const DObj*d,int n){return d->localModels[n];}
float I_fres(float x){return 1/x;}int XModelGetNumLods(const XModel*m){return m->numLods;}
XModelLodRampType XModelGetLodRampType(const XModel*){return 1;}
struct{bool enabled=false;float dist=0;}g_testLods[4];
"""
code+=function('src/gfx_d3d/r_scene.cpp','void __cdecl R_SetLodOrigin(')+'\n'
code+='double __cdecl'+(root/'src/gfx_d3d/r_model_lod.cpp').read_text().split('double __cdecl',1)[1]+'\n'
for sig in ['double __cdecl XModelLodInfo_GetDist(', 'int __cdecl XModelGetLodForDist_Internal(', 'int __cdecl XModelGetLodForDist(']:code+=function('src/xanim/xmodel_utils.cpp',sig)+'\n'
for sig in ['int __cdecl DObjGetLodForDist(', 'void __cdecl DObjGetSurfaceData(']:code+=function('src/xanim/dobj_utils.cpp',sig)+'\n'
code+=r"""
int main(){
 float axis[3][3];AxisClear(axis);float host[3]{0,0,60},guestSound[3]{2990,0,60},hostSound[3]{10,0,60};
 SNDL_SetListener(0,0,1,host,axis);snd_alias_t spatial,global;global.flags=0;
 assert(SND_ActiveListenerCount()==1&&SND_GetListenerIndexNearestToOrigin(guestSound)==0);
 assert(Snd_GetGlobalPriorityVolume(&spatial,guestSound)==0);
#ifdef KISAK_EXPERIMENTAL_SPLITSCREEN
 CL_SplitGuestUpdateSound();assert(SND_ActiveListenerCount()==2);
 assert(g_snd.listeners[1].clientNum==1&&g_snd.listeners[1].orient.axis[0][0]<-.99f);
 assert(SND_GetListenerIndexNearestToOrigin(guestSound)==1&&SND_GetListenerIndexNearestToOrigin(hostSound)==0);
 assert(Snd_GetGlobalPriorityVolume(&spatial,guestSound)>.96&&Snd_GetGlobalPriorityVolume(&spatial,hostSound)>.96);
 assert(Snd_GetGlobalPriorityVolume(&global,guestSound)==0);
 float far[3]{7000,0,60},nearest[3];assert(Snd_GetGlobalPriorityVolume(&spatial,far)==0);
 SND_GetNearestListenerPosition(guestSound,nearest);assert(nearest[0]==3000&&nearest[2]==60);
 // Disconnecting either slot preserves the other slot's spatial voices.
 g_snd.voiceAliasHash[0]=1;g_snd.voice[0].alias=&spatial;
 SNDL_DisconnectListener(0);assert(stops==0&&SND_ActiveListenerCount()==1&&SND_GetListenerIndexNearestToOrigin(hostSound)==1);
 SNDL_SetListener(0,0,1,host,axis);guestVisible=false;CL_SplitGuestUpdateSound();assert(stops==0&&!g_snd.listeners[1].active);
 guestVisible=true;liveLook=false;CL_SplitGuestUpdateSound();assert(std::abs(g_snd.listeners[1].orient.axis[0][1]-1)<.001);
 usePrediction=true;CL_SplitGuestUpdateSound();assert(g_snd.listeners[1].orient.origin[0]==3100&&g_snd.listeners[1].orient.origin[2]==65);usePrediction=false;CL_SplitGuestUpdateSound();
#endif
 // A model beside P2 stays detailed even behind P1's camera; distant models
 // still reduce detail or cull, and removing P2 restores ordinary host LOD.
 refdef_s ref;XModel model;DObj obj;obj.localModels[0]=&model;char lod[1];offscreenScale.current.value=8;
 R_SetLodOrigin(&ref);DObjGetSurfaceData(&obj,guestSound,1,true,lod);
#ifdef KISAK_EXPERIMENTAL_SPLITSCREEN
 assert(lod[0]==0&&rg.lodParms.extraCamOriginValid);guestVisible=false;
#else
 assert(lod[0]==-1);
#endif
 R_SetLodOrigin(&ref);DObjGetSurfaceData(&obj,guestSound,1,false,lod);assert(lod[0]==-1&&!rg.lodParms.extraCamOriginValid);
 float middle[3]{200,0,0};DObjGetSurfaceData(&obj,middle,1,false,lod);assert(lod[0]==1);
 DObjGetSurfaceData(&obj,hostSound,1,false,lod);assert(lod[0]==0);
 SNDL_DisconnectListener(0);
#ifdef KISAK_EXPERIMENTAL_SPLITSCREEN
 assert(stops==0);SNDL_DisconnectListener(1);assert(stops==1);
#endif
 assert(SND_ActiveListenerCount()==0);puts("Shared model LOD and listener spatial/lifetime checks passed");
}
"""
# Guest helpers are present in the fixture but only called with two listener slots.
for split in [False,True]:
 cpp=out/f'spatial-{int(split)}.cpp';cpp.write_text(code);exe=cpp.with_suffix('.exe')
 command=[args.compiler,'-std=c++17','-w',f'-DMAX_LOCAL_CLIENTS={2 if split else 1}',str(cpp),'-o',str(exe)]
 if split:command+=['-DKISAK_EXPERIMENTAL_SPLITSCREEN=1']
 subprocess.run(command,check=True);subprocess.run([str(exe)],check=True)
