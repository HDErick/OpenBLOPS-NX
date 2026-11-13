"""Exercise real campaign and MP target selection without launching the game."""
from pathlib import Path
import argparse
import os
import re
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--compiler', default='C:/msys64/ucrt64/bin/g++.exe')
a = p.parse_args()
root = Path(__file__).resolve().parents[3]
source = (root / 'src/game/actor_threat.cpp').read_text()

def function(name, source=source):
    match = re.search(r'^\w[^\n]*\b' + name + r'\([^;]*?\)\s*\{', source, re.M)
    assert match, name
    end = source.index('{', match.start()) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end]

harness = r"""
#include <cassert>
#include <climits>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#ifndef __fastcall
#define __fastcall
#endif
#ifndef __cdecl
#define __cdecl
#endif
#define PROF_SCOPED(x)
#define iassert(x) assert(x)
struct gentity_s; struct sentient_s; struct actor_s;
struct Handle {
 gentity_s* value=nullptr;
 bool isDefined() const { return value!=nullptr; }
 gentity_s* ent() const { return value; }
};
struct Favorite {
 sentient_s* value=nullptr;
 bool isDefined() const { return value!=nullptr; }
 sentient_s* sentient() const { return value; }
};
struct Client {
 struct { int sessionState=0; } sess;
 struct { int eFlags=0,viewlocked_entNum=0; unsigned perks[2]{}; } ps;
 int flags=0;
};
struct Vehicle { int infoIdx=0; } vehicle;
struct gentity_s {
 actor_s* actor=nullptr; sentient_s* sentient=nullptr; Client* client=nullptr;
 int flags=0,health=100,maxHealth=100;
 struct { int number=0; } s;
 struct { float currentOrigin[3]{}; } r;
 Vehicle* scr_vehicle=&vehicle;
};
struct sentient_s {
 gentity_s* ent=nullptr;
 int eTeam=0,iThreatBiasGroupIndex=0,iThreatBias=0,attackerCount=0;
 bool inuse=false,bIgnoreMe=false,bIgnoreAll=false;
 float entityTargetThreat=0;
 Handle scriptOwner,targetEnt,syncedMeleeEnt;
};
struct sentient_info_t {
 struct { bool bVisible=true; int iLastVisTime=10000; } VisCache;
 int lastKnownPosTime=10000,iLastAttackMeTime=0,attackTime=0;
 float vLastKnownPos[3]{};
};
struct actor_s {
 gentity_s* ent=nullptr; sentient_s* sentient=nullptr;
 int threatUpdateTime=0,hasThreateningEnemy=0,iPacifistWait=1000,species=0;
 bool bPacifist=false;
 sentient_info_t sentientInfo[4];
 Favorite pFavoriteEnemy;
 gentity_s* scriptedTarget=nullptr;
};
struct { int time=10000; sentient_s sentients[4]; } level;
gentity_s g_entities[4]; actor_s actors[4]; Client client;
struct Dvar { struct { int integer=0; bool enabled=false; } current; };
Dvar interval,show,ghost,attackerLimit;
Dvar *ai_threatUpdateInterval=&interval,*ai_showPotentialThreatDir=&show,*perk_dogsAttackGhost=&ghost;
Dvar* ai_maxAttackerCount=&attackerLimit;
constexpr int AI_SPECIES_DOG=1,AI_SPECIES_ZOMBIE_DOG=3,TDS_ATTACKER_COUNT=3;
bool Flame_GetLocalClientSourceRange() { return true; }
float colorRed[4]{},colorGreen[4]{},colorYellow[4]{};
constexpr int TDS_TOTAL_THREAT=0,TDS_THREATBIAS_GROUP=1,TDS_THREATBIAS=2;
bool havePath=true,tooManyAttackers=false,ignoreBias=false;
int pathChecks=0,attackerChecks=0;
bool Assert_MyHandler(const char*,int,int,const char*,...) { assert(false);return true; }
void __debugbreak() { assert(false); }
char* va(const char* fmt,...) {
 static char b[256];va_list a;va_start(a,fmt);vsnprintf(b,sizeof(b),fmt,a);va_end(a);return b;
}
void Actor_IncrementThreatTime(actor_s*) {}
void Actor_PotentialThreat_Debug(actor_s*) {}
gentity_s* Actor_GetScriptTargetEntity(actor_s* a) { return a->scriptedTarget; }
gentity_s* Actor_GetTargetEntity(actor_s* a) { return a->sentient->targetEnt.value; }
sentient_s* Actor_GetTargetSentient(actor_s* a) {
 auto* e=Actor_GetTargetEntity(a);return e?e->sentient:nullptr;
}
void Sentient_SetEnemy(sentient_s* a,gentity_s* e,int) { a->targetEnt.value=e; }
int Sentient_EnemyTeam(int team) { return team==1?2:team==2?1:0; }
sentient_s* Sentient_NextSentient(sentient_s* prev,int flags) {
 for(int i=prev?int(prev-level.sentients)+1:0;i<4;++i)
  if(level.sentients[i].inuse && (flags&(1<<level.sentients[i].eTeam)))return &level.sentients[i];
 return nullptr;
}
sentient_s* Sentient_FirstSentient(int flags) { return Sentient_NextSentient(nullptr,flags); }
int Actor_GetThreatBias(int,int) { return ignoreBias?INT_MIN:0; }
void DebugThreatStringSimple(actor_s*,gentity_s*,char*,float*) {}
void DebugThreatStringAll(actor_s*,sentient_s*,int) {}
void DebugResetThreatStrings(actor_s*) {}
void DebugSetThreatString(int,int) {}
void DebugSetThreatStringFromString(int,const char*) {}
struct VehicleInfo { bool remoteControl=false; } vehicleInfo;
VehicleInfo* BG_GetVehicleInfo(int) { return &vehicleInfo; }
float Vec3Distance(const float* a,const float* b) {
 float x=a[0]-b[0],y=a[1]-b[1],z=a[2]-b[2];return sqrtf(x*x+y*y+z*z);
}
bool Actor_ShouldIgnoreFromAttackerCount(actor_s*,sentient_s*,int) { ++attackerChecks;return tooManyAttackers; }
bool Actor_CanBePathedTo(actor_s*,sentient_s*,int) { ++pathChecks;return havePath; }
bool Actor_IsFullyAware(actor_s*,sentient_s*,int) { return true; }
double Sentient_GetScarinessForDistance(sentient_s*,sentient_s*,float) { return 0; }
int Actor_ThreatFromVisibilityAndAwareness(int,int,int) { return 1000; }
int Actor_ThreatFromScariness(float) { return 0; }
int Actor_ThreatFromDistance(float distance) { return int(2500-distance); }
int Actor_ThreatFromAttackerCount(actor_s*,sentient_s*,int);
int Actor_ThreatBonusForCurrentEnemy(int,int,int,int,int) { return 0; }
int Actor_ThreatCoveringFire(actor_s*,sentient_s*) { return 0; }
int Actor_ThreatFlashed(sentient_s*) { return 0; }
"""
harness += '\n'.join(function(name) for name in [
    'Actor_CheckIgnore', 'Actor_ThreatFromAttackerCount', 'Actor_UpdateSingleThreat', 'Actor_UpdateThreat'])
aim_source = (root / 'src/game/actor_aim.cpp').read_text()
harness += '\n#ifdef KISAK_SP\n'
harness += '\n'.join(function(name, aim_source) for name in [
    'Actor_ShotLength_SP', 'Actor_NormalizeShot_SP', 'Actor_ClearPlayerMissRay_SP'])
harness += '\n#endif\n'
harness += r"""
void reset() {
 level={};level.time=10000;client={};ignoreBias=false;attackerLimit.current.integer=3;
 havePath=true;tooManyAttackers=false;pathChecks=attackerChecks=0;
 for(int i=0;i<4;++i) {
  g_entities[i]={};actors[i]={};
  auto& s=level.sentients[i];auto& e=g_entities[i];auto& a=actors[i];
  s.inuse=true;s.ent=&e;s.eTeam=i%2?1:2;
  e.sentient=&s;e.actor=&a;e.s.number=i;
  a.ent=&e;a.sentient=&s;
 }
 g_entities[0].client=&client;g_entities[0].actor=nullptr;
 g_entities[0].r.currentOrigin[0]=1000;
 g_entities[2].r.currentOrigin[0]=100;
 g_entities[3].r.currentOrigin[0]=200;
 for(auto& a:actors)for(int i=0;i<4;++i)
  memcpy(a.sentientInfo[i].vLastKnownPos,g_entities[i].r.currentOrigin,sizeof(float)*3);
}
int main() {
 // Axis gunner must prefer the nearby allied actor over the distant player.
 reset();Actor_UpdateThreat(&actors[1]);
#ifdef KISAK_SP
 assert(Actor_GetTargetEntity(&actors[1])==&g_entities[2]);
#else
 assert(Actor_GetTargetEntity(&actors[1])==&g_entities[0]);
#endif
 // Allied gunner must acquire a hostile actor without a scripted shoot override.
 reset();Actor_UpdateThreat(&actors[2]);
#ifdef KISAK_SP
 assert(Actor_GetTargetEntity(&actors[2])==&g_entities[1]);
#else
 assert(!Actor_GetTargetEntity(&actors[2]));
#endif
 // A visible enemy on a roof is still a valid campaign gun target.
 reset();havePath=false;Actor_UpdateThreat(&actors[1]);
#ifdef KISAK_SP
 assert(Actor_GetTargetEntity(&actors[1])==&g_entities[2]);assert(pathChecks==0);
#else
 assert(!Actor_GetTargetEntity(&actors[1]));assert(pathChecks>0);
#endif
 // The MP dog attacker cap must not veto a campaign gunner's only target.
 reset();tooManyAttackers=true;Actor_UpdateThreat(&actors[1]);
#ifdef KISAK_SP
 assert(Actor_GetTargetEntity(&actors[1])==&g_entities[2]);assert(attackerChecks==0);
#else
 assert(!Actor_GetTargetEntity(&actors[1]));assert(attackerChecks>0);
#endif
 // Human saturation is a modest score penalty, not the canine -10000.
 reset();level.sentients[2].attackerCount=3;
#ifdef KISAK_SP
 assert(Actor_ThreatFromAttackerCount(&actors[1],&level.sentients[2],0)==-450);
#else
 assert(Actor_ThreatFromAttackerCount(&actors[1],&level.sentients[2],0)==-10450);
#endif
 actors[1].species=AI_SPECIES_DOG;
 assert(Actor_ThreatFromAttackerCount(&actors[1],&level.sentients[2],0)==-10450);
 assert(Actor_ThreatFromAttackerCount(&actors[1],&level.sentients[2],1)==-300);
 reset();level.sentients[2].bIgnoreMe=true;Actor_UpdateThreat(&actors[1]);
 assert(Actor_GetTargetEntity(&actors[1])==&g_entities[0]);
 reset();ignoreBias=true;Actor_UpdateThreat(&actors[1]);assert(!Actor_GetTargetEntity(&actors[1]));
 reset();actors[1].sentientInfo[0].lastKnownPosTime=0;actors[1].sentientInfo[2].lastKnownPosTime=0;
 Actor_UpdateThreat(&actors[1]);assert(!Actor_GetTargetEntity(&actors[1]));
 reset();level.sentients[1].bIgnoreAll=true;Actor_UpdateThreat(&actors[1]);assert(!Actor_GetTargetEntity(&actors[1]));
 reset();actors[1].scriptedTarget=&g_entities[2];level.sentients[1].entityTargetThreat=1;
 Actor_UpdateThreat(&actors[1]);assert(Actor_GetTargetEntity(&actors[1])==&g_entities[2]);
 reset();actors[1].bPacifist=true;Actor_UpdateThreat(&actors[1]);assert(!Actor_GetTargetEntity(&actors[1]));
#ifdef KISAK_SP
 reset();level.sentients[2].bIgnoreMe=true;g_entities[0].health=0;
 Actor_UpdateThreat(&actors[1]);assert(!Actor_GetTargetEntity(&actors[1]));

 const float start[3]={0,0,55}, target[3]={100,0,0};
 const float mins[3]={-15,-15,0}, maxs[3]={15,15,70};
 const float sides[2]={-1,1};
 for (float sign : sides) {
  float ray[3]={1,sign*0.1f,-0.2f};
  Actor_ClearPlayerMissRay_SP(start,target,mins,maxs,ray);
  assert(sign*ray[1]*85.0f/ray[0]>15.0f);
  assert(std::fabs(ray[2]/ray[0]+0.2f)<1.0e-5f);
 }
 const float diagonal[3]={100,100,0};
 float diagonalRay[3]={1,1,0};
 Actor_ClearPlayerMissRay_SP(start,diagonal,mins,maxs,diagonalRay);
 assert(diagonalRay[1]*85.0f/diagonalRay[0]>115.0f);
 float alreadyClear[3]={1,0.5f,0};
 Actor_ClearPlayerMissRay_SP(start,target,mins,maxs,alreadyClear);
 assert(alreadyClear[0]==1&&alreadyClear[1]==0.5f&&alreadyClear[2]==0);
 float aboveCrouchedPlayer[3]={1,0,0};
 const float crouched[3]={15,15,48};
 Actor_ClearPlayerMissRay_SP(start,target,mins,crouched,aboveCrouchedPlayer);
 assert(aboveCrouchedPlayer[0]==1&&aboveCrouchedPlayer[1]==0&&aboveCrouchedPlayer[2]==0);
 puts("PASS: campaign miss rays clear the player while existing misses keep their aim");
#endif
 puts("PASS: target teams, AI combat, rooftop enemies, attacker cap, script ignore/override, awareness, pacifists");
}
"""
out = root / 'build-nx/actor-target-tests'
out.mkdir(parents=True, exist_ok=True)
cpp = out / 'targets.cpp'
cpp.write_text(harness)
env = os.environ.copy()
env['PATH'] = str(Path(a.compiler).parent) + os.pathsep + env.get('PATH', '')
for mode in ['SP', 'MP']:
    exe = out / ('targets-' + mode + '.exe')
    subprocess.run([a.compiler, '-std=c++20', '-O2', *(['-DKISAK_SP'] if mode == 'SP' else []),
                    str(cpp), '-o', str(exe)], check=True, env=env)
    print(mode, flush=True)
    subprocess.run([str(exe)], check=True, env=env)
