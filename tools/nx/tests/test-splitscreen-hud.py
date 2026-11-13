"""Exercise guest HUD data ownership, damage transitions, screen placement and split HUD projection."""
from pathlib import Path
import argparse,re,subprocess
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--compiler',default='/c/msys64/ucrt64/bin/g++.exe');args=p.parse_args()
root=Path(__file__).resolve().parents[3];out=root/'build-nx/splitscreen-hud-tests';out.mkdir(parents=True,exist_ok=True)
def read(n):return (root/n).read_text()
def function(n,sig):
 s=read(n);a=s.index(sig);b=s.index('{',a)+1;depth=1
 while depth:depth+=(s[b]=='{')-(s[b]=='}');b+=1
 return s[a:b]
def clean(s):return re.sub(r'^\s*#(?:include|pragma).*\n','',s,flags=re.M)
code=r"""
#include <cassert>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>
#ifndef __cdecl
#define __cdecl
#endif
#define MAX_LOCAL_CLIENTS 2
#define KISAK_SP 1
#define KISAK_EXPERIMENTAL_SPLITSCREEN 1
#define PROF_SCOPED(x)
#define __debugbreak() assert(false)
#define iassert(x) assert(x)
struct Material {};struct Font_s{}font;float colorWhite[4]{1,1,1,1};
"""
h=read('src/client/screen_placement.h');code+=h[h.index('struct ScreenPlacement //'):h.index('struct ScreenPlacementStack')]
code+=r"""
ScreenPlacement scrPlaceView[2];
struct {struct{int displayWidth=1280,displayHeight=721;float aspectRatioScenePixel=1;}vidConfig;}cls;
struct {bool renderHiResShot=false;int hiResShotTiles=1,hiResShotCol=0,hiResShotRow=0;}rg;
template<class... T> bool Assert_MyHandler(T...){return false;}
const char *va(const char *fmt,...){static char text[256];va_list a;va_start(a,fmt);vsnprintf(text,sizeof(text),fmt,a);va_end(a);return text;}
void ScrPlace_CalcSafeAreaOffsets(float,float,float,float,float,float,float,float,float*,float*,float*,float*);
struct hudelem_s {int type=0,flags=0;float value=0,sort=0;};
struct playerState_s {int stats[5]{100,0,100,0,0};int clientNum=1,pm_type=0,cursorHint=0,cursorHintString=0,otherFlags=0,eFlags=0,eFlags2=0,weapon=1,weaponstate=0,damageEvent=0,damageCount=0,damageYaw=0,damagePitch=0,clip=8,reserve=32;unsigned perks[2]{};struct{hudelem_s current[31],archival[31];}hud;float fWeaponPosFrac=0,viewangles[3]{};};
struct clientState_s {int clientIndex=0;};struct snapshot_s {playerState_s ps;int serverTime=1000,numClients=0;clientState_s clients[32];};
struct clientInfo_t{bool infoValid=false;struct{int score=0;}score;};
struct rectDef_s{float x=0,y=0,w=0,h=0;int horzAlign=0,vertAlign=0;};
struct refdef_s {unsigned x=0,y=360,width=1280,height=361;int time=1000;float vieworg[3]{},tanHalfFovX=1,tanHalfFovY=1;float viewaxis[3][3]{{1,0,0},{0,1,0},{0,0,1}};};
struct cg_s {int healthOverlayPulseIndex=0,healthOverlayPulsePhase=0,healthOverlayPulseTime=0,healthOverlayLastHitTime=0,healthOverlayPulseDuration=0,healthOverlayHurt=0;
float healthOverlayFromAlpha=0,healthOverlayToAlpha=0,healthOverlayOldHealth=1;
struct{clientInfo_t clientinfo[32];}bgs;snapshot_s *snap=nullptr,*nextSnap=nullptr;int weaponSelect=0,ammoFadeTime=0,weaponSelectTime=0,stanceFadeTime=0,renderingThirdPerson=0,cursorHintTime=0,cursorHintFade=0,cursorHintIcon=0,cursorHintString=0,localClientNum=0,clientNum=0,time=0,hudTime=0,cameraMode=0,v_dmg_time=0,damageTime=0;float v_dmg_roll=0,v_dmg_pitch=0;float refdefViewAngles[3]{},gunPitch=0,gunYaw=0;struct{bool bPositionToADS=false;}playerEntity;bool drawHud=false,inKillCam=false;struct{int time=0,duration=0;float yaw=0;}viewDamage[8];playerState_s predictedPlayerState;refdef_s refdef;}games[2];
cg_s *CG_GetLocalClientGlobals(int n){assert(n>=0&&n<2);return &games[n];}
struct{snapshot_s snap;}clients[2];
struct dvar_t {struct{float value=1;int integer=1000;bool enabled=true;}current;}kickScale,kickMin,kickMax,reduction,iconTime,iconScope,iconWidth,iconHeight,iconOffset;
auto bg_viewKickScale=&kickScale;auto bg_viewKickMin=&kickMin;auto bg_viewKickMax=&kickMax;auto perk_damageKickReduction=&reduction;
auto cg_hudDamageIconTime=&iconTime;auto cg_hudDamageIconInScope=&iconScope;auto cg_hudDamageIconWidth=&iconWidth;auto cg_hudDamageIconHeight=&iconHeight;auto cg_hudDamageIconOffset=&iconOffset;
using dvar_s=dvar_t;dvar_t itemHeight,scoreFont,textOffset;auto cg_scoreboardItemHeight=&itemHeight;auto cg_scoreboardFont=&scoreFont;auto cg_scoreboardTextOffset=&textOffset;dvar_t hintFade,seatFade;auto cg_hintFadeTime=&hintFade;auto cg_seatHintFadeTime=&seatFade;constexpr int TP_FOR_MODEL=1,LOCMSG_SAFE=0;
const dvar_s *Dvar_FindVar(const char *){return nullptr;}
float Dvar_GetFloat(const char *){return .8f;}void Dvar_GetUnpackedColor(const dvar_s*,float *){}
void Com_sprintf(char *out,int size,const char *fmt,...){va_list a;va_start(a,fmt);vsnprintf(out,size,fmt,a);va_end(a);}
Material scoreMaterial;Material *Material_RegisterHandle(const char *,int){return &scoreMaterial;}
int R_TextHeight(Font_s *){return 32;}
bool guestConnected=true;
bool draw2D=true,drawHud=true,menu=false,split=true;int menuNotifications=0,damageQuads=0;float flashY=0,flashH=0,arrowY=0;
bool TestDvarRegistered(const char*);
bool Dvar_GetBool(const char *n){
 if(!TestDvarRegistered(n))return false;
 if(!std::strcmp(n,"cg_draw2D"))return draw2D;
#ifndef KISAK_SP
 if(!std::strcmp(n,"cg_drawHUD"))return false;
#endif
 return drawHud;
}
void CG_SplitGuestResetMpHud(){}
void CG_SplitGuestUpdateMpHud(snapshot_s*){}
void CG_SplitGuestDrawMpHudText(){}
bool guestMenuOpen=false;int guestMenusDrawn=0;
bool CL_SplitGuestOwnsMenu(){return guestMenuOpen;}
void CL_SplitGuestDrawMenu(){if(guestMenuOpen)++guestMenusDrawn;}

bool CL_IsSplitGuest(int n){return n==1&&guestConnected;}bool CL_SplitGuestHasView(){return split;}
void CG_MenuShowNotify(int n,int){assert(n==0);++menuNotifications;}

bool Demo_IsThirdPersonCamera(){return false;}bool Demo_IsMovieCamera(){return false;}bool Demo_IsPlaying(){return false;}bool Demo_IsGameHudHidden(){return false;}
bool CG_Flashbanged(int){return false;}bool CG_IsTvguided(int,bool){return false;}bool CG_GetWeapReticleZoom(const cg_s *,float *){return false;}
float AngleNormalize360(float a){return std::fmod(a+360,360);}
void AngleVectors(const float *,float *v,void*,void*){v[0]=1;v[1]=0;v[2]=0;}
float vectoyaw(const float *){return 0;}float testRandom(){return .5f;}
#define random testRandom
void CL_GetScreenDimensions(int *w,int *h,float *a){*w=1280;*h=721;*a=1280.0f/721;}
void UI_FillRectPhysical(float,float y,float,float h,const float *){flashY=y;flashH=h;++damageQuads;}
float ScrPlace_HiResGetScale(){return 1;}
void CG_CalcCrosshairPosition(const cg_s *,float*,float*){}
struct{Material *damageMaterial=nullptr,*whiteMaterial=nullptr;}cgMedia;
void CG_DrawRotatedQuadPic(const ScreenPlacement *,float,float y,float[4][2],float,const float*,Material*){arrowY=y;}
struct uiInfo_s {struct{int openMenuCount=0;}uiDC;}sharedUI;
#define bcassert(n,max) assert((n)<(max))
struct{int uiContextIndex=0;}clientGameStates[MAX_LOCAL_CLIENTS];

int Com_LocalClient_GetUIContextIndex(int);
uiInfo_s *UI_UIContext_GetInfo(int);
bool UI_AnyMenuVisible(int n);
bool CL_SplitGuestOwnsMenu();
std::vector<float> values;std::vector<std::string> commands,texts;
void DrawSingleHudElem2d(int n,const hudelem_s *e){assert(n==1);values.push_back(e->value);}
int GetSortedDebugHudElems(hudelem_s **){return 0;}
void CG_Draw2dHudElemList(hudelem_s **,int,int,int,bool,bool);
void R_BeginClientCmdList2D(){commands.push_back("begin");}
void R_AddCmdProjectionSet2D(){commands.push_back("projection");}
void R_AddCmdSetScissorValues(bool active,int,int y,int,int h){if(active)assert(y==360&&h==361);commands.push_back(active?"clip":"unclip");}
void R_AddCmdEndOfList(){commands.push_back("end");}
bool CL_SplitGuestGetViewAngles(float *a){a[0]=0;a[1]=45;a[2]=0;return true;}
unsigned BG_GetViewmodelWeaponIndex(const playerState_s *ps){return ps->weapon;}int BG_GetNumWeapons(){return 10;}
int BG_GetTotalAmmoReserve(const playerState_s *ps,unsigned){return ps->reserve;}
bool BG_WeaponIsClipOnly(unsigned weapon){return weapon==2;}int BG_GetAmmoInClip(const playerState_s *ps,unsigned){return ps->clip;}
Font_s *UI_GetFontHandle(const ScreenPlacement *sp,int,float){assert(sp==&scrPlaceView[0]||sp==&scrPlaceView[1]);return &font;}
float UI_TextWidth(const char *s,int,Font_s *,float){return std::strlen(s)*10.0f;}
void ScrPlace_ApplyRect(const ScreenPlacement *,float*,float*,float*,float*,int,int);
std::vector<float> textY;int hintCalls=0;std::vector<int> scoreViews;
void UI_DrawText(const ScreenPlacement *sp,char *t,int,Font_s *,float x,float y,int ha,int va,float scale,const float*,int){float w=1,h=1;ScrPlace_ApplyRect(sp,&x,&y,&w,&h,ha,va);assert(y>=sp->realViewportBase[1]&&y<=sp->realViewportBase[1]+sp->realViewportSize[1]);texts.emplace_back(t);textY.push_back(y);}
void UI_DrawHandlePic(const ScreenPlacement *sp,float x,float y,float w,float h,int ha,int va,const float*,Material*){ScrPlace_ApplyRect(sp,&x,&y,&w,&h,ha,va);assert(y>=sp->realViewportBase[1]&&y+h<=sp->realViewportBase[1]+sp->realViewportSize[1]);scoreViews.push_back(sp==&scrPlaceView[1]?1:0);}
void CG_SplitGuestResetNames(){}
int crosshairPasses=0;void CG_DrawCrosshair(int n){assert(n==1&&games[n].gunYaw==45&&games[n].refdef.y==360);++crosshairPasses;}
int revivePasses=0;void CG_DrawReviveIndicators_SP(int n){assert(n==1);++revivePasses;}
void CG_UpdateCursorHints(cg_s *);
const char *CL_GetConfigString(int index){assert(index==422);return "Hold &&1 to open door";}
int bindingReads=0;bool UI_GetKeyBindingLocalizedString(int n,const char *,char *out,int){assert(n==0);++bindingReads;strcpy(out,"X");return true;}
const char *UI_SafeTranslateString(const char *s){return s;}
void I_strncpyz(char *out,const char *in,int n){snprintf(out,n,"%s",in);}
const char *SEH_LocalizeTextMessage(const char *s,const char *,int){return s;}
bool SEH_ReplaceDirectiveInStringWithBinding(int n,const char *,char *){assert(n==0);return false;}
const char *UI_ReplaceConversionString(const char *,const char *b){assert(!strcmp(b,"X"));return "Hold X to open door";}
void CG_DrawCursorhint(int n,const rectDef_s *r,Font_s*,float,float*,int){assert(n==1&&games[1].nextSnap==games[1].snap&&r->horzAlign==2&&r->vertAlign==2);CG_UpdateCursorHints(&games[1]);++hintCalls;}
"""
for sig in ['void __cdecl ScrPlace_CalcSafeAreaOffsets(', 'void __cdecl ScrPlace_SetupFloatViewport(', 'double __cdecl ScrPlace_ApplyX(', 'double __cdecl ScrPlace_ApplyY(', 'void __cdecl ScrPlace_ApplyRect(']:
 code+=function('src/client/screen_placement.cpp',sig)+'\n'
for sig in ['void __cdecl Com_LocalClients_AssignUIContextsForInGame(', 'void __cdecl Com_LocalClients_AssignUIContextsForFrontEnd(', 'int __cdecl Com_LocalClient_GetUIContextIndex(']:
 code+=function('src/qcommon/com_clients.cpp',sig)+'\n'
code+=function('src/ui/ui_main.cpp','uiInfo_s *__cdecl UI_UIContext_GetInfo(').replace('return &uiInfoArray[contextIndex];','return &sharedUI;')+'\n'
code+=function('src/ui/ui_main.cpp','uiInfo_s *__cdecl UI_GetInfo(')+'\n'
code+=function('src/ui/ui_main.cpp','bool __cdecl UI_AnyMenuVisible(')+'\n'
code+=r"""
dvar_t corners,cornerRadius,debugWaypoints;auto waypointOffscreenRoundedCorners=&corners;auto waypointOffscreenCornerRadius=&cornerRadius;auto waypointDebugDraw=&debugWaypoints;
float colorYellow[4]{1,1,0,1},colorGreen[4]{0,1,0,1};
float Vec2Length(const float *v){return std::sqrt(v[0]*v[0]+v[1]*v[1]);}
float Vec2Normalize(float *v){float n=Vec2Length(v);if(n){v[0]/=n;v[1]/=n;}return n;}
void CL_DrawStretchPicPhysical(float,float,float,float,float,float,float,float,const float *,Material *){assert(false);}
void GetViewAxisProjections(const float *origin,const float (*axis)[3],const float *target,float *out){for(int i=0;i<3;++i){out[i]=0;for(int j=0;j<3;++j)out[i]+=(target[j]-origin[j])*axis[i][j];}}
"""
for sig in ['char __cdecl WorldPosToScreenPos(', 'bool __cdecl ClampScreenPosToEdges(']:
 code+=function('src/cgame/cg_hudelem.cpp',sig)+'\n'
for sig in ['void __cdecl CopyInUseHudElems(', 'int __cdecl compare_hudelems(', 'int __cdecl GetSortedHudElems(', 'void __cdecl CG_Draw2dHudElemList(', 'void __cdecl CG_Draw2dHudElems(']:
 code+=function('src/cgame/cg_hudelem.cpp',sig)+'\n'
# Use the native blood-overlay state machine and draw path with the guest PS.
health_functions=['double __cdecl CG_CalcPlayerHealth(', 'void __cdecl CG_ResetLowHealthOverlay(', 'double __cdecl CG_FadeLowHealthOverlay(', 'void __cdecl CG_PulseLowHealthOverlay(', 'void __cdecl CG_DrawPlayerLowHealthOverlay(']
health_source='\n'.join(function('src/cgame_mp/cg_newDraw_mp.cpp',sig) for sig in health_functions)
health_dvars=sorted(set(re.findall(r'hud_healthOverlay_\w+',health_source)))
code+=''.join('dvar_t object_'+n+';auto '+n+'=&object_'+n+';\n' for n in health_dvars)
code+=r"""
bool CG_KillCamEntityEnabled(int){return false;}
float lowHealthY=0,lowHealthH=0;int lowHealthDraws=0;
void CL_DrawStretchPic(const ScreenPlacement*sp,float x,float y,float w,float h,int ha,int va,float,float,float,float,const float*c,Material*){
 assert(sp==&scrPlaceView[1]&&c[3]>0);ScrPlace_ApplyRect(sp,&x,&y,&w,&h,ha,va);lowHealthY=y;lowHealthH=h;++lowHealthDraws;
}
"""
code+=health_source+'\n'
code+=function('src/cgame/cg_playerstate.cpp','void __cdecl CG_DamageFeedback(')+'\n'
for sig in ['void __cdecl CG_DrawFlashDamage(', 'void __cdecl CG_DrawDamageDirectionIndicators(']:
 code+=function('src/cgame/cg_draw_indicators.cpp',sig)+'\n'
for sig in ['void __cdecl CG_UpdateCursorHints(', 'const char *__cdecl CG_GetUseString(', 'static void CG_GetZombieScoreColor_SP(', 'static void CG_DrawZombieScoreRow_SP(', 'void CG_DrawZombiePlayerScores_SP(']:
 code+=function('src/cgame_mp/cg_newDraw_mp.cpp',sig)+'\n'
code+=r"""
struct itemDef_s{};dvar_t clipWidth,stockWidth;auto ui_right_ammo_width=&clipWidth;auto ui_ammo_stock_width=&stockWidth;
void Dvar_SetIntIfChanged(dvar_s *d,int n){d->current.integer=n;}
const playerState_s *CG_GetPredictedPlayerState(int n){return &games[n].predictedPlayerState;}
bool BG_PlayerHasWeapon(const playerState_s *,unsigned w){return w>0&&w<10;}
struct WeaponDef {bool suppressAmmoReserveDisplay=false,fuelTankWeapon=false;int ammoCounterClip=1;}definition;
struct WeaponVariantDef{}variant;
const WeaponDef *BG_GetWeaponDef(unsigned){return &definition;}
const WeaponVariantDef *BG_GetWeaponVariantDef(unsigned){return &variant;}
int BG_GetAmmoInClipForWeaponDef(const playerState_s *ps,const WeaponVariantDef *){return ps->clip;}
int BG_GetClipSize(unsigned){return 8;}
unsigned GetWeaponAltIndex(const cg_s *,unsigned){return 0;}
constexpr int AMMO_COUNTER_CLIP_NONE=0,AMMO_COUNTER_CLIP_ALTWEAPON=2;
double AmmoCounterFadeAlpha(int n,cg_s *g){assert(n==1&&g==&games[1]&&g->ammoFadeTime>0);return 1;}
void AmmoColor(cg_s *g,float *,unsigned){assert(g==&games[1]);}
bool CG_CheckPlayerForLowAmmoSpecific(const cg_s *g,unsigned){return g->predictedPlayerState.reserve==0;}
const float colorLowAmmo[4]{1,.3f,.3f,1};
void UI_DrawWrappedText(const ScreenPlacement *sp,char *t,const rectDef_s *r,Font_s *f,float x,float y,float scale,float *c,int style,int,rectDef_s *){UI_DrawText(sp,t,64,f,x,y,r->horzAlign,r->vertAlign,scale,c,style);}
"""
for sig in ['unsigned int __cdecl GetWeaponIndex(', 'unsigned int __cdecl ClipCounterWeapIdx(', 'void __cdecl CG_DrawPlayerWeaponAmmoStock(', 'void __cdecl CG_DrawPlayerWeaponAmmoClip(']:
 code+=function('src/cgame/cg_ammocounter.cpp',sig)+'\n'
code+=r"""
void CG_OwnerDraw(int n,rectDef_s,float x,float y,float w,float h,int ha,int va,int id,int,float *c,Material *m){assert(n==1);rectDef_s r{x,y,w,h,ha,va};if(id==112)CG_DrawPlayerLowHealthOverlay(n,&r,m,c);else{assert(id==288);CG_DrawZombiePlayerScores_SP(n,&r);}}
void CG_OwnerDrawText(int n,rectDef_s,float x,float y,float w,float h,int ha,int va,float tx,float ty,int id,int,int,Font_s *f,float scale,float *c,Material *m,int style,int ta,itemDef_s *,char *){
 assert(n==1);rectDef_s r{x,y,w,h,ha,va};
 if(id==120)CG_DrawPlayerWeaponAmmoClip(n,&r,f,scale,c,m,style,tx,ty,ta);
 else if(id==122)CG_DrawPlayerWeaponAmmoStock(n,&r,f,scale,c,m,style);
 else {assert(id==72);CG_DrawCursorhint(n,&r,f,scale,c,style);}
}
"""
code+=clean(read('src/cgame/cg_splitscreen_hud.cpp'))+'\n'
# Exercise the actual backend viewport/scissor setup, including native scissor's
# viewport-origin addition (a lower viewport plus lower Y would double-offset).
code+=r"""
struct GfxViewport {int x=0,y=0,width=0,height=0;};
struct GfxCmdBufSourceState {int renderTargetWidth=1280,renderTargetHeight=721;GfxViewport sceneViewport,scissorViewport;int scissorEnabled=0,viewportIsDirty=0;};
struct GfxViewInfo {bool isMissileCamera=false;struct{GfxViewport displayViewport;}cullViewInfo;};
struct{bool splitscreen=true;}backend;auto backEndData=&backend;
void R_SetViewportStruct(GfxCmdBufSourceState *s,const GfxViewport *v){s->sceneViewport=*v;}
"""
code+=function('src/gfx_d3d/r_state.cpp','void __cdecl R_SetScissorStruct(')+'\n'
code+=function('src/gfx_d3d/rb_draw3d.cpp','static bool RB_SetSplitHudViewport(')+'\n'
code+=r"""
bool near(float a,float b){return std::fabs(a-b)<.01f;}
int main(){
 ScrPlace_SetupFloatViewport(&scrPlaceView[0],0,0,1280,360);ScrPlace_SetupFloatViewport(&scrPlaceView[1],0,360,1280,361);
 auto &lower=scrPlaceView[1];assert(lower.realViewportBase[1]==360&&lower.subScreen[1]==360);
 assert(near(ScrPlace_ApplyY(&lower,0,2),540.5f));assert(near(ScrPlace_ApplyY(&lower,480,4),721));
 float x=0,y=480,w=640,h=0;ScrPlace_ApplyRect(&lower,&x,&y,&w,&h,4,4);assert(near(y,721)&&near(w,1280));
 // Actual map entry assigns slot 1 its own game index, but there is only one UI.
 Com_LocalClients_AssignUIContextsForInGame();
 assert(clientGameStates[1].uiContextIndex==1);
 assert(UI_GetInfo(0)==UI_GetInfo(1)&&UI_GetInfo(1)==&sharedUI);
 sharedUI.uiDC.openMenuCount=1;
 assert(UI_AnyMenuVisible(0)&&!UI_AnyMenuVisible(1));
 guestMenuOpen=true;
 assert(!UI_AnyMenuVisible(0)&&UI_AnyMenuVisible(1));
 guestMenuOpen=false;sharedUI.uiDC.openMenuCount=0;
 guestConnected=false;assert(Com_LocalClient_GetUIContextIndex(1)==1);
 Com_LocalClients_AssignUIContextsForFrontEnd();assert(Com_LocalClient_GetUIContextIndex(1)==0);
 guestConnected=true;Com_LocalClients_AssignUIContextsForInGame();
 // Edge and safe-center anchors must include the lower viewport's origin.
 for(int align:{1,3,7,8,9,10}){float y=ScrPlace_ApplyY(&lower,0,align);assert(y>=360&&y<=721);float x=0,w=1,h=1,y2=0;ScrPlace_ApplyRect(&lower,&x,&y2,&w,&h,align,align);assert(near(y,y2));}
 assert(near(ScrPlace_ApplyY(&lower,-20,3),721-20*361.0f/480));
 ScreenPlacement right;ScrPlace_SetupFloatViewport(&right,640,0,640,721);for(int align:{1,3,7,8,9,10})assert(ScrPlace_ApplyX(&right,0,align)>=640);
 snapshot_s hostSnap;games[0].predictedPlayerState.clientNum=0;hostSnap.numClients=2;hostSnap.clients[0].clientIndex=0;hostSnap.clients[1].clientIndex=1;games[0].nextSnap=&hostSnap;
 games[0].bgs.clientinfo[0].infoValid=true;games[0].bgs.clientinfo[0].score.score=900;
 games[0].bgs.clientinfo[1].infoValid=true;games[0].bgs.clientinfo[1].score.score=500;
 games[0].bgs.clientinfo[2].infoValid=true;games[0].bgs.clientinfo[2].score.score=9999;
 itemHeight.current.integer=18;textOffset.current.value=.5f;
 // Capture the host's native widget layout. No fixed guest corner positions.
 float style[4]{1,1,1,1};rectDef_s parent{};
 auto capture=[&](bool text,int id,rectDef_s r){CG_SplitGuestCaptureHudWidget(0,text,id,parent,r,0,style,&scoreMaterial,&font,.35f,0,0,0,3,0);};
 capture(false,112,{0,0,640,480,4,4});assert(s_guestWidgetCount==1&&s_guestWidgets[0].ownerDraw==112);s_guestWidgetCount=0;
 capture(false,288,{-145,-90,100,27,3,3});capture(true,120,{-100,-25,20,20,3,3});capture(true,122,{-60,-25,50,20,3,3});capture(true,72,{0,70,24,24,2,2});
 assert(s_guestWidgetCount==4);capture(false,370,{});assert(s_guestWidgetCount==4);
 CG_SplitGuestCaptureHudWidget(1,false,288,parent,parent,0,style,&scoreMaterial,nullptr,0,0,0,0,0,0);assert(s_guestWidgetCount==4);
 clipWidth.current.integer=77;stockWidth.current.integer=88;
 refdef_s view;auto &ps=clients[1].snap.ps;ps.cursorHint=1;ps.cursorHintString=3;ps.clip=8;ps.reserve=32;ps.damageEvent=10;ps.damageCount=15;
 // flags=4 asks the actual script HUD path to query native menu visibility.
 ps.hud.current[0]={2,4,500,0};ps.hud.current[1]={2,1,77,1};clients[0].snap.ps.clip=30;clients[0].snap.ps.hud.current[0]={2,0,9000,0};
 games[0].v_dmg_time=9999;const auto host=games[0];kickMax.current.value=10;
 assert(CG_SplitGuestDrawHud(&view));assert((values==std::vector<float>{500,77}));assert(revivePasses==1&&crosshairPasses==1);assert(texts.size()==4&&texts[0]=="500"&&texts[1]=="900"&&texts[2]=="8"&&texts[3]=="/ 32"&&hintCalls==1);assert(clipWidth.current.integer==77&&stockWidth.current.integer==88);
 assert((commands==std::vector<std::string>{"begin","projection","clip","unclip","end"}));
 assert(games[1].cursorHintIcon==1&&games[1].cursorHintString==3);assert(!strcmp(CG_GetUseString(1),"Hold X to open door")&&bindingReads==1);
 assert(!games[1].v_dmg_time); // skip historical damage on initial join
 ps.damageEvent=11;ps.damageYaw=64;ps.damagePitch=0;view.time+=16;CG_SplitGuestDrawHud(&view);
 int expiry=games[1].v_dmg_time;assert(expiry==1516&&menuNotifications==0&&damageQuads>0);
 view.time+=1;CG_SplitGuestDrawHud(&view);
 assert(flashY==360&&flashH==361&&arrowY>360);view.time+=16;CG_SplitGuestDrawHud(&view);assert(games[1].v_dmg_time==expiry);
 // Latest network snapshot may lead the render clock by more than icon duration.
 // Directional feedback must still appear next frame and expire normally.
 clients[1].snap.serverTime=9000;ps.damageEvent++;view.time=1600;CG_SplitGuestDrawHud(&view);
 assert(games[1].damageTime==1600);
 view.time=1616;CG_SplitGuestDrawHud(&view);assert(games[1].viewDamage[1].time==1600&&arrowY>360);
 assert(!std::memcmp(&host,&games[0],sizeof(host)));assert(clients[0].snap.ps.clip==30&&clients[0].snap.ps.hud.current[0].value==9000);
 // A host menu does not hide guest script HUD. A guest menu does.
 values.clear();sharedUI.uiDC.openMenuCount=1;CG_SplitGuestDrawHud(&view);
 assert((values==std::vector<float>{500,77}));
 guestMenuOpen=true;values.clear();CG_SplitGuestDrawHud(&view);
 assert((values==std::vector<float>{77}));
 guestMenuOpen=false;sharedUI.uiDC.openMenuCount=0;guestMenusDrawn=0;
 // Repeated captures update the native layout without adding duplicates.
 auto previousY=textY.back();capture(true,122,{-60,-45,50,20,3,3});CG_SplitGuestDrawHud(&view);assert(s_guestWidgetCount==4&&textY.back()<previousY);
 // Downing the guest must retain the script HUD and ammo without a viewmodel.
 ps.pm_type=6;ps.clip=1;ps.reserve=12;CG_SplitGuestDrawHud(&view);assert(texts[texts.size()-2]=="1"&&texts.back()=="/ 12");
 ps.eFlags^=2;view.time+=16;CG_SplitGuestDrawHud(&view);assert(!games[1].v_dmg_time);
 ps.weapon=2;ps.reserve=5;CG_SplitGuestDrawHud(&view);assert(texts.back()=="/ 5");
 auto count=commands.size();draw2D=false;assert(!CG_SplitGuestDrawHud(&view)&&commands.size()==count);guestMenuOpen=true;assert(CG_SplitGuestDrawHud(&view)&&guestMenusDrawn==1);guestMenuOpen=false;draw2D=true;
 GfxCmdBufSourceState source;source.sceneViewport={0,360,1280,361};GfxViewInfo vi;vi.cullViewInfo.displayViewport={0,360,1280,361};
 assert(RB_SetSplitHudViewport(&source,&vi)&&source.sceneViewport.y==0&&source.sceneViewport.height==721);
 assert(source.scissorViewport.y==360&&source.scissorViewport.height==361); // no doubled offset
 backend.splitscreen=false;assert(!RB_SetSplitHudViewport(&source,&vi));backend.splitscreen=true;vi.isMissileCamera=true;assert(!RB_SetSplitHudViewport(&source,&vi));
 // Waypoint projection returns physical pixels in the lower viewport.
 games[1].refdef=refdef_s{};float target[3]{100,0,0},screen[2]{},normal[2]{},distance=0;
 assert(WorldPosToScreenPos(1,target,screen));assert(near(screen[0],640)&&near(screen[1],540.5f));
 corners.current.enabled=false;debugWaypoints.current.enabled=false;
 assert(!ClampScreenPosToEdges(1,screen,10,10,10,10,normal,&distance));assert(near(screen[1],540.5f));
 target[1]=-1000;target[2]=1000;WorldPosToScreenPos(1,target,screen);
 assert(ClampScreenPosToEdges(1,screen,10,10,10,10,normal,&distance));assert(screen[1]>=370&&screen[1]<=711&&screen[0]>=10&&screen[0]<=1270&&distance>0);
 corners.current.enabled=true;cornerRadius.current.value=16;WorldPosToScreenPos(1,target,screen);
 assert(ClampScreenPosToEdges(1,screen,10,10,10,10,normal,&distance));assert(screen[1]>=370&&screen[1]<=711);
 rectDef_s scores{12,-32,100,27,1,3};texts.clear();scoreViews.clear();CG_DrawZombiePlayerScores_SP(0,&scores);
 assert((texts==std::vector<std::string>{"900","500"}));assert((scoreViews==std::vector<int>{0,0}));
 hostSnap.numClients=1;CG_DrawZombiePlayerScores_SP(0,&scores);assert(texts.size()==3&&texts.back()=="900");
 // Blood overlay follows P2 health and remains inside the lower viewport.
 CG_SplitGuestResetHud();capture(false,112,{0,0,640,480,4,4});
 hud_healthOverlay_phaseEnd_toAlpha->current.value=0;
 hud_healthOverlay_pulseStart->current.value=.8f;hud_healthOverlay_pulseStop->current.value=.9f;
 ps.pm_type=0;ps.stats[0]=25;ps.stats[2]=100;view.time=5000;CG_SplitGuestDrawHud(&view);
 view.time=5100;CG_SplitGuestDrawHud(&view);
 assert(lowHealthDraws>0&&near(lowHealthY,360)&&near(lowHealthH,361));
 CG_SplitGuestResetHud();assert(!s_guestHud.valid&&!games[1].v_dmg_time&&s_guestWidgetCount==0);
 puts("PASS: captured native HUD layout, actual clip/stock widgets and own score rows, native one-event damage feedback without menu activation, viewport-local flash/direction, last stand HUD, lower placement and full-frame 2D projection with clipping");
}
"""

# Derive boolean registration from the actual native registration functions,
# with the real SP/MP preprocessor guards. Unknown dvars always return false.
registry_source=function('src/cgame_mp/cg_main_mp.cpp','void __cdecl CG_RegisterDvars(')+'\n'+function('src/cgame_mp/cg_newDraw_mp.cpp','void __cdecl CG_AntiBurnInHUD_RegisterDvars(')
registry_file=out/'registry.cpp';registry_file.write_text(registry_source)
for sp in (True,False):
 flags=['-DKISAK_EXPERIMENTAL_SPLITSCREEN=1','-DKISAK_SP=1' if sp else '-DKISAK_MP=1']
 expanded=subprocess.run([args.compiler,'-E','-P','-x','c++',*flags,str(registry_file)],check=True,capture_output=True,text=True).stdout
 names=sorted(set(re.findall(r'_Dvar_RegisterBool\s*\(\s*"([^"]+)"',expanded)))
 assert 'hud_enable' in names and 'cg_draw2D' in names
 if not sp:assert 'cg_drawHUD' not in names and 'hud_drawHUD' not in names
 registered='bool TestDvarRegistered(const char*n){const char*names[]={'+','.join('"'+n+'"' for n in names)+'};for(auto name:names)if(!strcmp(n,name))return true;return false;}\n'
 test=code if sp else code.replace('#define KISAK_SP 1','').replace('revivePasses==1','revivePasses==0')
 cpp=out/('hud-sp.cpp' if sp else 'hud-mp.cpp');exe=cpp.with_suffix('.exe');cpp.write_text(test+registered)
 subprocess.run([args.compiler,'-std=c++17','-O0',str(cpp),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
print('PASS: native MP registry has neither cg_drawHUD nor hud_drawHUD; hud_enable drives HUD; menus draw even with HUD disabled')
