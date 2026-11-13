"""Exercise SP revive marker submission, color timing and interrupted revives."""
from pathlib import Path
import argparse
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler', default='/c/msys64/ucrt64/bin/g++.exe')
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
out = root / 'build-nx/revive-indicator-tests'
out.mkdir(parents=True, exist_ok=True)
hud = (root / 'src/cgame/cg_hudelem.cpp').read_text()
commands = (root / 'src/game_mp/g_client_script_cmd_mp.cpp').read_text()
def function(source, signature):
    start = source.index(signature)
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]
code = r'''
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <vector>
#define ARRAY_COUNT(a) (sizeof(a)/sizeof((a)[0]))
enum team_t { TEAM_FREE=0, TEAM_AXIS=1, TEAM_ALLIES=2, TEAM_SPECTATOR=3 };
constexpr int ET_PLAYER=1, ET_INVISIBLE=6, HE_TYPE_WAYPOINT=15;
struct clientState_s { int clientIndex=0; team_t team=TEAM_ALLIES; int needsRevive=0,lastStandStartTime=0,beingRevived=0; };
struct snapshot_s { struct { int clientNum=0,pm_type=0; } ps; int numClients=2; clientState_s clients[4]; } snap;
struct centity_s { unsigned clientFlags=2; struct { int eType=ET_PLAYER; struct { unsigned eFlags=0; } lerp; } nextState; struct {float origin[3]{};}pose; } entities[4];
struct cg_s { snapshot_s *nextSnap=&snap; struct { struct { team_t team=TEAM_ALLIES; } clientinfo[32]; } bgs; int localClientNum=0,clientNum=0,time=1500;struct{int clientNum=0;}predictedPlayerState;centity_s predictedPlayerEntity; } cg,guest;
snapshot_s guestSnap;
struct { struct { int integer=4; } current; } maxclients;
auto com_maxclients=&maxclients;
union hudelem_color_t { struct { unsigned char r,g,b,a; }; int rgba; };
struct hudelem_s { int type=0,targetEntNum=0,ui3dWindow=0; hudelem_color_t color{}; float fontScale=0,x=0,y=0,z=0; };
struct Material {} revive;
bool materialPresent=true;
int registrations=0;
std::vector<hudelem_s> submitted;
struct { struct { float value=30; } current; } localDuration;
auto player_lastStandBleedoutTime=&localDuration;
struct{struct{float value=40;}current;}reviveOffset;auto waypointPlayerOffsetRevive=&reviveOffset;
const char *hostDuration="45";
const char *CL_GetConfigString(unsigned index) { assert(index==0); return "serverinfo"; }
const char *Info_ValueForKey(const char *info,const char *key) { assert(!strcmp(info,"serverinfo") && !strcmp(key,"player_lastStandBleedoutTime")); return hostDuration; }
bool CL_IsSplitGuest(int n){return n==1;}
cg_s *CG_GetLocalClientGlobals(int n) { assert(n>=0&&n<2); return n==0?&cg:&guest; }
centity_s *CG_GetEntity(int n, int entity) { assert(n==0 && entity>=0 && entity<4); return &entities[entity]; }
Material *Material_RegisterHandle(const char *name,int track) { assert(!strcmp(name,"waypoint_revive") && track==7); ++registrations; return materialPresent ? &revive : nullptr; }
void DrawOffscreenViewableWaypointMaterial(int n,const hudelem_s *elem,Material *mat) {
    assert(n>=0 && n<2 && mat==&revive);
    assert(elem->type==HE_TYPE_WAYPOINT && elem->ui3dWindow==-1 && elem->fontScale==1);
    assert(elem->color.r==255 && elem->color.a==255);
    submitted.push_back(*elem);
}
using scr_entref_t=int;
struct gclient_s { int revive=0; struct { int pm_type=6; } ps; } downedClient;
struct gentity_s { gclient_s *client=&downedClient; struct { int number=1; } s; } downed;
gentity_s *PlayerCmd_ViewModelSelf_SP(scr_entref_t n) { assert(n==1); return &downed; }
clientState_s *G_GetClientState(int n) { assert(n==1); return &snap.clients[n]; }
'''
code += function(hud, 'static hudelem_color_t CG_ReviveIndicatorColor_SP(')
code += function(hud, 'void CG_DrawReviveIndicators_SP(')
code += function(commands, 'static void PlayerCmd_StartRevive_SP(')
code += function(commands, 'static void PlayerCmd_StopRevive_SP(')
code += r'''
void draw(int expected) { submitted.clear(); CG_DrawReviveIndicators_SP(0); assert(submitted.size()==static_cast<unsigned>(expected)); }
int main() {
    snap.clients[0].clientIndex=0; snap.clients[1].clientIndex=1;
    draw(0); assert(registrations==0);
    snap.clients[1].lastStandStartTime=1500;
    draw(1); assert(submitted[0].targetEntNum==1);
    assert(submitted[0].color.g==179 && submitted[0].color.b==0); // BO1 amber.
    cg.time=1500+22500; draw(1);
    assert(submitted[0].color.g==89 && submitted[0].color.b==0);
    // The server says 45, even though this joining client's default is 30.
    cg.time=1500+30000; draw(1);
    assert(submitted[0].color.g==60);
    cg.time=1500+45000; draw(1); assert(submitted[0].color.g==0);
    // Use the actual engine methods which the script calls on start/cancel.
    cg.time=1500+10000;
    PlayerCmd_StartRevive_SP(1); draw(1);
    assert(downedClient.revive==1 && downedClient.ps.pm_type==7);
    assert(submitted[0].color.rgba==-1);
    cg.time=1500+20000;
    PlayerCmd_StopRevive_SP(1); draw(1);
    assert(downedClient.revive==0 && downedClient.ps.pm_type==6);
    assert(snap.clients[1].lastStandStartTime==1500);
    assert(submitted[0].color.g==99 && submitted[0].color.b==0);
    PlayerCmd_StartRevive_SP(1); draw(1); assert(submitted[0].color.rgba==-1);
    cg.time=1500+46000;
    PlayerCmd_StopRevive_SP(1); draw(1); assert(submitted[0].color.g==0);
    // A new down has a new timeline; future start times clamp to amber.
    snap.clients[1].lastStandStartTime=cg.time+500; draw(1);
    assert(submitted[0].color.g==179);
    snap.clients[1].lastStandStartTime=1500;
    hostDuration=""; cg.time=1500+15000; draw(1); assert(submitted[0].color.g==89);
    hostDuration="0"; draw(1); assert(submitted[0].color.g==0);
    hostDuration="45"; cg.time=1500;
    // Every client uses the received snapshot, not server globals. Reverse the
    // viewpoint and confirm the joining client can see a downed host.
    cg.clientNum=snap.ps.clientNum=1;
    snap.clients[1].lastStandStartTime=0; snap.clients[0].lastStandStartTime=1500;
    draw(1); assert(submitted[0].targetEntNum==0 && submitted[0].color.g==179);
    snap.clients[0].lastStandStartTime=0; cg.clientNum=snap.ps.clientNum=0;
    snap.clients[1].needsRevive=1; draw(1);
    snap.clients[1].needsRevive=0; snap.clients[1].beingRevived=1; draw(1);
    assert(submitted[0].color.rgba==-1);
    snap.clients[1].beingRevived=0; draw(0); // Revival removes the marker.
    snap.clients[1].lastStandStartTime=1500;
    entities[1].clientFlags=0; draw(0); entities[1].clientFlags=2;
    entities[1].nextState.eType=ET_INVISIBLE; draw(0); entities[1].nextState.eType=ET_PLAYER;
    entities[1].nextState.lerp.eFlags=0x20; draw(0); entities[1].nextState.lerp.eFlags=0;
    snap.clients[1].team=TEAM_AXIS; draw(0);
    snap.clients[1].team=TEAM_SPECTATOR; draw(0); snap.clients[1].team=TEAM_ALLIES;
    snap.ps.pm_type=6; draw(0); snap.ps.pm_type=7; draw(0); snap.ps.pm_type=0;
    snap.ps.clientNum=1; draw(0); snap.ps.clientNum=0;
    snap.clients[1].clientIndex=31; draw(0); snap.clients[1].clientIndex=-1; draw(0); snap.clients[1].clientIndex=1;
    snap.ps.clientNum=33; draw(0); snap.ps.clientNum=0;
    materialPresent=false; draw(0); materialPresent=true;
    cg.nextSnap=nullptr; draw(0); cg.nextSnap=&snap;
    snap.numClients=3; snap.clients[2].clientIndex=2; snap.clients[2].needsRevive=1;
    registrations=0; draw(2); assert(registrations==1);
#ifdef KISAK_EXPERIMENTAL_SPLITSCREEN
    // Guest owns only its PS/camera; metadata and body live in the shared host CG.
    snap.numClients=2;snap.ps.clientNum=cg.clientNum=0;snap.ps.pm_type=0;
    snap.clients[0].lastStandStartTime=1500;snap.clients[1].needsRevive=0;
    guest.nextSnap=&guestSnap;guestSnap.numClients=0;guestSnap.ps.clientNum=guest.clientNum=1;guest.localClientNum=1;
    guest.bgs.clientinfo[1].team=TEAM_FREE;guest.time=1500+22500;
    cg.predictedPlayerEntity.pose.origin[0]=100;cg.predictedPlayerEntity.pose.origin[1]=200;cg.predictedPlayerEntity.pose.origin[2]=30;
    submitted.clear();CG_DrawReviveIndicators_SP(1);assert(submitted.size()==1);
    assert(submitted[0].targetEntNum==1023&&submitted[0].x==100&&submitted[0].y==200&&submitted[0].z==70&&submitted[0].color.g==89);
    snap.clients[0].beingRevived=1;submitted.clear();CG_DrawReviveIndicators_SP(1);assert(submitted.size()==1&&submitted[0].color.rgba==-1);
    snap.clients[0].beingRevived=0;submitted.clear();CG_DrawReviveIndicators_SP(1);assert(submitted.size()==1&&submitted[0].color.g==89);
    guestSnap.ps.pm_type=6;submitted.clear();CG_DrawReviveIndicators_SP(1);assert(submitted.empty());guestSnap.ps.pm_type=0;
    snap.clients[0].lastStandStartTime=0;submitted.clear();CG_DrawReviveIndicators_SP(1);assert(submitted.empty());
    puts("PASS: guest marker uses shared host roster/body and explicit world coordinates with its own viewer/color time");
#endif
    puts("PASS: revive color timeline, host duration, actual Start/StopRevive and marker lifecycle");
}
'''
cpp = out / 'revive.cpp'
cpp.write_text(code, encoding='utf-8')
for name, definitions in [('pc-sp',['-DWIN32','-DKISAK_SP']), ('nx-sp',['-DKISAK_NX','-DKISAK_SP']), ('split-sp',['-DKISAK_SP','-DKISAK_EXPERIMENTAL_SPLITSCREEN'])]:
    exe=out/(name+'.exe')
    subprocess.run([args.compiler,'-std=c++17','-O2',*definitions,str(cpp),'-o',str(exe)], check=True)
    subprocess.run([str(exe)],check=True)
    print(name+': passed',flush=True)
assert 'DrawOffscreenViewableWaypointMaterial(localClientNum, elem, nullptr);' in hud
assert 'hasMaterial = materialOverride || CG_ServerMaterialName' in hud
assert 'material = materialOverride ? materialOverride : Material_RegisterHandle(materialName, 7);' in hud
assert 'CG_DrawReviveIndicators_SP(localClientNum);' in (root/'src/cgame_mp/cg_draw_mp.cpp').read_text()
registration=(root/'src/bgame/bg_misc.cpp').read_text().split('player_lastStandBleedoutTime = _Dvar_RegisterFloat(',1)[1].split('player_lastStandBleedoutTimeNoRevive =',1)[0]
assert '#ifdef KISAK_SP' in registration and '0x4084u,' in registration and '#else' in registration and '0x4080u,' in registration
print('SP host-duration publication and existing waypoint routing checked; no game launched.')
