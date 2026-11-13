"""Test actual guest interpolation/vision and render-only FX ownership without starting the game."""
from pathlib import Path
import argparse,re,subprocess
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--compiler',default='/c/msys64/ucrt64/bin/g++.exe');args=p.parse_args()
root=Path(__file__).resolve().parents[3];out=root/'build-nx/splitscreen-presentation-tests';out.mkdir(parents=True,exist_ok=True)
def read(n):return (root/n).read_text()
def function(n,sig):
 s=read(n);a=s.index(sig);b=s.index('{',a)+1;depth=1
 while depth:depth+=(s[b]=='{')-(s[b]=='}');b+=1
 return s[a:b]
def clean(s):return re.sub(r'^\s*#(?:include|pragma).*\n','',s,flags=re.M).replace('__int32','int')
pre=r"""
#include <cassert>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <string>
#include <map>
#include <vector>
#ifndef __cdecl
#define __cdecl
#endif
#define KISAK_SP 1
#define KISAK_EXPERIMENTAL_SPLITSCREEN 1
"""
h=read('src/cgame/cg_visionsets.h');end=h.index('struct visField_t');h=clean(h[:end])
code=pre+h+r"""
struct playerState_s {int pm_type=0,clientNum=1,eFlags=0;int stats[5]{};unsigned perks[2]{};float visionSetLerpRatio=0;float origin[3]{},viewangles[3]{},viewHeightCurrent=60;};
struct snapshot_s {playerState_s ps;int serverTime=0;};
struct Overlay {float amount=0;};
struct refdef_s {GfxVisionSet visionset;float postEmissiveBrightening=0,vieworg[3]{},viewaxis[3][3]{},tanHalfFovX=2,tanHalfFovY=.7f,zFar=1000;int time=0;bool playerTeleported=false;Overlay dof,flameFx,waterSheetingFx,genericFilter,poisonFx,electrifiedFx,transportedFx;};
struct cg_s {snapshot_s *snap=nullptr,*nextSnap=nullptr;float flareRatioLastFrame=0;char visionNameFlare[64]{};refdef_s refdef;int localClientNum=0,clientNum=0,time=0;playerState_s predictedPlayerState;bool lowHealthVision=false,underwaterVision=false;int damageVisionEndTime=0;char visionNameNaked[64]{};visionSetVars_t visionSetFrom[VISIONSETMODECOUNT]{},visionSetTo[VISIONSETMODECOUNT]{},visionSetCurrent[VISIONSETMODECOUNT]{};visionSetLerpData_t visionSetLerpData[VISIONSETMODECOUNT]{};} games[2];
cg_s *CG_GetLocalClientGlobals(int n){return &games[n];}
struct {int realtime=0;} cls;
struct {struct {int serverTime=0;playerState_s ps;}snap;}clients[2];
constexpr int CS_VISIONSET_NAKED=1550,CS_SP_VISIONSET_LASTSTAND=1588;
std::map<int,std::string> configs;bool hasView=true,liveLook=true;int waits=0,resets=0,configCalls[3]{},updates=0;
void R_WaitFrontendWorkerCmds(){++waits;}void FX_SplitGuestReset(){++resets;}
void SND_DisconnectListener(unsigned n){assert(n==1);}
void CG_SplitGuestResetHud(){}
const char *CL_GetConfigString(int n){return configs[n].c_str();}
const char *Info_ValueForKey(const char *text,const char *key){assert(!std::strcmp(key,"1"));return text;}
void I_strncpyz(char *to,const char *from,int n){std::snprintf(to,n,"%s",from);}
bool CL_SplitGuestGetViewAngles(float *a){if(!liveLook)return false;a[0]=5;a[1]=80;a[2]=0;return true;}
bool CL_SplitGuestHasView(){return hasView;}
void CL_SplitGuestResetPrediction(){}
void AnglesToAxis(const float *,float (*)[3]);
bool CL_SplitGuestGetWorldCamera(float *origin,float (*axis)[3]){if(!hasView)return false;auto &ps=clients[1].snap.ps;std::memcpy(origin,ps.origin,12);origin[2]+=ps.viewHeightCurrent;float angles[3];if(!CL_SplitGuestGetViewAngles(angles))std::memcpy(angles,ps.viewangles,12);AnglesToAxis(angles,axis);return true;}
void AnglesToAxis(const float *a,float axis[3][3]){axis[0][0]=a[1];}
struct FxCameraUpdate {float vieworg[3]{},viewaxis[3][3]{},tanHalfFovX=0,tanHalfFovY=0,zfar=0;};FxCameraUpdate extra;
void FX_SetExtraCamera(int n,const FxCameraUpdate *c){assert(n==0);extra=*c;}
void CG_InitVisionSets(int n){assert(n==1);}
bool CG_VisionSetStartLerp_To(int n,int mode,int,const char *name,int){assert(n==1&&mode==VISIONSETMODE_LASTSTAND&&!std::strcmp(name,"laststand"));games[n].visionSetCurrent[mode].filmEnable=true;for(auto &v:games[n].visionSetCurrent[mode].filmSaturation)v=0;return true;}
void CG_VisionSetConfigString_Naked(int n){assert(n==1);++configCalls[0];games[n].visionSetCurrent[0].filmSaturation[0]=.65f;}
void CG_VisionSetConfigString_Night(int n){assert(n==1);++configCalls[1];}
void CG_VisionSetConfigString_LastStand_SP(int n){assert(n==1);++configCalls[2];}
void CG_VisionSetsUpdate(int n){assert(n==1);++updates;}
bool CG_InFlareVision(int){return false;}bool CG_IsInfrared(int){return false;}bool CG_IsTvguided(int,bool){return false;}char CG_LookingThroughNightVision(int){return false;}
"""
code+=function('src/cgame/cg_visionsets.cpp','void __cdecl CG_VisionSetApplyToRefdef(')+'\n'
code+=clean(read('src/client/splitscreen_presentation.cpp'))+r"""
void sample(int time,int now,float x,refdef_s &view){clients[1].snap.serverTime=time;cls.realtime=now;view.vieworg[0]=x;view.vieworg[1]=0;view.vieworg[2]=60;view.playerTeleported=false;CL_SplitGuestSmoothCamera(&view);}
bool near(float a,float b){return std::fabs(a-b)<.001f;}
int main(){
 refdef_s v;sample(1000,0,0,v);assert(v.playerTeleported&&v.vieworg[0]==0);
 sample(1050,50,10,v);assert(v.vieworg[0]==0);sample(1050,66,10,v);assert(near(v.vieworg[0],3.2f));
 sample(1050,82,10,v);assert(near(v.vieworg[0],6.4f));sample(1050,100,10,v);assert(v.vieworg[0]==10);
 sample(1100,100,20,v);assert(v.vieworg[0]==10);sample(1100,125,20,v);assert(v.vieworg[0]==15);
 // Early arrivals start at the position already on screen, without a jump.
 sample(1150,125,30,v);assert(v.vieworg[0]==15);sample(1150,150,30,v);assert(v.vieworg[0]==22.5f);
 sample(1150,500,30,v);assert(v.vieworg[0]==30); // packet stall never extrapolates through walls
 clients[1].snap.ps.eFlags=2;sample(1200,550,100,v);assert(v.playerTeleported&&v.vieworg[0]==100);
 clients[1].snap.ps.pm_type=6;sample(1250,600,102,v);assert(v.playerTeleported&&v.vieworg[0]==102);
 clients[1].snap.ps.stats[4]++;sample(1300,625,108,v);assert(v.playerTeleported&&v.vieworg[0]==108); // same mode/teleport bit, new spawn
 sample(900,650,-20,v);assert(v.playerTeleported&&v.vieworg[0]==-20);
 auto &host=games[0];auto &guest=games[1];std::strcpy(host.visionNameNaked,"kino");
 for(auto &channel:host.visionSetCurrent){channel.filmEnable=true;channel.charPrimaryDiffuseScale=1;channel.charPrimarySpecularScale=1;for(float &x:channel.filmSaturation)x=1;}
 // Host is already downed and monochrome before the guest initializes.
 host.predictedPlayerState.pm_type=6;host.refdef.visionset.film.filmSaturation[0]=0;host.refdef.poisonFx.amount=1;
 clients[1].snap.ps.pm_type=0;v=host.refdef;v.time=2000;CL_SplitGuestApplyVision(&v);
 assert(v.visionset.film.filmSaturation[0]==1&&host.refdef.visionset.film.filmSaturation[0]==0);
 assert(v.poisonFx.amount==0&&guest.predictedPlayerState.viewangles[1]==80);
 clients[1].snap.ps.pm_type=6;CL_SplitGuestApplyVision(&v);assert(v.visionset.film.filmSaturation[0]==0);
 clients[1].snap.ps.pm_type=0;CL_SplitGuestApplyVision(&v);assert(v.visionset.film.filmSaturation[0]==1);
 configs[CS_VISIONSET_NAKED]="guest_vision 100";CL_SplitGuestApplyVision(&v);assert(configCalls[0]==1&&near(v.visionset.film.filmSaturation[0],.65f));
 CL_SplitGuestApplyVision(&v);assert(configCalls[0]==1);configs[CS_SP_VISIONSET_LASTSTAND]="laststand_alt 200";CL_SplitGuestApplyVision(&v);assert(configCalls[2]==1);
 clients[1].snap.ps.origin[0]=123;CL_SplitGuestSetFxCamera();assert(extra.vieworg[0]==123&&extra.vieworg[2]==60&&extra.viewaxis[0][0]==80);
 extra.vieworg[0]=0;hasView=false;CL_SplitGuestSetFxCamera();assert(extra.vieworg[0]==0);
 CL_SplitGuestResetPresentation();assert(!s_guestPresentation.valid&&waits==1&&resets==1);
 puts("PASS: 20 Hz snapshots render smoothly between updates, jitter continuity, teleport/respawn resets, independent native last-stand vision and second-camera FX visibility");
}
"""
def run(name,source):
 cpp=out/(name+'.cpp');exe=out/(name+'.exe');cpp.write_text(source)
 subprocess.run([args.compiler,'-std=c++17','-O0',str(cpp),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
run('presentation',code)
# Execute the native MP vision updater on a cold guest, before any HUD or gun
# helper has supplied cg.snap. This is the first-spawn access-violation path.
mp=code[:code.index('void sample(')].replace('#define KISAK_SP 1','')
apply=function('src/cgame/cg_visionsets.cpp','void __cdecl CG_VisionSetApplyToRefdef(')
mp=mp.replace(apply,'void CG_VisionSetApplyToRefdef(int n,bool){assert(n==1);}')
old='void CG_VisionSetsUpdate(int n){assert(n==1);++updates;}'
helpers=r"""
void SetDefaultVision(int n){std::strcpy(games[n].visionNameNaked,"default");}
void UpdateVarsLerp(int,const visionSetVars_t*,const visionSetVars_t*,visionSetLerpData_t*,visionSetVars_t*){}
float observedFlare=-1;
double CalculateFlareRatio(float ratio,float){return ratio;}
void UpdateVarsLerpCustom(float ratio,const visionSetVars_t*,const visionSetVars_t*,visionSetLerpData_t*,visionSetVars_t*){observedFlare=ratio;++updates;}
"""
mp=mp.replace(old,helpers+function('src/cgame/cg_visionsets.cpp','void __cdecl CG_VisionSetsUpdate('))
start='bool CG_VisionSetStartLerp_To(int n,int mode,int,const char *name,int)'
a=mp.index(start);b=mp.index('\n',a)
mp=mp[:a]+'bool CG_VisionSetStartLerp_To(int n,int mode,int,const char *,int){assert(n==1&&mode==VISIONSETMODE_FLARE);return true;}'+mp[b:]
mp+=r"""
int main(){
 auto &host=games[0];auto &guest=games[1];std::strcpy(host.visionNameNaked,"nuketown");
 refdef_s view;view.time=1050;
 clients[1].snap.ps.clientNum=1;clients[1].snap.serverTime=1050;
 clients[1].snap.ps.visionSetLerpRatio=.75f;
 assert(!guest.snap&&!guest.nextSnap);
 CL_SplitGuestApplyVision(&view);
 assert(guest.snap&&guest.snap==guest.nextSnap&&guest.snap->serverTime==1050);
 assert(guest.snap->ps.clientNum==1&&guest.snap->ps.visionSetLerpRatio==.75f&&observedFlare==.75f);
 assert(!host.snap&&!host.nextSnap);
 clients[1].snap.ps.perks[1]=0x400;clients[1].snap.serverTime=1100;
 CL_SplitGuestApplyVision(&view);assert(observedFlare==0&&guest.snap->serverTime==1100);
 clients[1].snap.ps.perks[1]=0;clients[1].snap.ps.visionSetLerpRatio=.25f;
 CL_SplitGuestApplyVision(&view);assert(observedFlare==.25f);
 CL_SplitGuestResetPresentation();guest.snap=guest.nextSnap=nullptr;
 CL_SplitGuestApplyVision(&view);assert(guest.snap&&observedFlare==.25f);
 puts("PASS: native MP vision reads guest-owned snapshot on first spawn, fresh snapshots, perk suppression and rejoin before HUD/viewmodel initialization");
}
"""
run('presentation-mp',mp)
fx=pre+r"""
struct FxCamera {float origin[3]{},axis[3][3]{};unsigned isValid=1;};
struct FxSpriteInfo {int indexCount=7;};struct FxEffectDef {int id=1;}flash;
struct FxEffect {unsigned short owner=0;unsigned uniqueHandle=0;struct {int dobjHandle=0;}boltAndSortOrder;};
struct FxEffectContainer {FxEffect effect;bool alive=true;}pool[128];
struct FxSystem {int localClientNum=0,msecDraw=1234,msecNow=1234,frameCount=5;bool isInitialized=true;FxCamera camera;FxSpriteInfo sprite;unsigned gfxCloudCount=4;} ;
struct FxSystemContainer {FxSystem system;}container;
struct FxGenerateVertsCmd {FxSystem *system;void *postLightInfo=nullptr;float vieworg[3]{},viewaxis[3][3]{};};
struct FxReflectParams {};
struct refdef_s {float vieworg[3]{10,20,30},viewaxis[3][3]{{0,1,0}},tanHalfFovX=2,tanHalfFovY=1,zFar=1000;int time=1234;};
struct playerState_s {int predictableEventSequence=0,predictableEvents[4]{},weapon=1;};struct {struct {playerState_s ps;}snap;}clients[2];
struct cg_s {playerState_s predictedPlayerState;int viewModelPose=1;}game;
struct ViewModelInfo {void *viewModelDObj=(void*)1;}model;
struct WeaponDef {FxEffectDef *viewFlashEffect=&flash;}weapon;
struct {unsigned tag_flash=10;}scr_const;
constexpr int EV_FIRE_WEAPON=29,EV_FIRE_WEAPON_LASTSHOT=30,EV_FIRE_WEAPON_LEFT=31,EV_FIRE_WEAPON_LASTSHOT_LEFT=32;
struct dvar_t {struct{bool enabled=true;}current;}enabled;auto fx_enable=&enabled;auto fx_draw=&enabled;
int created=0,killed=0,meshCount=100,begin=0,end=0,sprites=0,beams=0,lights=0,sorts=0,nonSprite=0,draws=0;
FxSystemContainer *FX_GetSystem(int n){assert(n==0);return &container;}
FxEffectContainer *UniqueHandleToEffect(FxSystem *,unsigned h){for(auto &e:pool)if(h&&e.alive&&e.effect.uniqueHandle==h)return &e;return nullptr;}
void FX_KillEffect(FxSystem *,FxEffectContainer *e){assert(e->alive);e->alive=false;++killed;}
cg_s *CG_GetLocalClientGlobals(int n){assert(n==1);return &game;}
const ViewModelInfo *CG_GetLocalClientViewModelInfo(int n){assert(n==1);return &model;}
int BG_GetViewmodelWeaponIndex(const playerState_s *){return 1;}int BG_GetNumWeapons(){return 2;}const WeaponDef *BG_GetWeaponDef(int){return &weapon;}
int CG_DObjGetWorldTagMatrix(const int *,void *,unsigned tag,float axis[3][3],float *origin){assert(tag==10);origin[0]=99;axis[0][0]=1;return 1;}
unsigned FX_PlayOrientedEffect(FxSystem *,const FxEffectDef *def,int time,const float *origin,const float axis[3][3]){assert(def==&flash&&origin[0]==99&&axis[0][0]==1&&time==1234);++created;auto &e=pool[created];e.alive=true;e.effect.uniqueHandle=1000+created;e.effect.owner=created;return e.effect.uniqueHandle;}
void FX_SetupCamera(FxCamera *c,const float *o,const float axis[3][3],float,float,float){std::memcpy(c->origin,o,12);std::memcpy(c->axis,axis,36);}
bool FX_SplitGuestIsDrawing();bool FX_SplitGuestEffectVisible(const FxSystem *,const FxEffect *);
void FX_SortEffects(FxSystem *s){assert(FX_SplitGuestIsDrawing()&&s->camera.origin[0]==10);++sorts;}
void FX_GetNullReflection(FxReflectParams *){}
void FX_DrawNonSpriteElems(FxSystem *,const FxReflectParams *,int){assert(FX_SplitGuestIsDrawing());++nonSprite;}
void FX_FillGenerateVertsCmd(int n,FxGenerateVertsCmd *cmd){assert(n==0);cmd->system=&container.system;}
void R_BeginCodeMeshVerts(){++begin;}void R_EndCodeMeshVerts(){++end;}
void FX_SpriteGenerateVerts(FxGenerateVertsCmd *cmd){assert(cmd->vieworg[0]==10&&cmd->viewaxis[0][1]==1);meshCount+=10;++sprites;}
void FX_Beam_GenerateVerts(FxGenerateVertsCmd *){meshCount+=10;++beams;}void FX_PostLight_GenerateVerts(void *,FxSystem *){meshCount+=10;++lights;}
void FX_DrawSpriteElems(FxSystem *s,int time,const FxReflectParams *,int){assert(time==1234&&FX_SplitGuestIsDrawing());s->sprite.indexCount=99;s->gfxCloudCount=50;++draws;}
"""
fx+=clean(read('src/EffectsCore/fx_shared_guest.cpp'))+r"""
int main(){
 refdef_s view;auto &ps=clients[1].snap.ps;ps.predictableEventSequence=100;FX_SplitGuestMuzzleFlash(&view);assert(!created);
 ps.predictableEvents[0]=EV_FIRE_WEAPON;ps.predictableEventSequence=101;FX_SplitGuestMuzzleFlash(&view);assert(created==1);
 FX_SplitGuestMuzzleFlash(&view);assert(created==1);assert(!FX_SplitGuestEffectVisible(&container.system,&pool[1].effect));
 FxEffect child;child.owner=1;assert(!FX_SplitGuestEffectVisible(&container.system,&child));
 s_drawingGuestFx=true;assert(FX_SplitGuestEffectVisible(&container.system,&child));child.boltAndSortOrder.dobjHandle=1536;assert(!FX_SplitGuestEffectVisible(&container.system,&child));s_drawingGuestFx=false;
 auto before=container.system;FX_DrawSharedGuest(&view);assert(begin==1&&end==1&&meshCount==130&&sorts==1&&nonSprite==1&&draws==1);
 assert(container.system.camera.origin[0]==before.camera.origin[0]&&container.system.sprite.indexCount==7&&container.system.gfxCloudCount==4);
 assert(container.system.msecNow==1234&&container.system.msecDraw==1234&&container.system.frameCount==5&&!s_drawingGuestFx);
 ps.predictableEventSequence=102;ps.predictableEvents[1]=10;FX_SplitGuestMuzzleFlash(&view);assert(created==1);
 ps.predictableEventSequence=103;ps.predictableEvents[2]=EV_FIRE_WEAPON_LASTSHOT;FX_SplitGuestMuzzleFlash(&view);assert(created==2);
 // The same pool slot reused with another generation must never be hidden/killed.
 pool[1].effect.uniqueHandle=9999;assert(FX_SplitGuestEffectVisible(&container.system,&pool[1].effect));
 FX_SplitGuestReset();assert(killed==1&&pool[1].alive&&!pool[2].alive&&!s_haveGuestFireSequence);
 enabled.current.enabled=false;FX_DrawSharedGuest(&view);assert(begin==1);
 puts("PASS: guest FX camera and append-only meshes, no second simulation tick, restored upper-view scratch, private muzzle ownership/generations, fire-event deduplication and cleanup");
}
"""
run('fx',fx)
# These are the real draw entry points; their first gate protects all classes,
# including trails and the host's depth-hacked muzzle particles.
for sig in ['void __cdecl FX_DrawSpriteEffect(', 'void __cdecl FX_DrawNonSpriteEffect(', 'void __cdecl FX_DrawTrailsForEffect(']:
 body=function('src/EffectsCore/fx_draw.cpp',sig)
 assert 'FX_SplitGuestEffectVisible(system, effect)' in body[:900]
assert 'CL_SplitGuestSetFxCamera();' in function('src/EffectsCore/fx_update.cpp','void __cdecl FX_BeginUpdate(')
print('PASS: all native FX draw classes use the ownership gate and FX spawn visibility receives both cameras')
