"""Test automatic spectator entry and native SP follow snapshot behavior."""
from pathlib import Path
import argparse,subprocess
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--compiler',default='/c/msys64/ucrt64/bin/g++.exe');args=p.parse_args()
root=Path(__file__).resolve().parents[3];out=root/'build-nx/spectator-auto-follow-tests';out.mkdir(parents=True,exist_ok=True)
s=(root/'src/game_mp/g_active_mp.cpp').read_text()
def function(sig):
 a=s.index(sig);b=s.index('{',a)+1;depth=1
 while depth:depth+=(s[b]=='{')-(s[b]=='}');b+=1
 return s[a:b]
code=r"""
#include <cassert>
#include <cstdio>
#include <cstring>
#define KISAK_SP 1
#ifndef __cdecl
#define __cdecl
#endif
constexpr int CON_CONNECTED=2,CON_DISCONNECTED=0,SESS_STATE_PLAYING=0,SESS_STATE_DEAD=1,SESS_STATE_SPECTATOR=2;
constexpr int ET_INVISIBLE=6,HUDELEM_UPDATE_CURRENT=1;enum team_t{TEAM_FREE=0,TEAM_AXIS=1,TEAM_ALLIES=2,TEAM_NUM_TEAMS=5};
struct playerState_s{int clientNum=0,pm_type=0,viewmodelIndex=1;unsigned otherFlags=4,eFlags=0;int stats[5]{100};};
struct clientState_s{team_t team=TEAM_ALLIES;};
struct gclient_s{struct{int connected=CON_CONNECTED,sessionState=SESS_STATE_PLAYING,forceSpectatorClient=-1,enterTime=0,archiveTime=0,psOffsetTime=0,noSpectate=0;clientState_s cs;}sess;playerState_s ps;int spectatorClient=-1;float fGunPitch=0,fGunYaw=0;}clients[4];
struct gentity_s{gclient_s*client=nullptr;struct{int number=0,eType=1;}s;struct{unsigned svFlags=2;int contents=1;}r;int takedamage=1;}entities[4];
struct{int maxclients=4,time=100,startTime=0;gclient_s*clients=::clients;}level;
"""
code+=function('bool __cdecl G_ClientCanSpectateTeam(')+'\n'+function('bool __cdecl G_ClientCanSpectateTeamOrLocalPlayer(')+'\n'
code+=r"""
void StopFollowing(gentity_s*e){e->client->spectatorClient=-1;e->client->sess.forceSpectatorClient=-1;e->client->ps.otherFlags&=~2u;e->client->ps.clientNum=e->s.number;}
int Cmd_FollowCycle_f(gentity_s*,int){return 0;}
bool SV_GetArchivedClientInfo(unsigned n,int*,playerState_s*ps,clientState_s*cs,void*,int*health,int*flags){if(n>=4||clients[n].sess.connected!=CON_CONNECTED||clients[n].sess.sessionState!=SESS_STATE_PLAYING)return false;*ps=clients[n].ps;*cs=clients[n].sess.cs;*health=ps->stats[0];*flags=ps->otherFlags;return true;}
void HudElem_UpdateClient(gclient_s*,int,int){}
"""
code+=function('void __cdecl SpectatorClientEndFrame(')+'\n'
a=s.index('struct AutoSpectatorFollowState');b=s.index('void __cdecl ClientEndFrame(',a);code+=s[a:b]
code+=r"""
void frame(int n){++level.time;G_UpdateAutoSpectatorFollow(&entities[n],false);if(clients[n].sess.sessionState==SESS_STATE_SPECTATOR){SpectatorClientEndFrame(&entities[n]);G_UpdateAutoSpectatorFollow(&entities[n],true);}}
int main(){for(int i=0;i<4;++i){entities[i].client=&clients[i];entities[i].s.number=i;clients[i].ps.clientNum=i;}
 clients[2].ps.stats[0]=0;clients[3].sess.connected=CON_DISCONNECTED;
 frame(1);clients[1].sess.sessionState=SESS_STATE_SPECTATOR;frame(1);
 assert(clients[1].spectatorClient==0&&clients[1].ps.clientNum==0&&(clients[1].ps.otherFlags&2));
 StopFollowing(&entities[1]);frame(1);assert(clients[1].spectatorClient==-1); // manual freecam remains available
 clients[1].sess.sessionState=SESS_STATE_PLAYING;clients[1].ps.otherFlags=4;frame(1);clients[1].sess.sessionState=SESS_STATE_SPECTATOR;frame(1);assert(clients[1].spectatorClient==0);
 clients[0].sess.sessionState=SESS_STATE_DEAD;clients[2].ps.stats[0]=100;frame(1);assert(clients[1].spectatorClient==2);
 clients[2].sess.connected=CON_DISCONNECTED;frame(1);assert(clients[1].spectatorClient==-1&&s_autoSpectatorFollow[1].pending);
 clients[0].sess.sessionState=SESS_STATE_PLAYING;clients[1].sess.noSpectate=1<<TEAM_ALLIES;frame(1);assert(clients[1].spectatorClient==-1);
 clients[1].sess.noSpectate=0;frame(1);assert(clients[1].spectatorClient==0);
 StopFollowing(&entities[1]);clients[1].sess.sessionState=SESS_STATE_PLAYING;frame(1);clients[1].sess.sessionState=SESS_STATE_SPECTATOR;clients[1].sess.archiveTime=1000;frame(1);assert(clients[1].spectatorClient==-1);
 clients[1].sess.archiveTime=0;frame(1);assert(clients[1].spectatorClient==0);
 StopFollowing(&entities[1]);clients[1].sess.forceSpectatorClient=0;frame(1);assert(clients[1].spectatorClient==0&&clients[1].sess.forceSpectatorClient==0);
 StopFollowing(&entities[1]);frame(1);assert(clients[1].spectatorClient==0); // leaving forced camera acquires live target
 StopFollowing(&entities[1]);frame(1);assert(clients[1].spectatorClient==-1);
 ++level.startTime;frame(1);assert(clients[1].spectatorClient==0); // map reset rearms entry policy
 StopFollowing(&entities[1]);clients[0].sess.connected=CON_DISCONNECTED;clients[1].sess.enterTime++;frame(1);assert(clients[1].spectatorClient==-1);
 clients[0].sess.connected=CON_CONNECTED;frame(1);assert(clients[1].spectatorClient==0);
 puts("PASS: spectator entry follows live eligible player via native SP snapshot, waits for joins, skips dead/disconnected/self/forbidden teams, preserves manual freecam/forced/archive cameras, and rearms on respawn/map/slot reuse");}
"""
cpp=out/'spectator.cpp';exe=out/'spectator.exe';cpp.write_text(code)
subprocess.run([args.compiler,'-std=c++17','-O0',str(cpp),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
assert 'G_UpdateAutoSpectatorFollow(ent, false);' in s and 'G_UpdateAutoSpectatorFollow(ent, true);' in s
