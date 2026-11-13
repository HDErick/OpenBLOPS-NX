"""Exercise direct-IP admission and client connection flow without starting the game."""
from pathlib import Path
import argparse
import re
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler', default='/ucrt64/bin/g++.exe')
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
out = root / 'build-nx/direct-connect-tests'
out.mkdir(parents=True, exist_ok=True)
def function(path, name):
    text = (root / path).read_text(encoding='utf-8', errors='replace')
    start = text.index(name)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]
protocol = re.search(r'#define OPENBLOPS_DIRECT_PROTOCOL "([^"]+)"', (root/'src/universal/q_shared.h').read_text()).group(1)
code = r'''
#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#ifndef __cdecl
#define __cdecl
#endif
#define __int16 short
#define __debugbreak() assert(false)
#ifdef KISAK_SP
#define OPENBLOPS_GAME_MODE "sp"
#else
#define OPENBLOPS_GAME_MODE "mp"
#endif
#define OPENBLOPS_DIRECT_PROTOCOL "PROTOCOL_VALUE"
constexpr int SND_STOP_ALL=0, CA_DISCONNECTED=0, CA_CHALLENGING=4, CA_CONNECTING=3, ERR_DROP=1, ERR_DISCONNECT=2, ERR_SERVERDISCONNECT=3, NS_SERVER=1;
struct netadr_t { int host=0; unsigned short port=0; unsigned char ip[4]{}; };
struct clientUIActive_t { int keyCatchers=0; } ui;
struct clientConnection_t { char serverMessage[256]{}; netadr_t serverAddress; int connectTime=0, connectPacketCount=0, qport=0; } connection;
struct { char servername[256]{}; } cls;
struct dvar_t { struct { bool enabled=false; int integer=32; } current; } running, authenticating, serviceError, maxclients;
using dvar_s=dvar_t;
dvar_t *com_maxclients=&maxclients;
dvar_t *com_sv_running=&running, *sv_authenticating=&authenticating, *sv_dwlsgerror=&serviceError;
int cl_serverLoadingMap, g_waitingForServer, g_qport=1234, state;
static int allocations, killed, frames, loads, steamCalls, cancelled, shutdowns, rendererStarts;
static bool steamAllowed;
std::vector<std::string> command;
int Cmd_Argc(){return static_cast<int>(command.size());}
const char *Cmd_Argv(int i){return command.at(i).c_str();}
void Com_CancelStartupConnect(){++cancelled;}
void Directory_ConnectionStarted(const char *){}
void SND_StopSounds(int){}
void CL_AllocatePerLocalClientMemory(){++allocations;}
clientUIActive_t *CL_GetLocalClientUIGlobals(int){return &ui;}
void *CL_GetLocalClientGlobals(int){return nullptr;}
clientConnection_t *CL_GetLocalClientConnection(int){return &connection;}
void SV_KillLocalServer(){++killed; running.current.enabled=false;}
void Com_ShutdownInternal(const char *){++shutdowns;running.current.enabled=false;}
void CL_InitRenderer(){++rendererStarts;}
int Com_LocalClient_GetControllerIndex(int){return 0;}
void SV_Frame(int,int){++frames;}
void FS_DisablePureCheck(int){}
void CL_Disconnect(int,int){state=CA_DISCONNECTED;}
void Con_Close(int){}
void I_strncpyz(char *to,const char *from,int size){std::snprintf(to,size,"%s",from);}
int I_strcmp(const char *a,const char *b){return std::strcmp(a,b);}
int I_stricmp(const char *a,const char *b){return strcasecmp(a,b);}
unsigned short BigShort(unsigned short n){return static_cast<unsigned short>((n<<8)|(n>>8));}
bool NET_StringToAdr(const char *text,netadr_t *adr){if(!std::strcmp(text,"bad"))return false;adr->host=!std::strcmp(text,"localhost")?0:7;return true;}
bool NET_IsLocalAddress(netadr_t a){return a.host==0;}
bool Sys_IsLANAddress(netadr_t){return true;}
bool Steam_UpdateClientAuthTicket(netadr_t){++steamCalls;return steamAllowed;}
void CL_SetLocalClientConnectionState(int,int s){state=s;}
void CL_LocalClient_ClearCUIFlag(int,int){}
void Cbuf_ExecuteBuffer(int,int,char *){}
void UI_CloseAll(int){}
static int errorsCleared, lastError;
void UI_ClearErrors(){++errorsCleared;}
void SCR_UpdateLoadScreen(){++loads;}
template<class... T> void Com_Printf(T...){}
template<class... T> void Com_PrintWarning(T...){}
template<class... T> void Com_Error(int code,T...){lastError=code;throw std::runtime_error("connection error");}
template<class... T> void _Dvar_RegisterBool(T...){}
std::map<std::string,std::string> values;
const char *Info_ValueForKey(const char *,const char *key){return values[key].c_str();}
struct challenge_t { bool connected=false; netadr_t adr; int time=0, challenge=0, firstTime=0, firstPing=0, pingTime=0; unsigned long long steamID64=0; };
constexpr int CS_CONNECTED=3, CS_CLIENTLOADING=4, CS_ACTIVE=5;
struct client_t { struct { int state=0; } header; char name[32]{}; };
struct { int time=100; challenge_t challenges[1024]; client_t clients[32]; } svs;
static const char *mapName="";
const char *Dvar_GetString(const char *name){return !std::strcmp(name,"mapname")?mapName:"Unknown Soldier";}
bool Com_IsZombieMap(const char *map){return !std::strncmp(map,"zombie_",7);}
bool Assert_MyHandler(const char*,int,int,const char*,const char*){assert(false);return true;}
char *SEH_SafeTranslateString(const char *s){return const_cast<char *>(s);}
char *UI_ReplaceConversionString(char *,const char *s){return const_cast<char *>(s);}
template<class... T> void Com_sprintf(char *out,int size,const char *format,T... args){std::snprintf(out,size,format,args...);}
bool NET_CompareAdr(netadr_t a,netadr_t b){return a.host==b.host&&a.port==b.port;}
static std::vector<std::string> packets;
static std::vector<netadr_t> recipients;
const char *va(const char *format,int n){static char buffer[128];std::snprintf(buffer,sizeof(buffer),format,n);return buffer;}
void NET_OutOfBandPrint(int,netadr_t to,const char *message){recipients.push_back(to);packets.emplace_back(message);}
int nextRandom(){static int n=7;return ++n;}
#define rand() nextRandom()
'''.replace('PROTOCOL_VALUE', protocol)
code += function('src/client_mp/cl_main_pc_mp.cpp', 'bool __cdecl CL_CDKeyValidate(') + '\n'
code += function('src/client_mp/cl_main_pc_mp.cpp', 'void __cdecl CL_Connect_f(') + '\n'
code += function('src/client_mp/cl_main_mp.cpp', 'void __cdecl CL_DisconnectError(') + '\n'
code += function('src/client_mp/cl_main_mp.cpp', 'static bool CL_ShouldSyncServiceStats(') + '\n'
code += function('src/client_mp/g_client_mp.cpp', 'void __cdecl ClientCleanName(') + '\n'
code += function('src/client_mp/sv_client_mp.cpp', 'void __cdecl SV_StringToLower(') + '\n'
code += function('src/client_mp/sv_client_mp.cpp', 'char __cdecl SV_ValidateName(') + '\n'
code += '#ifdef OPENBLOPS_NO_STEAM_AUTH\n'
code += function('src/client_mp/sv_client_mp.cpp', 'static bool SV_ChooseDirectPlayerName(') + '\n'
code += function('src/client_mp/sv_client_mp.cpp', 'static const char *SV_DirectConnectCompatibilityError(') + '\n'
code += function('src/client_mp/sv_client_mp.cpp', 'void __cdecl SV_GetChallenge(') + '\n#endif\n'
code += r'''
static void client_tests(){
    command={"connect","192.168.1.50"}; running.current.enabled=true; steamAllowed=false;
#ifdef OPENBLOPS_NO_STEAM_AUTH
    CL_Connect_f();
    assert(steamCalls==0 && state==CA_CONNECTING);
    assert(allocations==2 && loads==1 && cancelled==1);
#ifdef KISAK_SP
    assert(killed==0 && frames==0 && shutdowns==1 && rendererStarts==1);
#else
    assert(killed==1 && frames==1 && shutdowns==0 && rendererStarts==0);
#endif
    assert(connection.serverAddress.host==7 && BigShort(connection.serverAddress.port)==3074);
    assert(connection.connectTime==-99999 && connection.connectPacketCount==0 && connection.qport==g_qport);
    assert(!CL_ShouldSyncServiceStats(false) && !CL_ShouldSyncServiceStats(true));
#else
    bool rejected=false;try{CL_Connect_f();}catch(const std::runtime_error&){rejected=true;}
    assert(rejected && steamCalls==1);
    assert(CL_ShouldSyncServiceStats(false) && !CL_ShouldSyncServiceStats(true));
#endif
    command={"connect","localhost"};CL_Connect_f();assert(state==CA_CHALLENGING);
    command={"connect","bad"};bool invalid=false;
    try{CL_Connect_f();}catch(const std::runtime_error&){invalid=true;}
    assert(invalid && state==CA_DISCONNECTED);
    const int before=allocations;command={"connect"};CL_Connect_f();assert(allocations==before);
}
#ifdef OPENBLOPS_NO_STEAM_AUTH
static void name_tests(){
    auto &host=svs.clients[0];auto &joining=svs.clients[1];auto &loading=svs.clients[2];
    host.header.state=CS_ACTIVE;std::strcpy(host.name,"Same Player");
    char name[32]="Same Player";
    assert(SV_ChooseDirectPlayerName(&joining,name,sizeof(name)));
    assert(!std::strcmp(name,"Same Player(1)"));
    char displayName[32];
    ClientCleanName(name,displayName,sizeof(displayName));
    assert(!std::strcmp(displayName,"Same Player(1)"));
    joining.header.state=CS_CONNECTED;std::strcpy(joining.name,name);
    std::strcpy(name,"same player");
    assert(SV_ChooseDirectPlayerName(&loading,name,sizeof(name)));
    assert(!std::strcmp(name,"same player(2)"));
    // Repeated userinfo changes must keep the assigned suffix stable.
    std::strcpy(name,"Same Player");
    assert(SV_ChooseDirectPlayerName(&joining,name,sizeof(name)));
    assert(!std::strcmp(name,"Same Player(1)"));
    // A still-loading client reserves its name too, and never conflicts with itself.
    joining.header.state=CS_CLIENTLOADING;
    assert(!SV_ValidateName(&loading,"Same Player(1)"));
    assert(SV_ValidateName(&joining,"Same Player(1)"));
    std::strcpy(name,"Unique Player");
    assert(SV_ChooseDirectPlayerName(&loading,name,sizeof(name)));
    assert(!std::strcmp(name,"Unique Player"));
    std::strcpy(host.name,"ABCDEFGHIJKLMNOPQRSTUVWXYZ01234");
    std::strcpy(name,host.name);
    assert(SV_ChooseDirectPlayerName(&loading,name,sizeof(name)));
    assert(std::strlen(name)==31 && std::string(name).substr(28)=="(1)");
    // Suffix generation must not make reserved names acceptable.
    for(const char *reserved:{"Unknown Soldier","Bad DemoClient Name"}){
        std::strcpy(name,reserved);assert(!SV_ChooseDirectPlayerName(&loading,name,sizeof(name)));
    }
    host.header.state=joining.header.state=0;
}
static void server_tests(){
    char userinfo[1024]{};
    values={{"openblops_mode",OPENBLOPS_GAME_MODE},{"openblops_direct",OPENBLOPS_DIRECT_PROTOCOL}};
    assert(!SV_DirectConnectCompatibilityError(userinfo));
    values["openblops_mode"]=!std::strcmp(OPENBLOPS_GAME_MODE,"mp")?"sp":"mp";
    assert(SV_DirectConnectCompatibilityError(userinfo));
    values["openblops_mode"]=OPENBLOPS_GAME_MODE;
    values.erase("openblops_direct");assert(SV_DirectConnectCompatibilityError(userinfo));
    values["openblops_direct"]="old";assert(SV_DirectConnectCompatibilityError(userinfo));
    values["openblops_direct"]=OPENBLOPS_DIRECT_PROTOCOL;
    const netadr_t first{7,3074}, second{8,3074}, otherPort{7,3075};
    SV_GetChallenge(first);assert(packets.size()==1 && recipients.back().host==7);
    assert(packets.back().find("challengeResponse ")==0);
    assert(NET_CompareAdr(svs.challenges[0].adr,first));
    assert(svs.challenges[0].steamID64==0 && svs.challenges[0].pingTime==100);
    const int challenge=svs.challenges[0].challenge;
    SV_GetChallenge(first);assert(svs.challenges[0].challenge==challenge);
    SV_GetChallenge(second);assert(NET_CompareAdr(svs.challenges[1].adr,second));
    SV_GetChallenge(otherPort);assert(NET_CompareAdr(svs.challenges[2].adr,otherPort));
    assert(NET_CompareAdr(svs.challenges[0].adr,first));
    const auto sent=packets.size();authenticating.current.enabled=true;
    SV_GetChallenge(first);assert(packets.size()==sent);
    authenticating.current.enabled=false;serviceError.current.enabled=true;
    SV_GetChallenge(first);assert(packets.size()==sent);
    assert(steamCalls==0);
}
#endif
int main(){
#ifdef KISAK_SP
maxclients.current.integer=4;
#endif
client_tests();
    auto disconnect = [](const char *reason, int expected) {
        const int cleared = errorsCleared;
        try { CL_DisconnectError(reason); assert(false); }
        catch (const std::runtime_error &) { assert(lastError == expected); }
        assert(errorsCleared == cleared + (expected == ERR_DISCONNECT));
    };
    running.current.enabled = false;
    mapName = "zombie_theater";
#if defined(KISAK_SP) && defined(OPENBLOPS_NO_STEAM_AUTH)
    for (const char *reason : {"EXE_DISCONNECTED", "EXE_SERVERQUIT", "PLATFORM_DISCONNECTED_FROM_SERVER"})
        disconnect(reason, ERR_DISCONNECT);
    disconnect("EXE_SERVERKICKED", ERR_SERVERDISCONNECT);
    running.current.enabled = true;
    disconnect("EXE_DISCONNECTED", ERR_SERVERDISCONNECT);
    running.current.enabled = false;
    mapName = "vorkuta";
#endif
    disconnect("EXE_DISCONNECTED", ERR_SERVERDISCONNECT);
#ifdef OPENBLOPS_NO_STEAM_AUTH
name_tests();server_tests();
#endif
puts("client transitions, Zombies host-exit recovery, duplicate names, compatibility, service-stat policy, and challenge checks passed");}
'''
source=out/'direct.cpp';source.write_text(code)
for name,defines in [
 ('pc-direct',['-DWIN32','-DOPENBLOPS_NO_STEAM_AUTH']),
 ('nx-direct',['-DKISAK_NX','-DOPENBLOPS_NO_STEAM_AUTH']),
 ('pc-zombies-direct',['-DWIN32','-DKISAK_SP','-DOPENBLOPS_NO_STEAM_AUTH']),
 ('nx-zombies-direct',['-DKISAK_NX','-DKISAK_SP','-DOPENBLOPS_NO_STEAM_AUTH']),
 ('legacy-mp',['-DWIN32']),
 ('legacy-sp',['-DWIN32','-DKISAK_SP'])
]:
 exe=out/(name+'.exe')
 subprocess.run([args.compiler,'-std=c++20',*defines,str(source),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
 print(name+': passed',flush=True)
