"""Exercise real snapshot-camera, scene reuse and two-view dispatch without launching the game."""
from pathlib import Path
import argparse
import subprocess

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler',default='/c/msys64/ucrt64/bin/g++.exe')
args=parser.parse_args()
root=Path(__file__).resolve().parents[3]
out=root/'build-nx/splitscreen-camera-tests';out.mkdir(parents=True,exist_ok=True)
def function(path,signature):
    source=(root/path).read_text(encoding='utf-8',errors='replace')
    start=source.index(signature);end=source.index('{',start)+1;depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]
code=r'''
#include <cassert>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <vector>
#ifndef __cdecl
#define __cdecl
#endif
#define KISAK_EXPERIMENTAL_SPLITSCREEN 1
#define PROF_SCOPED_RUNTIME_NAME(x)
#define __debugbreak() assert(false)
constexpr int CA_ACTIVE=10;
using uint=unsigned;
struct GfxViewport {int x=0,y=0,width=0,height=0;};
struct refdef_s {
    unsigned x=0,y=0,width=1280,height=720;
    float tanHalfFovX=1.0f,tanHalfFovY=0.75f,vieworg[3]{},viewaxis[3][3]{},yaw=0;
    int time=5000,localClientNum=0,splitscreen=0;bool playerTeleported=false;
    bool useScissorViewport=false,extraCamPosValid=false;
    float blurRadius=0;
    struct {int value=0;} doubleVision,saveScreenFx;
};
struct playerState_s {int pm_type=0,clientNum=1,otherFlags=4;float origin[3]{10,20,30},viewangles[3]{0,90,0},viewHeightCurrent=60;};
struct clientActive_t {struct {bool valid=true;int snapFlags=0;playerState_s ps;} snap;};
clientActive_t clientStore[2];auto clients=clientStore;
struct clientConnection_t {int clientNum=1;} connectionStore[2];auto clientConnections=connectionStore;
struct cg_s {refdef_s refdef;playerState_s predictedPlayerState;struct{struct{int team=0;}clientinfo[32];}bgs;int frametime=16;int sentinel=123;} games[2];
struct cgs_t {float viewAspect=16.0f/9.0f;} statics[2];
struct {int serverId=42;struct {int displayWidth=1280,displayHeight=721;} vidConfig;} cls;
bool s_splitGuestConnected=true,viewEnabled=true,fullscreen=false,s_splitGuestViewSubmitted=false;
bool usePrediction=false;float listenerOrigin[3]{};
void SND_SetListener(unsigned n,int,int,const float *p,const float(*)[3]){assert(n==1&&p[2]==(usePrediction?333:90));std::memcpy(listenerOrigin,p,12);}
void CL_SplitGuestLog(const char *){}
void CL_SplitGuestSmoothCamera(refdef_s *){}
void CL_SplitGuestApplyVision(refdef_s *){}
bool CG_SplitGuestDrawHud(const refdef_s *);
void FX_SplitGuestMuzzleFlash(const refdef_s *){}
void FX_DrawSharedGuest(const refdef_s *){}
int states[2]{CA_ACTIVE,CA_ACTIVE};
int CL_GetLocalClientConnectionState(int n){return states[n];}
bool Dvar_GetBool(const char *name){assert(!std::strcmp(name,"splitguest_view"));return viewEnabled;}
bool UI_IsFullscreen(int n){assert(n==0);return fullscreen;}
bool guestMenu=false;bool CL_SplitGuestOwnsMenu(){return guestMenu;}
cg_s *CG_GetLocalClientGlobals(int n){return &games[n];}
cgs_t *CG_GetLocalClientStaticGlobals(int n){return &statics[n];}
bool liveLook=false;
bool CL_SplitGuestGetViewAngles(float *a){a[0]=12;a[1]=75;a[2]=0;return liveLook;}
bool CL_SplitGuestPredictedCamera(float *origin,float *a){if(!usePrediction)return false;origin[0]=111;origin[1]=222;origin[2]=333;a[0]=12;a[1]=75;a[2]=0;return true;}
void AnglesToAxis(const float *angles,float axis[3][3]){
    assert(angles[0]==(liveLook?12:0)&&angles[1]==(liveLook?75:90));std::memset(axis,0,9*sizeof(float));axis[0][1]=1;axis[1][0]=-1;axis[2][2]=1;
}
struct GfxSceneEntity {unsigned short entnum=0,gfxEntIndex=0;};
struct GfxViewParms {float origin[3]{};};
struct GfxSceneParms {int localClientNum=0;GfxViewport viewport;};
struct GfxViewInfo {struct {GfxViewport sceneViewport;} cullViewInfo;int id=0,localClientNum=0;bool isMissileCamera=false,hasCmdBuf=false;const void *cmds=nullptr;struct {int dirty=0;} rbUI3D;};
struct GfxBackEndData {unsigned shadowableLightHasShadowMap[8]{},spotShadowCount=0,emissiveSpotLightCount=0;unsigned viewInfoCount=1,viewInfoIndex=0;bool splitscreen=false;GfxViewInfo viewInfo[4];struct {unsigned renderFxFlags=0;} gfxEnts[8];} frame;
auto frontEndDataOut=&frame;auto backEndData=&frame;
struct {} gfxCmdBufState;
void R_SetViewport(decltype(gfxCmdBufState) *,const GfxViewport *){}
struct {bool registered=true;struct {int unused=0;} x;} rg;
struct {struct {bool enabled=false;} current;} norefresh;
auto r_norefresh=&norefresh;
struct World {
    struct {int surfaceCount=8;} model;
    decltype(model) *models=&model;
    unsigned char modelCurrent[8]{},modelPrevious[8]{},surfaceCurrent[8]{},surfacePrevious[8]{};
    struct {unsigned smodelCount=8;unsigned char *smodelVisData[1],*surfaceVisData[1],*smodelVisDataCameraSaved,*surfaceVisDataCameraSaved;} dpvs;
    World(){dpvs.smodelVisData[0]=modelCurrent;dpvs.surfaceVisData[0]=surfaceCurrent;dpvs.smodelVisDataCameraSaved=modelPrevious;dpvs.surfaceVisDataCameraSaved=surfacePrevious;}
} world;
struct {World *world=&::world;} rgp;
struct {struct {unsigned short sceneDObjIndex[1536]{};} dpvs;unsigned drawSurfCount[20]{};unsigned sceneDObjCount=2,sceneModelCount=1;GfxSceneEntity sceneDObj[1024];unsigned char sceneDObjVisData[7][1024]{},sceneModelVisData[7][1024]{};struct {float floatTime=5;} def;} scene;
int waits=0,visibilityClears=0,vertexClears=0,submissions=0;
int shadowSelections=0,drawMethodUpdates=0;bool allowShadows=true;
void Com_Memset(void *p,int v,unsigned n){std::memset(p,v,n);}
bool R_GetAllowShadowMaps(){return allowShadows;}
void R_ChooseShadowedLights(GfxViewInfo *){assert(frame.spotShadowCount==0);++shadowSelections;}
void R_UpdateDrawMethod(GfxBackEndData *){++drawMethodUpdates;}
static void R_SelectViewShadowMaps(GfxViewInfo *,bool);
int bodyAdds=0,weaponAdds=0;
bool CG_SplitGuestAddHostBody(){
    const unsigned index=scene.sceneDObjCount++;scene.sceneDObj[index]={0,static_cast<unsigned short>(index+1)};
    scene.dpvs.sceneDObjIndex[0]=index;frame.gfxEnts[index+1].renderFxFlags=0x400024;++bodyAdds;return true;
}
bool CG_SplitGuestAddViewmodel(refdef_s *,int){
    assert(scene.dpvs.sceneDObjIndex[1023]==0xFFFF);
    const unsigned index=scene.sceneDObjCount++;scene.sceneDObj[index]={1023,static_cast<unsigned short>(index+1)};
    scene.dpvs.sceneDObjIndex[1023]=index;frame.gfxEnts[index+1].renderFxFlags=0x400002;++weaponAdds;return true;
}
refdef_s submitted;
void R_WaitFrontendWorkerCmds(){++waits;}
void R_ClearDpvsScene_For_ExtraCam(){++visibilityClears;}
void R_ClearSceneDynSModelVisBits(){++vertexClears;}
GfxViewParms parms;
GfxViewParms *R_AllocViewParms(){return &parms;}
void R_SetViewParmsForScene(refdef_s *view,GfxViewParms *out){std::memcpy(out->origin,view->vieworg,sizeof(out->origin));}
void R_SetSceneParms(refdef_s *view,GfxSceneParms *out){out->localClientNum=view->localClientNum;out->viewport={int(view->x),int(view->y),int(view->width),int(view->height)};}
void R_CorrectLodScale(refdef_s *){}
int lodView=-1;
int R_WorldLod_SelectRenderView(int view){const int previous=lodView;lodView=view;return previous;}
void R_WorldLod_FrameUpdate(float,float *,int owner){assert(owner==0&&lodView==1);}
void R_GenerateSortedDrawSurfs(const GfxSceneParms *sp,const GfxViewParms *,const GfxViewParms *,int,const refdef_s *view,bool missile,bool shared){
    R_SelectViewShadowMaps(&frame.viewInfo[1],shared);
    assert(shadowSelections==0&&drawMethodUpdates==0);
    assert(frame.spotShadowCount==0&&frame.emissiveSpotLightCount==0);
    for(auto bits:frame.shadowableLightHasShadowMap)assert(bits==0);
    assert(!missile&&shared&&sp->localClientNum==0); // world ownership is unchanged
    assert((frame.gfxEnts[2].renderFxFlags&1)!=0); // guest body culled only in this view
    assert((frame.gfxEnts[1].renderFxFlags&1)!=0); // host first-person gun stays hidden
    assert((frame.gfxEnts[3].renderFxFlags&1)==0); // host body visible to guest
    assert(frame.gfxEnts[scene.dpvs.sceneDObjIndex[1023]+1].renderFxFlags==0x400002); // own depth-hacked gun
    for(auto count:scene.drawSurfCount)assert(count==0);
    assert(world.dpvs.smodelVisDataCameraSaved[0]==(submissions==0?0:99));
    std::memset(world.modelCurrent,99,8);std::memset(world.surfaceCurrent,88,8);
    ++submissions;submitted=*view;frame.viewInfoIndex=frame.viewInfoCount++;
    frame.viewInfo[frame.viewInfoIndex].id=1;
}
bool guestHudEnabled=false;
bool CG_SplitGuestDrawHud(const refdef_s *){if(guestHudEnabled)frame.viewInfo[frame.viewInfoCount].cmds=(void*)0x5678;return guestHudEnabled;}
void R_ClearClientCmdList2D(){frame.viewInfo[frame.viewInfoCount].cmds=nullptr;}
void R_RenderSharedGuestView(refdef_s *,int,int);
std::vector<int> draws;
void RB_ResetStatTracking(int){}
void RB_Draw3DInternal(GfxViewInfo *view){draws.push_back(view->id);}
void R_WindStartBackend(){}
char *va(const char *,...){static char text[]="test";return text;}
template<class...T> bool Assert_MyHandler(T...){return true;}
template<class...T> void Com_Printf(T...){}
'''
header=(root/'src/client/splitscreen.h').read_text()
code+=header[header.index('struct LocalClientViewport'):header.index('#ifdef KISAK_EXPERIMENTAL_SPLITSCREEN')]+"\n"
source=(root/'src/gfx_d3d/r_scene.cpp').read_text()
start=source.index('struct SharedGuestShadowState');end=source.index('};',start)+2
code+=source[start:end]+'\n'
for signature in ['static void R_SharedGuestShadowState(', 'static void R_SelectViewShadowMaps(', 'void R_RenderSharedGuestView(']:
    code+=function('src/gfx_d3d/r_scene.cpp',signature)+'\n'
for signature in ['bool CL_SplitGuestHasViewport(', 'bool CL_SplitGuestHasView(', 'void CL_SplitGuestApplyHostViewport(', 'void CL_SplitGuestRender(']:
    code+=function('src/client/splitscreen.cpp',signature)+'\n'
code+=function('src/gfx_d3d/rb_backend.cpp','void __cdecl RB_Draw3D()')+'\n'
code+=function('src/gfx_d3d/rb_draw3d.cpp','static const GfxViewport *RB_SetSceneClearViewport(')+'\n'
code+=r'''
int main(){
    std::memset(world.modelCurrent,25,8);std::memset(world.modelPrevious,51,8);
    std::memset(world.surfaceCurrent,26,8);std::memset(world.surfacePrevious,52,8);
    games[0].predictedPlayerState.clientNum=0;
    for(auto &index:scene.dpvs.sceneDObjIndex)index=0xFFFF;
    scene.dpvs.sceneDObjIndex[1023]=0;
    frame.spotShadowCount=4;frame.emissiveSpotLightCount=1;
    for(auto &bits:frame.shadowableLightHasShadowMap)bits=0x12345678;
    frame.viewInfo[0].id=0;frame.viewInfo[0].cmds=(void *)0x1234;
    frame.viewInfo[1].cmds=(void *)0x8888;frame.viewInfo[1].rbUI3D.dirty=99;
    scene.sceneDObj[0]={1023,1};frame.gfxEnts[1].renderFxFlags=0x400003; // host viewmodel
    scene.sceneDObj[1]={1,2};frame.gfxEnts[2].renderFxFlags=0x40;
    scene.drawSurfCount[0]=17;scene.drawSurfCount[3]=19;
    refdef_s host=games[0].refdef;host.blurRadius=4;host.saveScreenFx.value=99;
    games[0].refdef=host;
    assert(CL_SplitGuestHasView());
    clients[1].snap.valid=false;states[1]=9;
    assert(CL_SplitGuestHasViewport()&&!CL_SplitGuestHasView()); // split before guest snapshot/class selection
    states[1]=CA_ACTIVE;clients[1].snap.valid=true;

    clients[1].snap.ps.pm_type=4;assert(CL_SplitGuestHasView()&&CL_SplitGuestHasViewport());
    clients[1].snap.ps.pm_type=5;assert(!CL_SplitGuestHasView());
    clients[1].snap.ps.pm_type=0;clients[1].snap.valid=false;assert(!CL_SplitGuestHasView());
    clients[1].snap.valid=true;clients[1].snap.snapFlags=2;assert(!CL_SplitGuestHasView());
    clients[1].snap.snapFlags=0;clients[1].snap.ps.clientNum=0;assert(!CL_SplitGuestHasView());guestMenu=true;assert(CL_SplitGuestHasView());guestMenu=false;
    clients[1].snap.ps.otherFlags=2;assert(CL_SplitGuestHasView());clients[1].snap.ps.otherFlags=4;
    clients[1].snap.ps.clientNum=1;fullscreen=true;assert(CL_SplitGuestHasView()&&CL_SplitGuestHasViewport());fullscreen=false;
    games[0].refdef.extraCamPosValid=true;assert(!CL_SplitGuestHasView());games[0].refdef.extraCamPosValid=false;
    viewEnabled=false;assert(!CL_SplitGuestHasView());CL_SplitGuestRender();assert(submissions==0);viewEnabled=true;
    CL_SplitGuestApplyHostViewport(&games[0].refdef);
    assert(games[0].refdef.x==0&&games[0].refdef.y==0&&games[0].refdef.height==360&&games[0].refdef.width==1280);
    assert(std::fabs(games[0].refdef.tanHalfFovX-games[0].refdef.tanHalfFovY*statics[0].viewAspect*721/360)<1e-6);
    const cg_s savedHost=games[0];const auto savedSnapshot=clients[1].snap;
    CL_SplitGuestRender();assert(submissions==1&&frame.viewInfoCount==2&&frame.splitscreen);
    assert(scene.dpvs.sceneDObjIndex[1023]==0);
    assert(frame.spotShadowCount==4&&frame.emissiveSpotLightCount==1);
    for(auto bits:frame.shadowableLightHasShadowMap)assert(bits==0x12345678);
    assert(submitted.y==360&&submitted.height==361&&submitted.width==1280&&submitted.localClientNum==0);
    assert(submitted.vieworg[0]==10&&submitted.vieworg[1]==20&&submitted.vieworg[2]==90);
    assert(submitted.viewaxis[0][1]==1&&submitted.yaw==90&&submitted.time==host.time);
    assert(submitted.blurRadius==0&&submitted.saveScreenFx.value==0);
    assert(waits==2&&visibilityClears==1&&vertexClears==1&&lodView==-1);
    assert(world.modelCurrent[0]==25&&world.modelPrevious[0]==51);
    assert(world.surfaceCurrent[0]==26&&world.surfacePrevious[0]==52);
    assert(frame.gfxEnts[1].renderFxFlags==0x400003&&frame.gfxEnts[2].renderFxFlags==0x40);
    assert(frame.viewInfo[0].cmds==(void *)0x1234&&frame.viewInfo[1].cmds==nullptr&&frame.viewInfo[1].rbUI3D.dirty==0);
    assert(!std::memcmp(&savedHost,&games[0],sizeof(savedHost)));
    assert(!std::memcmp(&savedSnapshot,&clients[1].snap,sizeof(savedSnapshot)));
    CL_SplitGuestRender();assert(submissions==1); // don't exceed the two-view frame
    frame.viewInfoCount=1;frame.viewInfoIndex=0;games[0].refdef.time+=16;
    liveLook=true;guestHudEnabled=true;
    CL_SplitGuestRender();assert(submissions==2); // second frame uses guest visibility history
    assert(submitted.yaw==75&&games[1].predictedPlayerState.viewangles[0]==12);
    assert(frame.viewInfo[1].cmds==(void*)0x5678&&frame.viewInfo[1].hasCmdBuf);
    assert(clients[1].snap.ps.viewangles[1]==90); // authoritative state remains untouched
    assert(bodyAdds==1&&weaponAdds==2&&scene.dpvs.sceneDObjIndex[1023]==0);
    assert(world.modelCurrent[0]==25&&world.modelPrevious[0]==51);
    assert(RB_SetSceneClearViewport(&frame.viewInfo[0],nullptr)==&frame.viewInfo[0].cullViewInfo.sceneViewport);
    assert(RB_SetSceneClearViewport(&frame.viewInfo[1],nullptr)==&frame.viewInfo[1].cullViewInfo.sceneViewport);
    frame.viewInfo[1].isMissileCamera=true;assert(RB_SetSceneClearViewport(&frame.viewInfo[1],nullptr)==nullptr);
    frame.viewInfo[1].isMissileCamera=false;
    frame.viewInfoCount=1;frame.viewInfoIndex=0;usePrediction=true;
    CL_SplitGuestRender();assert(listenerOrigin[0]==111&&listenerOrigin[2]==333);assert(submitted.vieworg[0]==111&&submitted.vieworg[1]==222&&submitted.vieworg[2]==333&&submitted.yaw==75);
    assert(!std::memcmp(&savedSnapshot,&clients[1].snap,sizeof(savedSnapshot)));
    usePrediction=false;
    RB_Draw3D();assert((draws==std::vector<int>{0,1}));
    draws.clear();frame.splitscreen=false;assert(RB_SetSceneClearViewport(&frame.viewInfo[0],nullptr)==nullptr);RB_Draw3D();assert((draws==std::vector<int>{1}));
    draws.clear();frame.viewInfo[1].isMissileCamera=true;RB_Draw3D();assert((draws==std::vector<int>{1,0}));
    draws.clear();frame.viewInfoCount=0;RB_Draw3D();assert(draws.empty());
    s_splitGuestConnected=false;auto restored=host;CL_SplitGuestApplyHostViewport(&restored);assert(restored.height==720);
    frame.spotShadowCount=0;R_SelectViewShadowMaps(&frame.viewInfo[0],false);
    assert(shadowSelections==1&&drawMethodUpdates==1);
    for(auto bits:frame.shadowableLightHasShadowMap)assert(bits==0);
    allowShadows=false;R_SelectViewShadowMaps(&frame.viewInfo[0],false);
    assert(shadowSelections==1&&drawMethodUpdates==2);
    puts("PASS: spawn-gated independent snapshot camera, odd-height viewports/FOV, shared scene isolation, and two-view backend dispatch");
}
'''
cpp=out/'camera.cpp';cpp.write_text(code,encoding='utf-8')
exe=out/'camera.exe'
subprocess.run([args.compiler,'-std=c++20',str(cpp),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)

# Exercise the real LOD allocation, fade writes and frame timing as well: a
# second view must not consume the primary camera's fade clock or storage.
lod=r'''
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>
#ifndef __cdecl
#define __cdecl
#endif
#ifndef __declspec
#define __declspec(x)
#endif
#ifndef __thiscall
#define __thiscall
#endif
#define __int16 short
#define MAX_LOCAL_CLIENTS 2
#define iassert(x) assert(x)
#define __debugbreak() assert(false)
#include "gfx_d3d/r_world_lod.h"
struct World {
    unsigned surfaceCount=8,worldLodChainCount=1;
    GfxWorldLodChain *worldLodChains;
    GfxWorldLodInfo *worldLodInfos;
    unsigned *worldLodSurfaces;
    struct {unsigned *sortedSurfIndex;} dpvs;
} world;
struct {World *world=&::world;} rgp;
unsigned char *s_lodVals;
LodChainState *s_lodState[MAX_LOCAL_CLIENTS];
static float s_lastTime[MAX_LOCAL_CLIENTS];
static int s_renderView=-1;
const float c_fadeRate=1;
std::vector<size_t> sizes;
std::vector<void *> allocations;
unsigned char *Hunk_Alloc(size_t size,const char *,int){sizes.push_back(size);void *p=std::malloc(size);allocations.push_back(p);return static_cast<unsigned char *>(p);}
template<class... T> bool Assert_MyHandler(T...){return true;}
struct Step {int view;float dt;};std::vector<Step> steps;
void UpdateChain(int,const float *,float dt,int view){steps.push_back({view,dt});}
'''
for signature in ['int R_WorldLod_SelectRenderView(', 'int R_WorldLod_RenderView(',
    'void __thiscall LodChainState::Init(', 'void __cdecl SetFadeForSortedSurfaces(',
    'void __cdecl R_WorldLod_Init(', 'void __cdecl R_WorldLod_FrameUpdate(']:
    lod+=function('src/gfx_d3d/r_world_lod.cpp',signature)+'\n'
lod+=function('src/gfx_d3d/r_add_bsp.cpp','int __cdecl R_WorldLod_GetLodVal(')+'\n'
lod+=r'''
int main(){
    GfxWorldLodChain chains[1]{};chains[0].lodInfoCount=2;
    GfxWorldLodInfo infos[2]{};infos[1].surfCount=1;
    unsigned surfaces[1]{0},sort[1]{2};world.worldLodChains=chains;world.worldLodInfos=infos;
    world.worldLodSurfaces=surfaces;world.dpvs.sortedSurfIndex=sort;
    R_WorldLod_Init();
    assert(sizes[0]==16&&sizes[1]==sizeof(LodChainState)&&sizes[2]==sizeof(LodChainState));
    assert(s_lodState[0]!=s_lodState[1]&&s_lodVals[2]==0&&s_lodVals[10]==0);
    SetFadeForSortedSurfaces(0.5f,1,0);SetFadeForSortedSurfaces(1.0f,1,1);
    assert(R_WorldLod_GetLodVal(2,0)==127&&R_WorldLod_GetLodVal(2,1)==255);
    float camera[3]{};
    R_WorldLod_FrameUpdate(5,camera,0);
    const int previous=R_WorldLod_SelectRenderView(1);assert(previous==-1);
    assert(R_WorldLod_GetLodVal(2,0)==255);R_WorldLod_FrameUpdate(5,camera,0);
    R_WorldLod_FrameUpdate(5.25f,camera,0);
    R_WorldLod_SelectRenderView(previous);assert(R_WorldLod_GetLodVal(2,0)==127);
    R_WorldLod_FrameUpdate(5.5f,camera,0);
    assert(steps.size()==4&&steps[0].view==0&&steps[1].view==1);
    assert(steps[2].view==1&&steps[2].dt==0.25f&&steps[3].view==0&&steps[3].dt==0.5f);
    R_WorldLod_SelectRenderView(1);R_WorldLod_Init();assert(R_WorldLod_RenderView(0)==0);
    for(void *p:allocations)std::free(p);
    puts("PASS: separate LOD buffers/fades/clocks for two cameras, worker-scoped selection and map-reset initialization");
}
'''
cpp=out/'lod.cpp';cpp.write_text(lod,encoding='utf-8')
exe=out/'lod.exe'
subprocess.run([args.compiler,'-std=c++20','-I'+str(root/'src'),str(cpp),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)

# Deferred material selection must keep the host's shadowed table intact while
# choosing unshadowed guest variants, including fade/charred combinations.
material=r'''
#include <cassert>
#include <cstdio>
constexpr int SHADOW_NONE=0,SHADOW_MAP=1;
using GfxFadeType=int;
struct GfxViewInfo {bool isMissileCamera=false;int dynamicShadowType=SHADOW_MAP;};
struct GfxDrawSurfListInfo {GfxViewInfo *viewInfo=nullptr;};
struct GfxBackEndData {
    bool splitscreen=true;
    struct {unsigned type;} shadowableLights[4];
    unsigned char primaryLightTechType[16][2][2][4]{};
};
struct {unsigned char litTechType[16][8][2][2]{};} gfxDrawMethod;
'''
material+=function('src/gfx_d3d/r_draw_material.cpp','static unsigned char R_GetPrimaryLightTechType(')+'\n'
material+=r'''
int main(){
    GfxBackEndData data;GfxViewInfo host,guest;guest.dynamicShadowType=SHADOW_NONE;
    GfxDrawSurfListInfo hostInfo{&host},guestInfo{&guest};
    for(unsigned surface=0;surface<16;++surface)for(unsigned light=0;light<4;++light)
        for(int fade=0;fade<2;++fade)for(int charred=0;charred<2;++charred){
            data.shadowableLights[light].type=light;
            const unsigned char shadowed=100+surface+fade+charred,plain=10+surface+fade+charred;
            data.primaryLightTechType[surface][fade][charred][light]=shadowed;
            gfxDrawMethod.litTechType[surface][light][fade][charred]=plain;
            assert(R_GetPrimaryLightTechType(&data,&hostInfo,surface,fade,charred,light)==shadowed);
#ifdef KISAK_EXPERIMENTAL_SPLITSCREEN
            assert(R_GetPrimaryLightTechType(&data,&guestInfo,surface,fade,charred,light)==plain);
#else
            assert(R_GetPrimaryLightTechType(&data,&guestInfo,surface,fade,charred,light)==shadowed);
#endif
            assert(data.primaryLightTechType[surface][fade][charred][light]==shadowed);
            data.splitscreen=false;
            assert(R_GetPrimaryLightTechType(&data,&guestInfo,surface,fade,charred,light)==shadowed);
            data.splitscreen=true;guest.isMissileCamera=true;
            assert(R_GetPrimaryLightTechType(&data,&guestInfo,surface,fade,charred,light)==shadowed);
            guest.isMissileCamera=false;
        }
    puts("PASS: host/missile shadow techniques preserved; guest uses unshadowed primary-light variants");
}
'''
cpp=out/'material.cpp';cpp.write_text(material,encoding='utf-8')
for experimental in [False,True]:
    exe=out/('material-split.exe' if experimental else 'material-normal.exe')
    flags=['-DKISAK_EXPERIMENTAL_SPLITSCREEN=1'] if experimental else []
    subprocess.run([args.compiler,'-std=c++20',*flags,str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
