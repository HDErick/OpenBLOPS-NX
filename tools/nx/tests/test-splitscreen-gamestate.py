"""Verify the actual guest gamestate decoder does not reset host resources."""
from pathlib import Path
import argparse
import subprocess

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler',default='/c/msys64/ucrt64/bin/g++.exe')
args=parser.parse_args();root=Path(__file__).resolve().parents[3]
out=root/'build-nx/splitscreen-guest-tests';out.mkdir(parents=True,exist_ok=True)
s=(root/'src/client_mp/cl_parse_mp.cpp').read_text();a=s.index('void __cdecl CL_ParseGamestate(');b=s.index('void __cdecl CL_ParseWWWDownload',a);body=s[a:b]
code=r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#ifndef __cdecl
#define __cdecl
#endif
#define KISAK_SP
#define KISAK_EXPERIMENTAL_SPLITSCREEN
#define iassert(value) assert(value)
#define _BYTE unsigned char
#define ERR_DROP 1
#define MAX_CONFIGSTRINGS 16
constexpr int svc_configstring=2,svc_EOF=14,svc_dynentstate=6,svc_destructiblestate=7,svc_ropestate=8,svc_glassstate=9;
struct gameState_t {int stringOffsets[16]{};char stringData[65536]{};int dataCount=0,matchUIVisibilityFlags=0;};
struct {gameState_t gameState;float mapCenter[3]{};bool doVidRestart=false;} cls;
struct dvar_t {struct {bool enabled=true;const char *string="";} current;bool modified=false;} running,fastfiles,gameDir,paused;
using dvar_s=dvar_t;
auto com_sv_running=&running;auto useFastFile=&fastfiles;auto fs_gameDirVar=&gameDir;auto cl_paused=&paused;
struct entityState_s {int dummy=0;};
struct clientActive_t {int serverId=0;entityState_s entityBaselines[1024];} clients[2];
struct clientConnection_t {int connectPacketCount=0,serverCommandSequence=0,clientNum=0,checksumFeed=0;} connections[2];
struct msg_t {unsigned char *data=nullptr;int cursize=0;};
int sharedCallbacks=0,privateClears=0,downloads=0,worldReads=0;
int bytesIndex=0,shortsIndex=0,longsIndex=0,bitsIndex=0,stringsIndex=0;
std::vector<int> bytes{2,14,6,7,8,9},shorts{2},longs{10,7,1,123},bits{3,5};
std::vector<std::string> bigStrings{"42","guest-only-string"};
bool CL_IsSplitGuest(int client){return client==1;}
clientConnection_t *CL_GetLocalClientConnection(int client){return &connections[client];}
clientActive_t *CL_GetLocalClientGlobals(int client){return &clients[client];}
void Con_Close(int){++sharedCallbacks;}
void CL_AllocatePerLocalClientMemory(){++sharedCallbacks;}
void CL_ClearState(int client){assert(client==1);++privateClears;std::memset(&clients[client],0,sizeof(clients[client]));}
void CG_ResetRemoteAnimSnapshots_SP(){++sharedCallbacks;}
void MSG_ClearLastReferencedEntity(msg_t *){}
int MSG_ReadLong(msg_t *){return longs.at(longsIndex++);}
void MSG_ReadString(msg_t *,char *out,unsigned size){static int n=0;std::snprintf(out,size,"%s",n++==0?"zombie_theater":"coop");}
void Com_SetSpMapMode(const char *){++sharedCallbacks;}
bool DB_IsZoneLoaded(const char *){return true;}
void Com_SyncThreads(){++sharedCallbacks;}
void CL_ShutdownAll(){++sharedCallbacks;}
void Com_Restart(){++sharedCallbacks;}
void Com_UnloadFrontEnd(){++sharedCallbacks;}
void Com_UnloadLevelFastFiles(){++sharedCallbacks;}
void DB_SyncXAssets(){++sharedCallbacks;}
void CL_InitRenderer(){++sharedCallbacks;}
void Com_LoadLevelFastFiles(const char *){++sharedCallbacks;}
bool CCS_ShouldLoadConstConfigStrings(int){return true;}
void CCS_LoadConstantConfigStrings(const char *,const char *){++sharedCallbacks;}
void CCS_ClearConstantConfigStrings(){++sharedCallbacks;}
int CCS_GetChecksum(){return 7;}
void CCS_ValidateChecksums(int a,int b){assert(a==7&&b==7);}
int CCS_GetConfigStringNum(int index){return index==0?1:0;}
const char *CCS_GetConfigStringValue(int){return "constant";}
int MSG_ReadByte(msg_t *){return bytes.at(bytesIndex++);}
int MSG_ReadShort(msg_t *){return shorts.at(shortsIndex++);}
int MSG_ReadBit(msg_t *){return 0;}
int MSG_ReadBits(msg_t *,unsigned width){assert(width==12);return bits.at(bitsIndex++);}
char *MSG_ReadBigString(msg_t *){return bigStrings.at(stringsIndex++).data();}
int MSG_ReadEntityIndex(msg_t *,unsigned){assert(false);return 0;}
void MSG_ReadDeltaEntity(msg_t *,int,entityState_s *,entityState_s *,unsigned){assert(false);}
void CL_ParseMapCenter(){++sharedCallbacks;}
template<class... T> void Com_Printf(T...){}
template<class... T> void Com_PrintError(T...){}
template<class... T> void Com_Error(T...){assert(false);}
int FS_FOpenFileWrite(char *){assert(false);return 0;}
void FS_Write(char *,int,int){assert(false);}
void FS_FCloseFile(int){assert(false);}
void MSG_Discard(msg_t *){assert(false);}
void CL_ServerIdChanged(int){++sharedCallbacks;}
void CL_SystemInfoChanged(int){++sharedCallbacks;}
bool FS_NeedRestart(int){++sharedCallbacks;return false;}
void UI_CloseAll(int){++sharedCallbacks;}
void FS_Restart(int,int){++sharedCallbacks;}
void CL_InitDownloads(int client){assert(client==1);++downloads;}
void DynEnt_ReadGameState(msg_t *,int client){assert(client==1);++worldReads;}
void Destructible_ReadGameState(msg_t *,int client){assert(client==1);++worldReads;}
void Rope_ReadGameState(msg_t *,int client){assert(client==1);++worldReads;}
void GlassCl_ReadGameState(int client,msg_t *){assert(client==1);++worldReads;}
bool UI_Gametype_Custom_ReadClientData(msg_t *){return false;}
bool UI_Gametype_CanUseCustom(){return true;}
void Dvar_SetInt(dvar_t *,int){++sharedCallbacks;}
'''+body+r'''
int main(){
    std::memset(&cls.gameState,0x45,sizeof(cls.gameState));
    const gameState_t before=cls.gameState;
    cls.mapCenter[0]=100;cls.mapCenter[1]=200;cls.mapCenter[2]=300;
    clients[0].serverId=42;connections[0].clientNum=0;connections[0].checksumFeed=123;
    msg_t message;CL_ParseGamestate(1,&message);
    assert(!std::memcmp(&before,&cls.gameState,sizeof(before)));
    assert(cls.mapCenter[0]==100&&cls.mapCenter[1]==200&&cls.mapCenter[2]==300);
    assert(clients[0].serverId==42&&connections[0].clientNum==0&&connections[0].checksumFeed==123);
    assert(sharedCallbacks==0&&privateClears==1&&downloads==1&&worldReads==4);
    assert(connections[1].clientNum==1&&connections[1].checksumFeed==123&&clients[1].serverId==42);
    assert(bytesIndex==6&&longsIndex==4&&shortsIndex==1&&bitsIndex==2&&stringsIndex==2);
    puts("PASS: actual guest gamestate decoding preserves host configstrings, map state, allocator, renderer, audio and UI lifecycle");
}
'''
cpp=out/'gamestate.cpp';cpp.write_text(code,encoding='utf-8');exe=out/'gamestate.exe'
subprocess.run([args.compiler,'-std=c++20',str(cpp),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
