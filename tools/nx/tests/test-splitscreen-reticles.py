"""Exercise native crosshair rendering/gating and spread using the guest's camera and player state."""
from pathlib import Path
import argparse, subprocess
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--compiler',default='/c/msys64/ucrt64/bin/g++.exe');args=p.parse_args()
root=Path(__file__).resolve().parents[3];out=root/'build-nx/splitscreen-reticle-tests';out.mkdir(parents=True,exist_ok=True)
def function(n,sig):
 s=(root/n).read_text();a=s.index(sig);b=s.index('{',a)+1;depth=1
 while depth:depth+=(s[b]=='{')-(s[b]=='}');b+=1
 return s[a:b]
code=r"""
#include <cassert>
#include <cmath>
#include <cstdio>
#define __cdecl
#define __debugbreak() assert(false)
#define PROF_SCOPED(x)
constexpr int TP_FOR_MODEL=1,WEAPOVERLAYRETICLE_NONE=0;
struct dvar_s{struct{bool enabled=true;int integer=0;}current;};
dvar_s gun,dynamic,swimming,paused,drawpaused,alpha;
auto cg_drawGun=&gun;auto cg_crosshairDynamic=&dynamic;auto show_reticle_during_swimming=&swimming;auto cg_paused=&paused;auto cg_drawpaused=&drawpaused;
struct WeaponVariantDef{void *overlayMaterial=nullptr;float fAdsZoomInFrac=.5f,fAdsZoomOutFrac=.5f;};
struct WeaponDef{bool bKeepCrosshairWhenADS=false,bDualWield=false;int overlayReticle=0,iReticleMinOfs=0;float fHipReticleSidePos=.5f;};
WeaponDef definitions[4];WeaponVariantDef variants[4];
struct playerState_s{int clientNum=0,weapon=1,weaponstate=0,weaponstateLeft=0,eFlags=0,eFlags2=0,pm_type=0,vehicleType=0,waterlevel=0,viewlocked_entNum=1023,locationSelectionInfo=0;float fWeaponPosFrac=0,aimSpreadScale=0;};
struct snapshot_s{playerState_s ps;};
struct cg_s{playerState_s predictedPlayerState;snapshot_s *nextSnap=nullptr;int localClientNum=0,cameraMode=0,renderingThirdPerson=0;float gunYaw=0,gunPitch=0,refdefViewAngles[3]{};struct{float vieworg[3]{},viewaxis[3][3]{{1,0,0},{0,1,0},{0,0,1}},tanHalfFovX=1,tanHalfFovY=.75f;}refdef;struct{bool bPositionToADS=false;}playerEntity;}games[2];
struct cgs_t{};
cg_s *CG_GetLocalClientGlobals(int n){return &games[n];}cgs_t *CG_GetLocalClientStaticGlobals(int){return nullptr;}
const WeaponDef*BG_GetWeaponDef(int n){return &definitions[n];}const WeaponVariantDef*BG_GetWeaponVariantDef(int n){return &variants[n];}
int BG_GetViewmodelWeaponIndex(const playerState_s*ps){return ps->weapon;}
void AngleVectors(const float*a,float*f,void*,void*){float y=a[1]*3.14159265f/180,p=a[0]*3.14159265f/180;f[0]=cos(p)*cos(y);f[1]=cos(p)*sin(y);f[2]=-sin(p);}
bool CG_KillCamEntityEnabled(int){return false;}bool CG_IsShowingZombieMap(){return true;}bool CG_IsReticleTurnedOff(int){return false;}bool Demo_IsGameHudHidden(){return false;}
bool flash=false,flare=false,hud=true,hardcore=false;bool CG_Flashbanged(int){return flash;}bool CG_Flared(int){return flare;}bool CG_ShouldDrawHud(int){return hud;}bool IsHardcoreMode(int){return hardcore;}
bool R_StereoActivated(){return false;}
struct trace_t{float fraction=1;};void CG_LocationalTrace(trace_t*,const float*,const float*,int,int,int,int){}
void Vec3Lerp(const float*a,const float*b,float t,float*c){for(int i=0;i<3;++i)c[i]=a[i]+t*(b[i]-a[i]);}
float Vec3Distance(const float*a,const float*b){float n=0;for(int i=0;i<3;++i)n+=(a[i]-b[i])*(a[i]-b[i]);return sqrt(n);}
float colorWhite[4]{1,1,1,1},vec2_origin[2]{};
template<class...T>bool Assert_MyHandler(T...){return false;}
void CG_DrawVehicleCrossHair(int){}void CG_DrawTurretCrossHair(int){}int CG_PlayerTurretWeaponIdx(int){return 0;}
int overlays=0,centers=0,sides=0,lastWeapon=0;float lastX=0,lastY=0,lastSpread=0;
void CG_DrawAdsOverlay(int n,int,const float*,const float*){assert(n==1);++overlays;}
void CG_CalcCrosshairColor(int n,float,float*c){assert(n==1);c[3]=1;}
void CG_TransitionToAds(const cg_s*,const WeaponDef*,float,float*,float*){}
void CG_DrawAdsAimIndicator(int,const WeaponDef*,const float*,float,float,float){}
void CG_DrawReticleCenter(int n,const WeaponDef*,const float*,float x,float y,float,float){assert(n==1);++centers;lastX=x;lastY=y;}
void BG_GetSpreadForWeapon(const playerState_s*,const WeaponDef*,float*lo,float*hi){*lo=1;*hi=6;}
"""
for sig in ['void __cdecl CG_CalcCrosshairPosition(', 'char __cdecl CG_GetWeapReticleZoom(', 'bool __cdecl ShouldDrawCrosshair(', 'bool __cdecl AllowedToDrawCrosshair(', 'void __cdecl CG_CalcReticleSpread(']:
 code+=function('src/cgame/cg_draw_reticles.cpp',sig)+'\n'
code+=r"""
double CG_DrawWeapReticle(int n){float zoom=0;if(CG_GetWeapReticleZoom(&games[n],&zoom)){CG_DrawAdsOverlay(n,games[n].predictedPlayerState.weapon,colorWhite,vec2_origin);return 1-zoom;}return 1;}
void CG_DrawReticleSides(int n,int weapon,const float*,float,float,float,float){assert(n==1);++sides;lastWeapon=weapon;float size[2]{4,4},spread[2];CG_CalcReticleSpread(&games[n],&definitions[weapon],size,1,spread);lastSpread=spread[0];}
"""
code+=function('src/cgame/cg_draw_reticles.cpp','void __cdecl CG_DrawCrosshair(')+'\n'
code+=r"""
int main(){
 games[0].predictedPlayerState.weapon=1;games[0].predictedPlayerState.fWeaponPosFrac=1;
 auto &g=games[1];auto &ps=g.predictedPlayerState;g.localClientNum=1;ps.weapon=2;
 // Guest aim uses its own yaw/camera, not P1's ADS or weapon.
 g.gunYaw=90;g.refdefViewAngles[1]=90;float angles[3]{0,90,0};AngleVectors(angles,g.refdef.viewaxis[0],nullptr,nullptr);
 g.refdef.viewaxis[1][0]=-1;g.refdef.viewaxis[1][1]=0;g.refdef.viewaxis[2][2]=1;
 CG_DrawCrosshair(1);assert(centers==1&&sides==1&&lastWeapon==2&&std::abs(lastX)<.001&&std::abs(lastY)<.001);float tight=lastSpread;
 ps.aimSpreadScale=255;CG_DrawCrosshair(1);assert(lastSpread>tight);
 auto unchanged=[&](){int before=centers;CG_DrawCrosshair(1);assert(centers==before);};
 ps.fWeaponPosFrac=1;unchanged();ps.fWeaponPosFrac=0;
 ps.weaponstate=10;unchanged();ps.weaponstate=0;ps.weapon=0;unchanged();ps.weapon=2;
 flash=true;unchanged();flash=false;flare=true;unchanged();flare=false;
 hud=false;unchanged();hud=true;hardcore=true;unchanged();hardcore=false;
 g.renderingThirdPerson=TP_FOR_MODEL;unchanged();g.renderingThirdPerson=0;
 ps.weaponstate=47;unchanged();ps.weaponstate=0;
 variants[2].overlayMaterial=(void*)1;ps.fWeaponPosFrac=1;g.playerEntity.bPositionToADS=true;unchanged();assert(overlays==1);
 ps.fWeaponPosFrac=0;CG_DrawCrosshair(1);assert(lastWeapon==2);
 assert(games[0].predictedPlayerState.weapon==1&&games[0].predictedPlayerState.fWeaponPosFrac==1);
 puts("Native guest crosshair camera/spread, ADS overlay and suppression checks passed");
}
"""
cpp=out/'reticles.cpp';cpp.write_text(code);exe=cpp.with_suffix('.exe');subprocess.run([args.compiler,'-std=c++17','-w',str(cpp),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)

vehicle = r"""
#include <cassert>
#include <cstdio>
#ifndef __cdecl
#define __cdecl
#endif
using _WORD = unsigned short;
struct playerState_s { int vehiclePos = 0, viewlocked_entNum = 1; };
struct cg_s {
    playerState_s predictedPlayerState;
    struct { struct { int otherEntityNum = 1; } nextState; } predictedPlayerEntity;
} cg;
struct centity_s {
    struct { int weapon = 1; struct { int vehicleInfoIndex = 0; } vehicleState; } nextState;
    struct { struct { struct {
        short gunYaw = 0, gunPitch = 0;
        struct { short yaw, pitch; } gunnerAngles[4]{};
    } vehicle; } u; } currentState;
} cent;
struct vehicle_info_t { int driverControlledGunPos = -1; char gunnerWeapon[4][64]{}; } info;
struct WeaponDef { float leftArc = 10, rightArc = 5, topArc = 90, bottomArc = 90; } def;
float s_arcEpsilon = 0.01f;
bool CG_IsVehicleMayhemGameType() { return false; }
const cg_s *CG_GetLocalClientGlobals(int) { return &cg; }
int CG_GetPlayerWeapon(const playerState_s *, int) { return 1; }
const WeaponDef *BG_GetWeaponDef(int) { return &def; }
centity_s *CG_GetEntity(int, int) { return &cent; }
const vehicle_info_t *CG_GetVehicleInfo(int) { return &info; }
""" + function('src/cgame/cg_draw_reticles.cpp', 'double __cdecl CG_GetVehicleCrossHairAlpha(') + r"""
int main() {
    assert(CG_GetVehicleCrossHairAlpha(0) == 1);
    cent.currentState.u.vehicle.gunYaw = 900;
    assert(CG_GetVehicleCrossHairAlpha(0) == 1);
    cent.currentState.u.vehicle.gunYaw = -900;
    assert(CG_GetVehicleCrossHairAlpha(0) == 1);
    cent.currentState.u.vehicle.gunYaw = 1820;
    assert(CG_GetVehicleCrossHairAlpha(0) == 0);
    cent.currentState.u.vehicle.gunYaw = -920;
    assert(CG_GetVehicleCrossHairAlpha(0) == 0);
    cent.currentState.u.vehicle.gunYaw = 0;
    cent.currentState.u.vehicle.gunPitch = 17000;
    assert(CG_GetVehicleCrossHairAlpha(0) == 0);
    puts("PASS: ZPU reticle is visible inside its horizontal and vertical aiming arcs");
}
"""
cpp = out / 'vehicle-reticle.cpp'
cpp.write_text(vehicle)
exe = cpp.with_suffix('.exe')
subprocess.run([args.compiler, '-std=c++17', str(cpp), '-o', str(exe)], check=True)
subprocess.run([str(exe)], check=True)
