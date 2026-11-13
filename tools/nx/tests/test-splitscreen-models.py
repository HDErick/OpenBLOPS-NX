"""Exercise guest-owned viewmodels and host body submission using actual source functions."""
from pathlib import Path
import argparse
import subprocess
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler',default='/c/msys64/ucrt64/bin/g++.exe')
args=parser.parse_args();root=Path(__file__).resolve().parents[3]
out=root/'build-nx/splitscreen-model-tests';out.mkdir(parents=True,exist_ok=True)
def function(path,signature):
    source=(root/path).read_text(encoding='utf-8',errors='replace')
    start=source.index(signature);end=source.index('{',start)+1;depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]
code=r'''
#include <cassert>
#include <cstring>
#include <cstdio>
#include <vector>
#include <cmath>
#ifndef __cdecl
#define __cdecl
#endif
#define MAX_LOCAL_CLIENTS 2
#define bcassert(x,n) assert(static_cast<unsigned>(x)<(n))
#define __debugbreak() assert(false)
constexpr int ET_PLAYER=1,SCRIPTINSTANCE_SERVER=0;
struct XModel {int id;};XModel hands{1},gun{2},otherHands{3},overrideHands{4};
struct XAnim_s {unsigned size=66;};struct XAnimTree_s {XAnim_s *anims;};
struct DObj {int client,weapon=0;bool alive=true;};DObj objects[2]{{0,1},{1,0}},body{0,0};
XAnim_s animations[2];XAnimTree_s trees[2]{{&animations[0]},{&animations[1]}};
struct ShaderConstantSet {};
struct cpose_t {int eType=0,localClientNum=0;float origin[3]{},angles[3]{},wetness=0;ShaderConstantSet constantSet;};
struct entityState_s {int eType=ET_PLAYER;struct {unsigned eFlags=0;} lerp;};
struct centity_s {entityState_s nextState;cpose_t pose;};
struct playerState_s {int clientNum=1,pm_type=0,weapon=1,viewmodelIndex=0,model=0;unsigned eFlags=0;bool vehicle=false;float viewangles[3]{},velocity[3]{},fWeaponPosFrac=0;};
struct snapshot_s {playerState_s ps;int serverTime=0;};
struct {snapshot_s snap;}clients[2];
const playerState_s *predictedForTest=nullptr;
const playerState_s *CL_SplitGuestPredictedState(){return predictedForTest;}
struct refdef_s {int time=5000;float vieworg[3]{100,200,300},viewaxis[3][3]{{1,0,0},{0,1,0},{0,0,1}};};
struct clientInfo_t {bool infoValid=true;XAnimTree_s *pXAnimTree=&trees[0];};
struct bgs_t {clientInfo_t clientinfo[32];int (*Rand)()=nullptr;float (*Random)()=nullptr;} foreignBgs;
bgs_t *bgs=&foreignBgs;
struct cg_s {
    playerState_s predictedPlayerState;centity_s predictedPlayerEntity;
    snapshot_s *snap=nullptr,*nextSnap=nullptr;
    int localClientNum=0,clientNum=0,time=0,frametime=0;
    refdef_s refdef;float refdefViewAngles[3]{},viewModelAxis[4][3]{},xyspeed=0;
    struct {bool bPositionToADS=false;}playerEntity;
    cpose_t viewModelPose;bgs_t bgs;
} games[2];
cg_s *CG_GetLocalClientGlobals(int n){return &games[n];}
struct weaponInfo_s {XModel *handModel=nullptr,*gogglesModel=nullptr,*rocketModel=nullptr;};
weaponInfo_s weaponStore[2][2048];weaponInfo_s *cg_weaponsArray[2]{weaponStore[0],weaponStore[1]};
weaponInfo_s *CG_GetLocalClientWeaponInfo(int n,int w){return &cg_weaponsArray[n][w];}
struct ViewModelInfo {DObj *viewModelDObj=nullptr;XAnimTree_s *tree=nullptr;XAnim_s *anims=nullptr;};
ViewModelInfo models[2];auto cg_viewModelArray=models;
ViewModelInfo *CG_GetLocalClientViewModelInfo(int n){return &models[n];}
struct WeaponDef {XModel *handXModel=&hands;XModel *gunXModel[2]{&gun,&gun};} definitions[4];
struct {struct {bool enabled=true;} current;} drawGun;auto cg_drawGun=&drawGun;
int BG_GetViewmodelWeaponIndex(const playerState_s *ps){return ps->weapon;}
bool BG_UsingVehicleWeapon(const playerState_s *ps){return ps->vehicle;}
int BG_GetNumWeapons(){return 4;}
const WeaponDef *BG_GetWeaponDef(int n){return &definitions[n];}
unsigned char BG_GetPlayerWeaponModel(const playerState_s *ps,int){return ps->model;}
char *CL_GetConfigString(int n){static char configured[]="guest_hands",empty[]="";return n==1569?configured:empty;}
XModel *R_RegisterModel(char *){return &overrideHands;}
void CG_RegisterWeapon(int n,int){assert(n==1);}
int rebuilds=0,updates=0,frees=0,freedClient=-1;
int CG_WeaponDObjHandle(int);
bool NeedToRebuildViewmodel(int n,int w,unsigned char,const ViewModelInfo *m,weaponInfo_s *){return !m->viewModelDObj||m->viewModelDObj->weapon!=w;}
void ChangeViewmodelDobj(int n,const playerState_s *,int w,unsigned char,XModel *h,XModel *,XModel *,bool,bool){
    assert(n==1&&CG_WeaponDObjHandle(n)<1537);++rebuilds;
    objects[n].weapon=w;objects[n].alive=true;
    models[n]={&objects[n],&trees[n],&animations[n]};weaponStore[n][w].handModel=h;
}
void WeaponRunXModelAnims(int n,const playerState_s *ps,ViewModelInfo *m,int){assert(n==1&&bgs==&games[1].bgs&&m->viewModelDObj->weapon==ps->weapon);}
void DObjUpdateClientInfo(DObj *obj,float dt,int flags){assert(obj->client==1&&dt>=0&&flags==0);++updates;}
void CG_TouchViewModels(int n){assert(n==1);}
void CalculateWeaponPostion_PositionToADS(cg_s *,playerState_s *){}
struct weaponState_t {};struct GfxScaledPlacement {struct {float origin[3],quat[4];} base;};
void CG_BuildWeaponState(cg_s *g,weaponState_t *){assert(g==&games[1]);}
void BG_CalculateWeaponMovement(weaponState_t *,float *org,float *angles){org[0]=2;org[1]=3;org[2]=4;std::memset(angles,0,12);}
void CG_CalculateWeaponMovement_ClientSpecific(cg_s *g,GfxScaledPlacement *p,float *){for(int i=0;i<3;++i)p->base.origin[i]+=g->refdef.vieworg[i];}
void CG_SaveWeaponState(weaponState_t *,cg_s *){}
void UnitQuatToAxis(const float *,float axis[4][3]){for(int i=0;i<3;++i)for(int j=0;j<3;++j)axis[i][j]=i==j;}
void CG_UpdateViewModelPose(DObj *,int n){std::memcpy(games[n].viewModelPose.origin,games[n].viewModelAxis[3],12);}
struct Submission {DObj *obj;const cpose_t *pose;int entity;unsigned flags;float light[3];};std::vector<Submission> submissions;
void R_AddDObjToScene(DObj *obj,const cpose_t *pose,unsigned ent,unsigned flags,float *light,float,float,float,float,int,int,const ShaderConstantSet *,const ShaderConstantSet *,float,float){
    Submission s{obj,pose,int(ent),flags,{}};std::memcpy(s.light,light,12);submissions.push_back(s);
}
void Com_SafeClientDObjFree(unsigned handle,int n){assert(handle==1536);objects[n].alive=false;++frees;freedClient=n;}
void XAnimFreeTree(XAnimTree_s *tree,int,int){assert(tree==&trees[1]);}
template<class...T> bool Assert_MyHandler(T...){return false;}
DObj *Com_GetClientDObj(int ent,int n){assert(n==0&&ent==0);return body.alive?&body:nullptr;}
int bodyAnimations=0,links=0;
void BG_PlayerAnimation(int n,const entityState_s *,clientInfo_t *){assert(n==0&&bgs==&games[0].bgs);++bodyAnimations;}
void CG_Player_PreControllers(DObj *obj,centity_s *){assert(obj==&body&&bgs==&games[0].bgs);}
void CG_UpdateWeaponVisibilityImmediate(int n,centity_s *){assert(n==0&&bgs==&games[0].bgs);}
float DObjGetRadius(DObj *){return 24;}
void R_LinkDObjEntity(int n,int ent,float *,float radius){assert(n==0&&ent==0&&radius==40);++links;}
'''
for sig in ['int __cdecl CG_WeaponDObjHandle(', 'void __cdecl CG_FreeWeapons(',
    'bool CG_SplitGuestAddViewmodel(', 'void CG_SplitGuestFreeViewmodel()']:
    code+=function('src/cgame/cg_weapons.cpp',sig)+'\n'
code+=function('src/cgame_mp/cg_players_mp.cpp','bool CG_SplitGuestAddHostBody()')+'\n'
code+=r'''
int main(){
    models[0]={&objects[0],&trees[0],&animations[0]};weaponStore[0][1].handModel=&hands;
    games[0].predictedPlayerState.clientNum=0;games[0].nextSnap=new snapshot_s;
    games[0].predictedPlayerEntity.pose.origin[0]=77;
    const cg_s savedHost=games[0];const ViewModelInfo savedModel=models[0];
    assert(CG_WeaponDObjHandle(0)==1536&&CG_WeaponDObjHandle(1)==1536);
    assert(CG_WeaponDObjHandle(0)!=CG_WeaponDObjHandle(1)+1537); // different client map keys
    refdef_s view;
    assert(CG_SplitGuestAddViewmodel(&view,16));
    assert(rebuilds==1&&updates==1&&bgs==&foreignBgs);
    assert(models[1].viewModelDObj!=models[0].viewModelDObj&&models[1].tree!=models[0].tree);
    const auto &s=submissions.back();assert(s.obj==&objects[1]&&s.entity==1023&&s.flags==0x400002);
    assert(s.pose->eType==21&&s.pose->localClientNum==1&&s.pose->origin[0]==102&&s.pose->origin[1]==203&&s.pose->origin[2]==304);
    assert(s.light[0]==100&&games[1].snap==games[1].nextSnap&&games[1].snap->ps.weapon==1);
    assert(!std::memcmp(&savedHost,&games[0],sizeof(savedHost))&&!std::memcmp(&savedModel,&models[0],sizeof(savedModel)));
    assert(CG_SplitGuestAddViewmodel(&view,16)&&rebuilds==1&&updates==2);
    definitions[2].handXModel=&otherHands;games[1].predictedPlayerState.weapon=2;
    assert(CG_SplitGuestAddViewmodel(&view,16)&&rebuilds==2&&models[1].viewModelDObj->weapon==2);
    games[1].predictedPlayerState.viewmodelIndex=1;
    assert(CG_SplitGuestAddViewmodel(&view,16)&&weaponStore[1][2].handModel==&overrideHands&&rebuilds==3);
    games[1].predictedPlayerState.pm_type=4;assert(!CG_SplitGuestAddViewmodel(&view,16));
    games[1].predictedPlayerState.pm_type=0;games[1].predictedPlayerState.eFlags=0x100;assert(!CG_SplitGuestAddViewmodel(&view,16));
    games[1].predictedPlayerState.eFlags=0;games[1].predictedPlayerState.vehicle=true;assert(!CG_SplitGuestAddViewmodel(&view,16));
    games[1].predictedPlayerState.vehicle=false;drawGun.current.enabled=false;assert(!CG_SplitGuestAddViewmodel(&view,16));drawGun.current.enabled=true;
    games[1].predictedPlayerState.weapon=0;assert(!CG_SplitGuestAddViewmodel(&view,16));
    games[1].predictedPlayerState.weapon=99;assert(!CG_SplitGuestAddViewmodel(&view,16));
    // The HUD may have overwritten cg's PS: the gun must recover prediction,
    // while its snap/nextSnap retain the authoritative weapon and ADS fraction.
    playerState_s predicted=clients[1].snap.ps;predicted.weapon=2;predicted.velocity[0]=3;predicted.velocity[1]=4;predicted.fWeaponPosFrac=.8f;
    predictedForTest=&predicted;clients[1].snap.serverTime=1234;
    assert(CG_SplitGuestAddViewmodel(&view,16));
    assert(games[1].predictedPlayerState.weapon==2&&games[1].snap->ps.weapon==1&&games[1].snap->serverTime==1234);
    assert(games[1].xyspeed==5&&games[1].playerEntity.bPositionToADS);
    predictedForTest=nullptr;
    assert(CG_SplitGuestAddHostBody()&&bodyAnimations==1&&links==1&&bgs==&foreignBgs);
    const auto &h=submissions.back();assert(h.obj==&body&&h.entity==0&&h.flags==0x400024&&h.light[0]==77&&h.light[2]==32);
    body.alive=false;assert(!CG_SplitGuestAddHostBody());body.alive=true;
    games[0].predictedPlayerState.pm_type=6;assert(CG_SplitGuestAddHostBody());games[0].predictedPlayerState.pm_type=7;assert(CG_SplitGuestAddHostBody());games[0].predictedPlayerState.pm_type=4;assert(!CG_SplitGuestAddHostBody());games[0].predictedPlayerState.pm_type=0;
    games[0].predictedPlayerEntity.nextState.lerp.eFlags=0x20;assert(!CG_SplitGuestAddHostBody());games[0].predictedPlayerEntity.nextState.lerp.eFlags=0;
    CG_SplitGuestFreeViewmodel();assert(frees==1&&freedClient==1&&!objects[1].alive&&objects[0].alive);
    assert(models[1].viewModelDObj==nullptr&&models[1].tree==nullptr&&games[1].snap==nullptr&&games[1].nextSnap==nullptr);
    assert(models[0].viewModelDObj==savedModel.viewModelDObj&&weaponStore[0][1].handModel==&hands);
    cg_viewModelArray=nullptr;CG_SplitGuestFreeViewmodel();assert(frees==1);
    delete games[0].nextSnap;
    puts("PASS: guest weapon/hands/animation ownership, valid per-client handles, camera placement and independent cleanup");
    puts("PASS: host body submission, visibility gates and scoped animation context; host first-person state preserved");
}
'''
cpp=out/'models.cpp';cpp.write_text(code,encoding='utf-8');exe=out/'models.exe'
subprocess.run([args.compiler,'-std=c++20',str(cpp),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
