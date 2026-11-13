"""Verify native MP HUD metadata/text ownership without a second UI or animation lifecycle."""
from pathlib import Path
import argparse,re,subprocess
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--compiler',default='/c/msys64/ucrt64/bin/g++.exe');args=p.parse_args()
root=Path(__file__).resolve().parents[3];out=root/'build-nx/splitscreen-mp-hud-tests';out.mkdir(parents=True,exist_ok=True)
def function(path,sig):
 s=(root/path).read_text();a=s.index(sig);b=s.index('{',a)+1;d=1
 while d:d+=(s[b]=='{')-(s[b]=='}');b+=1
 return s[a:b]
code=r"""
#include <cassert>
#include <cstring>
#include <cstdio>
#include <vector>
#ifndef __cdecl
#define __cdecl
#endif
struct Material{}mapMaterial;
struct entityState_s{int number=0;};
struct snapshot_s{int serverTime=1000;int numEntities=0;entityState_s entities[512];};
struct playerState_s{int clientNum=1;};
struct clientInfo{int infoValid=0,team=0,oldteam=0,needsRevive=0,ffaTeam=0;unsigned perks[2]{};struct{int score=0;}score;void*animationOwner=nullptr;};
struct cg_s{
 int clientNum=1;int matchUIVisibilityFlags=0,numScores=0,scoreLimit=0,teamScores[4]{},teamPings[4]{},teamPlayers[4]{},scoreOrder[32]{},bombTimer[2]{};
 struct{clientInfo clientinfo[32];}bgs;
 float compassNorthYaw=0,compassNorth[2]{},compassMapUpperLeft[2]{},compassMapWorldSize[2]{};
 Material*compassMapMaterial=nullptr;int compassFadeTime=0,offhandFadeTime=0,time=1000;snapshot_s*snap=nullptr,*nextSnap=nullptr;playerState_s predictedPlayerState;
}games[2];
cg_s*CG_GetLocalClientGlobals(int n){return &games[n];}
struct cgs_t{char gametype[32]{};int maxclients=0,gameEndTime=0;void*resourceOwner=nullptr;}statics[2];
cgs_t*CG_GetLocalClientStaticGlobals(int n){return &statics[n];}
struct{struct{int numEntities=0,parseEntitiesNum=0;}snap;entityState_s parseEntities[2048];}clients[2];
struct ScreenPlacement{int y=0,height=360;}scrPlaceView[2]{{0,360},{360,361}};
struct UiContext{int owner=0;};UiContext cgDC[2],otherUi;
struct Expression{const char*filename=nullptr;};
struct textDef_s{int itemFlags=0;int cachedWidth=123;};
struct itemDef_s{int type=1;struct{textDef_s*textDef=nullptr;}typeData;Expression forecolorAExp;struct{float foreColor[4]{1,1,1,1};}window;};
textDef_s*Item_GetTextDef(itemDef_s*item){return item->typeData.textDef;}
bool CL_IsSplitGuest(int n){return n==1;}
float GetExpressionFloat(int n,itemDef_s*,const Expression*){assert(n==1);return .75f;}
void CG_ResetCompassData(int n,int t){assert(n==1&&t==0);}
int compassUpdates=0;
void CG_CompassUpdateActors(int n){assert(n==1&&games[n].compassMapWorldSize[0]==2000&&games[n].snap->numEntities==2);assert(games[n].snap->entities[0].number==7&&games[n].snap->entities[1].number==1);assert(games[n].bgs.clientinfo[1].team==2);++compassUpdates;}
// Execute the real score expression used by the native HUD.
constexpr int VAL_INT=1;
struct Value{int intVal=0;const char*string=nullptr;};
struct Operand{int dataType=VAL_INT;Value internals;};
struct OperandStack{Operand value;};
void GetOperand(OperandStack*s,Operand*o){*o=s->value;}
Value GetSourceInt(Operand*o){return o->internals;}
void AddOperandToStack(OperandStack*s,Operand*o){s->value=*o;}
"""
code+=r"""
using team_t=int;constexpr int VAL_STRING=2;
struct DebugDvar{struct{int integer=0;}current;};DebugDvar*uiscript_debug=nullptr;
int I_stricmp(const char*a,const char*b){return strcmp(a,b);}
const char*CG_GetFFATeamName(int){return "free";}const char*CG_GetTeamName(int){return "team";}
const char*GetNameForValueType(int){return "integer";}
void Expression_Error(const char*,...){assert(false);}void Expression_TraceInternal(const char*,...){}
int CL_GetLocalClientConnectionState(int){return 10;}
"""
code+=function('src/universal/com_expressions_eval.cpp','void __cdecl GetFieldForTeam(')
code+=function('src/universal/com_expressions_eval.cpp','void __cdecl GetTimeLeft(')
code+=function('src/universal/com_expressions_eval.cpp','void __cdecl GetScoreForClient(')
code+=r"""
int textDraws=0,lastScore=0;
void Item_Text_Paint(int n,UiContext*dc,itemDef_s*item){
 assert(n==1&&dc==&cgDC[0]&&scrPlaceView[0].y==360&&item->window.foreColor[3]==.75f);
 OperandStack stack;stack.value.internals.intVal=games[n].predictedPlayerState.clientNum;
 GetScoreForClient(n,item,&stack);lastScore=stack.value.internals.intVal;
 item->typeData.textDef->cachedWidth=999;item->window.foreColor[3]=0;++textDraws;
}
"""
source=(root/'src/cgame/cg_splitscreen_hud_mp.cpp').read_text()
code+=re.sub(r'^\s*#include.*\n','',source,flags=re.M)
code+=r"""
int main(){
 auto&host=games[0];auto&guest=games[1];
 host.compassNorthYaw=90;host.compassNorth[1]=1;host.compassMapWorldSize[0]=2000;host.compassMapWorldSize[1]=1000;host.compassMapMaterial=&mapMaterial;
 host.bgs.clientinfo[0].infoValid=host.bgs.clientinfo[1].infoValid=1;
 host.bgs.clientinfo[0].score.score=900;host.bgs.clientinfo[1].score.score=500;host.bgs.clientinfo[1].team=2;
 host.bgs.clientinfo[1].animationOwner=(void*)123;guest.bgs.clientinfo[1].animationOwner=(void*)456;
 strcpy(statics[0].gametype,"dm");statics[0].maxclients=2;statics[0].resourceOwner=(void*)123;statics[1].resourceOwner=(void*)456;
 clients[0].snap.numEntities=1;clients[0].parseEntities[0].number=99;
 clients[1].snap.numEntities=2;clients[1].snap.parseEntitiesNum=2047;clients[1].parseEntities[2047].number=7;clients[1].parseEntities[0].number=1;
 snapshot_s snapshot;guest.snap=guest.nextSnap=&snapshot;host.teamScores[1]=3;host.teamScores[2]=7;host.scoreLimit=75;statics[0].gameEndTime=61000;CG_SplitGuestUpdateMpHud(&snapshot);
 assert(compassUpdates==1&&guest.compassMapMaterial==&mapMaterial&&guest.compassFadeTime==1000);
 assert(guest.bgs.clientinfo[1].animationOwner==(void*)456&&statics[1].resourceOwner==(void*)456);
 assert(!strcmp(statics[1].gametype,"dm")&&statics[1].maxclients==2);
 Operand field;field.dataType=VAL_STRING;field.internals.string="score";Operand teamScore;
 GetFieldForTeam(1,1,&field,&teamScore);assert(teamScore.internals.intVal==3);
 GetFieldForTeam(1,2,&field,&teamScore);assert(teamScore.internals.intVal==7&&guest.scoreLimit==75);
 OperandStack timer;GetTimeLeft(1,nullptr,&timer);assert(timer.value.internals.intVal==60000);
 textDef_s text;itemDef_s sourceItem;sourceItem.typeData.textDef=&text;sourceItem.forecolorAExp.filename="score.menu";
 CG_SplitGuestCaptureHudText(0,&otherUi,&sourceItem);assert(s_guestTextCount==0); // pause/frontend excluded
 CG_SplitGuestCaptureHudText(0,&cgDC[0],&sourceItem);CG_SplitGuestCaptureHudText(0,&cgDC[0],&sourceItem);assert(s_guestTextCount==1);
 CG_SplitGuestDrawMpHudText();assert(textDraws==1&&lastScore==500&&text.cachedWidth==123&&sourceItem.window.foreColor[3]==1);
 assert(scrPlaceView[0].y==0&&host.bgs.clientinfo[0].score.score==900);
 host.teamScores[1]=4;host.teamScores[2]=9;statics[0].gameEndTime=91000;snapshot.serverTime=21000;
 host.bgs.clientinfo[1].score.score=600;CG_SplitGuestUpdateMpHud(&snapshot);
 GetFieldForTeam(1,2,&field,&teamScore);assert(teamScore.internals.intVal==9);GetTimeLeft(1,nullptr,&timer);assert(timer.value.internals.intVal==70000);
 snapshot.serverTime=22000;CG_SplitGuestUpdateMpHud(&snapshot);GetTimeLeft(1,nullptr,&timer);assert(timer.value.internals.intVal==69000);CG_SplitGuestDrawMpHudText();assert(lastScore==600);
 CG_SplitGuestResetMpHud();CG_SplitGuestDrawMpHudText();assert(s_guestTextCount==0&&textDraws==2);
 puts("PASS: P2 live match timer/team-score expressions, native score expression, private text/item copies, lower HUD placement, own snapshot compass input, shared map/score metadata, host animation/resource ownership and reset");
}
"""
cpp=out/'mp-hud.cpp';exe=cpp.with_suffix('.exe');cpp.write_text(code)
subprocess.run([args.compiler,'-std=c++17','-O0',str(cpp),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
