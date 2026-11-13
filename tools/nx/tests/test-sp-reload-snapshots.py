"""Reproduce an old-match snapshot reaching the SP decoder during map reload."""
from pathlib import Path
import argparse,subprocess
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--compiler',default='/c/msys64/ucrt64/bin/g++.exe');args=p.parse_args()
root=Path(__file__).resolve().parents[3];out=root/'build-nx/sp-reload-snapshot-tests';out.mkdir(parents=True,exist_ok=True)
def function(path,sig):
 s=(root/path).read_text();a=s.index(sig);b=s.index('{',a)+1;depth=1
 while depth:depth+=(s[b]=='{')-(s[b]=='}');b+=1
 return s[a:b]
parse=function('src/client_mp/cl_parse_mp.cpp','void __cdecl CL_ParseSnapshot(')
preflight=parse[:parse.index('    const char *v2;')]
reader=function('src/cgame_mp/cg_sp_anim_snapshot.inl','void CL_ReadAnimSnapshot_SP(')
length=reader[reader.index('{')+1:reader.index('    std::vector<unsigned char> bytes')]
code=r"""
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>
#include <stdexcept>
#include <cstdio>
#define __cdecl
#define KISAK_SP 1
constexpr int CA_PRIMED=9,ERR_DROP=1;
namespace SpAnimSnapshot{constexpr unsigned MaxBytes=1024*1024;}
struct msg_t{std::vector<unsigned char> bytes;int readcount=0;bool overflowed=false;};
int state=10,bodyReads=0,animationReads=0,lengthRead=-1;bool glassReady=false;
int CL_GetLocalClientConnectionState(int client){assert(client==0||client==1);return state;}
void MSG_Discard(msg_t*m){m->readcount=static_cast<int>(m->bytes.size());m->overflowed=true;}
int MSG_ReadLong(msg_t*m){int value=0;if(m->readcount+4>m->bytes.size()){m->overflowed=true;return -1;}memcpy(&value,m->bytes.data()+m->readcount,4);m->readcount+=4;return value;}
void Com_Error(int,const char*text){throw std::runtime_error(text);}
void CheckedAnimationLength(msg_t*msg){
"""+length+r"""
 ++animationReads;lengthRead=length;
}
"""+preflight+r"""
 // The rest of the decoder is represented only to expose its first resource
 // dependency: glass payload consumption disappears when old glass is freed.
 ++bodyReads;if(glassReady)MSG_ReadLong(msg);CheckedAnimationLength(msg);
}
msg_t packet(){msg_t m;for(int value:{-1,8,0,0}){auto*p=reinterpret_cast<unsigned char*>(&value);m.bytes.insert(m.bytes.end(),p,p+4);}return m;}
int main(){
 // This reproduces the logged invalid animation length when old glass bytes
 // reach the animation reader after the old glass state has been destroyed.
 auto old=packet();bool reproduced=false;try{CheckedAnimationLength(&old);}catch(const std::runtime_error&e){reproduced=strstr(e.what(),"Invalid SP animation snapshot length");}assert(reproduced);
 for(int client:{0,1})for(state=0;state<CA_PRIMED;++state){auto m=packet();CL_ParseSnapshot(client,&m);assert(m.overflowed&&m.readcount==m.bytes.size()&&bodyReads==0&&animationReads==0);}
 for(state=CA_PRIMED;state<=10;++state){glassReady=true;auto m=packet();CL_ParseSnapshot(0,&m);assert(!m.overflowed&&lengthRead==8);}
 assert(bodyReads==2&&animationReads==2);
 // The new loading gate must not suppress malformed active-game snapshots.
 state=10;glassReady=false;auto malformed=packet();bool rejected=false;
 try{CL_ParseSnapshot(1,&malformed);}catch(const std::runtime_error&e){rejected=strstr(e.what(),"Invalid SP animation snapshot length");}assert(rejected);
 puts("PASS: native SP snapshot preflight discards old packets before priming, valid gameplay reaches the decoder, malformed active animation lengths still fail");
}
"""
cpp=out/'reload.cpp';exe=cpp.with_suffix('.exe');cpp.write_text(code)
subprocess.run([args.compiler,'-std=c++17',str(cpp),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
spawn=function('src/server_mp/sv_init_mp.cpp','void __cdecl    SV_SpawnServer(')
assert spawn.index('Com_SyncThreads();')<spawn.index('CL_SplitGuestBeginMap();')<spawn.index('CL_AllocatePerLocalClientMemory')
assert spawn.index('CL_SplitGuestBeginMap();')<spawn.index('SV_ShutdownGameProgs();')
print('PASS: guest restart cleanup runs after thread synchronization and before client allocation/game shutdown')

gamestate = function('src/client_mp/cl_parse_mp.cpp', 'void __cdecl CL_ParseGamestate(')
preload = gamestate[gamestate.index('    if (!networkGuest && !com_sv_running->current.enabled'):gamestate.index('\n#endif\n    serverConfigStringChecksum')]
steps = ['CL_InitRenderer();', 'UI_SetMap(mapname, gametype);', 'UI_SetLoadingScreenMaterial(mapname);',
         'CL_SetLocalClientConnectionState(localClientNum, CA_LOADING);', 'SCR_UpdateScreen();',
         'Com_LoadLevelFastFiles(mapname);']
assert sorted(steps, key=preload.index) == steps
reload = r'''
#include <cassert>
struct Dvar { struct { bool enabled; } current; };
Dvar server{{false}}, fastfile{{true}};
Dvar *com_sv_running = &server, *useFastFile = &fastfile;
struct { bool hunkUsersStarted; } cls;
bool zoneLoaded;
int shutdowns, restarts, loads;
bool DB_IsZoneLoaded(const char *) { return zoneLoaded; }
void Com_SyncThreads() {}
void CL_ShutdownAll() { ++shutdowns; cls.hunkUsersStarted = false; }
void Com_Restart() { assert(!cls.hunkUsersStarted); ++restarts; }
void Com_UnloadFrontEnd() {}
void Com_UnloadLevelFastFiles() { zoneLoaded = false; }
void DB_SyncXAssets() {}
void CL_InitRenderer() {}
void Com_LoadLevelFastFiles(const char *) { assert(!zoneLoaded); zoneLoaded = true; ++loads; }
void ParseGamestate(bool networkGuest) {
 const char *mapname = "zombie_cosmodrome";
''' + preload + r'''
}
int main() {
 zoneLoaded = true;
 cls.hunkUsersStarted = true;
 ParseGamestate(false);
 assert(!cls.hunkUsersStarted && zoneLoaded && shutdowns == 1 && restarts == 1 && loads == 1);
 ParseGamestate(false);
 assert(shutdowns == 1 && loads == 1);
 zoneLoaded = false;
 ParseGamestate(false);
 assert(zoneLoaded && shutdowns == 2 && restarts == 2 && loads == 2);
 cls.hunkUsersStarted = true;
 ParseGamestate(true);
 server.current.enabled = true;
 ParseGamestate(false);
 server.current.enabled = false;
 fastfile.current.enabled = false;
 ParseGamestate(false);
 assert(cls.hunkUsersStarted && shutdowns == 2 && loads == 2);
}
'''
cpp = out / 'remote-gamestate-reload.cpp'
exe = cpp.with_suffix('.exe')
cpp.write_text(reload)
subprocess.run([args.compiler, '-std=c++17', str(cpp), '-o', str(exe)], check=True)
subprocess.run([str(exe)], check=True)
print('PASS: same-map remote restart shuts down old hunk users and reloads the zone before processing gamestate')

draw = r'''
#include <cassert>
#include <cstring>
enum {CA_DISCONNECTED, CA_CONNECTING=4, CA_LOADING=8, CA_PRIMED, CA_ACTIVE};
int state=CA_CONNECTING;
int CL_GetLocalClientConnectionState(int){return state;}
int I_strnicmp(const char *a,const char *b,int n){return strncasecmp(a,b,n);}
struct {struct {bool enabled=false;}current;} server;
auto *com_sv_running=&server;
struct {bool uiStarted=false;void *consoleFont=nullptr;} cls;
struct {void *loadingScreen=nullptr;} sharedUiInfo;
bool g_showLoadingScreenMenu=true;
const char *g_mapname="zombie_theater";
int scrPlaceFull=0,clears=0,artDraws=0;
const char *status=nullptr;
const float colorWhite[4]={1,1,1,1};
void SCR_ClearScreen(){++clears;}
void UI_DrawHandlePic(const int*,float x,float y,float w,float h,int ha,int va,const float*,void *art){
 assert(x==0&&y==0&&w==640&&h==480&&ha==4&&va==4&&art==sharedUiInfo.loadingScreen);
 ++artDraws;
}
void UI_FillRect(const int*,float,float,float,float,int,int,const float*){}
void UI_DrawText(const int*,char *text,int,void*,float,float,int,int,float,const float*,int){status=text;}
''' + function('src/client_mp/cl_scrn_mp.cpp', 'static bool SCR_DrawRemoteZombieLoading(') + r'''
int main(){
 cls.consoleFont=&scrPlaceFull;
 sharedUiInfo.loadingScreen=&scrPlaceFull;
 assert(SCR_DrawRemoteZombieLoading(0)&&artDraws==0&&!strcmp(status,"CONNECTING..."));
 state=CA_LOADING;
 assert(SCR_DrawRemoteZombieLoading(0)&&artDraws==1&&!strcmp(status,"LOADING ZOMBIES..."));
 cls.uiStarted=true;state=CA_PRIMED;
 assert(SCR_DrawRemoteZombieLoading(0)&&artDraws==2);
 sharedUiInfo.loadingScreen=nullptr;status=nullptr;
 assert(SCR_DrawRemoteZombieLoading(0)&&artDraws==2&&status);
 state=CA_ACTIVE;
 assert(!SCR_DrawRemoteZombieLoading(0));
 state=CA_LOADING;server.current.enabled=true;
 assert(!SCR_DrawRemoteZombieLoading(0));
 server.current.enabled=false;g_mapname="frontend";
 assert(!SCR_DrawRemoteZombieLoading(0)&&clears==4);
}
'''
cpp=out/'remote-loading.cpp';exe=cpp.with_suffix('.exe');cpp.write_text(draw)
subprocess.run([args.compiler,'-std=c++17',str(cpp),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
print('PASS: remote Zombies artwork is staged before zone preloading, draws without menus, falls back to status text and yields to gameplay')
