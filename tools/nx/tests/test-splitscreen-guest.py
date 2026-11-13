"""Exercise the actual opt-in local guest lifecycle without starting the game."""
from pathlib import Path
import argparse
import re
import subprocess

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler',default='/c/msys64/ucrt64/bin/g++.exe')
args=parser.parse_args()
root=Path(__file__).resolve().parents[3]
out=root/'build-nx/splitscreen-guest-tests';out.mkdir(parents=True,exist_ok=True)

def function(path,signature):
    source=(root/path).read_text(encoding='utf-8',errors='replace')
    start=source.index(signature);end=source.index('{',start)+1;depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]

code=r'''
#include <cassert>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <algorithm>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#ifndef __cdecl
#define __cdecl
#endif
#define KISAK_EXPERIMENTAL_SPLITSCREEN 1
#define KISAK_SP 1
#define OPENBLOPS_NO_STEAM_AUTH 1
using _BYTE=unsigned char;using _WORD=unsigned short;
#define __debugbreak() assert(false)
constexpr int CA_DISCONNECTED=0,CA_CHALLENGING=5,CA_CONNECTED=6,CA_PRIMED=9,CA_ACTIVE=10;
using connstate_t=int;
struct usercmd_s {int serverTime=0,weapon=0,offHandIndex=0,forwardmove=0,rightmove=0;struct {unsigned array[2]{};} button_bits;};
struct playerState_s {int clientNum=0,pm_type=0,weapon=0,weaponstate=0,weapFlags=0,cursorHint=0,cursorHintString=0,bobCycle=0,movementDir=0;float origin[3]{},velocity[3]{},viewangles[3]{};};
struct clientActive_t {
    int serverId=0,serverTime=0,cmdNumber=0,newSnapshots=0;
    struct {bool valid=false;int serverTime=0,snapFlags=0;playerState_s ps;} snap;
    usercmd_s cmds[128];float cgameOrigin[3]{},cgameVelocity[3]{},cgameViewangles[3]{};
    int cgamePredictedDataServerTime=0,cgameBobCycle=0,cgameMovementDir=0;
};
struct netadr_t {int type=0;unsigned short port=0;};
struct netchan_t {int qport=0,sock=0;netadr_t remoteAddress;};
struct clientConnection_t {netchan_t netchan;unsigned char netchanOutgoingBuffer[16]{},netchanIncomingBuffer[16]{};int qport=0,connectTime=0,lastPacketSentTime=0,lastPacketTime=0,clientNum=0,serverCommandSequence=0,lastExecutedServerCommand=0;netadr_t serverAddress;char serverCommands[128][1024]{};};
struct clientUIActive_t {int flags=0,keyCatchers=0,connectionState=0;};
clientActive_t clientStorage[2];auto clients=clientStorage;
clientConnection_t connectionStorage[2];auto clientConnections=connectionStorage;
clientUIActive_t clientUIActives[2];
struct {int realtime=1000,frametime=16;} cls;
struct dvar_t {struct {bool enabled=true;int integer=4;float value=30;} current;} running,maxclients,timeout;
auto com_sv_running=&running;auto com_maxclients=&maxclients;auto cl_timeout=&timeout;
dvar_t zombieMode;auto zombiemode=&zombieMode;
struct client_t {char name[32]{};struct {int state=0,connectionState_SP=0;netchan_t netchan;} header;};
struct {client_t clients[4];} svs;
int cl_maxLocalClients=2;
struct cg_s {int clientNum=0,lastAppliedAnimCommandTime=0;playerState_s predictedPlayerState;} gameStates[2];
struct msg_t {unsigned char *data=nullptr;int readcount=0,cursize=0,splitSize=0,maxsize=0;bool overflowed=false,readOnly=false;};
struct cmd_function_s {};
std::vector<std::string> logs;
void printlog(const char *format,va_list args){char text[1024];std::vsnprintf(text,sizeof(text),format,args);logs.emplace_back(text);}
void Com_Printf(int,const char *format,...){va_list args;va_start(args,format);printlog(format,args);va_end(args);}
void Com_PrintWarning(int,const char *format,...){va_list args;va_start(args,format);printlog(format,args);va_end(args);}
int selectedWeapon=-1,weaponSelections=0;int BG_GetNumWeapons(){return 64;}
void CL_SplitGuestSelectWeapon(unsigned w){selectedWeapon=w;++weaponSelections;}
void CL_SplitGuestResetInput(){}
void CL_SplitGuestResetMenu(){}
void CL_SplitGuestResetPresentation(){}
void CL_SplitGuestBuildInput(usercmd_s *cmd, const playerState_s *ps, int, int dt, bool enabled){
    assert(dt==16);
    if(enabled&&ps->pm_type==0){cmd->forwardmove=64;cmd->button_bits.array[0]=0x80000000u;}
}
const char *CG_SplitGuestHudStatus(){return "draw fixture";}
const char *CL_SplitGuestInputStatus(){return "input fixture";}
const char *CL_SplitGuestPredictionStatus(){return "prediction fixture";}
// Prediction itself has its own replay regression. This fixture supplies a
// distinct camera result to ensure the admission/frame path preserves it.
int predictionUpdates=0;
void CL_SplitGuestUpdatePrediction(){
 ++predictionUpdates;if(!clients[1].snap.valid)return;
 clients[1].cgameOrigin[0]=900;clients[1].cgameOrigin[1]=901;
 clients[1].cgamePredictedDataServerTime=clients[1].serverTime;
}
const char *AimAssist_SplitGuestStatus(){return "aim fixture";}
void R_WaitFrontendWorkerCmds(){}
void CG_SplitGuestFreeViewmodel(){}
int writes=0,sends=0,resends=0,userinfos=0,pures=0,disconnects=0,drains=0;
int controllers[2]{0,1};bool used[2]{true,false},zombies=true,automatic=false,systemlink=true,menu=false,dedicated=false;
bool CL_SplitScreenSessionRequested(){return automatic;}
void CL_SplitScreenSessionActivated(){}
bool Dvar_GetBool(const char *key){return !std::strcmp(key,"splitguest_auto")?automatic:systemlink;}
bool IsDedicatedServer(){return dedicated;}
bool Com_IsMenuLevel(int){return menu;}
constexpr int CS_CONNECTED=3,CS_ACTIVE=5,NA_LOOPBACK=2,SCRIPTINSTANCE_SERVER=0;
bool NET_IsLocalAddress(netadr_t address){return address.type==NA_LOOPBACK;}
int retiredGuests=0;
void SV_DropClient(client_t*c,const char*reason,bool tell,bool stats){assert(c==&svs.clients[1]&&!strcmp(reason,"EXE_DISCONNECTED")&&!tell&&!stats);c->header.state=2;++retiredGuests;}
using clientHeader_t=decltype(client_t{}.header);
int scriptResult;
void Scr_AddInt(int value,int){scriptResult=value;}
template<class... T> bool Assert_MyHandler(T...){return true;}
int CL_GetLocalClientConnectionState(int n){return clientUIActives[n].connectionState;}
clientConnection_t *CL_GetLocalClientConnection(int n){return &clientConnections[n];}
void Netchan_Setup(int source,netchan_t *channel,netadr_t from,int qport,unsigned char *,int outsize,unsigned char *,int insize){
    assert(outsize==2048&&insize==0x10000);channel->qport=qport;channel->sock=source;channel->remoteAddress=from;
}
bool NET_CompareBaseAdr(netadr_t a,netadr_t b){return a.type==b.type;}
void CL_SetLocalClientConnectionState(int n,int state){clientUIActives[n].connectionState=state;}
void CL_AddReliableCommand(int n,const char *text){assert(n==1&&!std::strcmp(text,"disconnect"));++disconnects;}
void CL_WritePacket(int n){assert(n==1);++writes;}
void CL_SendCmd(int n){assert(n==1);++sends;}
void CL_CheckForResend(int n){assert(n==1);++resends;}
void CL_CheckUserinfo(int n){assert(n==1);++userinfos;}
void CL_SendPureChecksums(int n){assert(n==1);++pures;}
void Com_LocalClient_SetBeingUsed(int n,bool value){assert(n==1);used[n]=value;}
int Com_LocalClient_GetControllerIndex(int n){return controllers[n];}
void Com_LocalClient_SetControllerIndex(int n,int value){assert(n==1);controllers[n]=value;}
int Com_LocalClient_GetNetworkID(int n){return n;}
void MSG_Init(msg_t *message,unsigned char *data,int size){*message={};message->data=data;message->maxsize=size;}
int MSG_GetByte(msg_t *message,int index){assert(index<message->cursize);return message->data[index];}
bool NET_GetLoopPacket(int n,netadr_t *,msg_t *){assert(n==1);++drains;return false;}
void NET_StringToAdr(const char *text,netadr_t *address){assert(!std::strcmp(text,"localhost"));address->type=2;}
const char *Dvar_GetString(const char *){return "zombie_theater";}
bool Com_IsZombieMap(const char *){return zombies;}
void MSG_SetDefaultUserCmd(playerState_s *ps,usercmd_s *command){*command={};command->weapon=ps->weapon;command->button_bits.array[0]=123;command->forwardmove=127;command->rightmove=-127;}
cg_s *CG_GetLocalClientGlobals(int n){return &gameStates[n];}
void Cmd_AddCommandInternal(const char *,void(*)(),cmd_function_s *){}
static bool s_splitGuestConnected;
static unsigned s_splitGuestSerial;
static int s_splitGuestStartTime;
static bool s_splitGuestViewSubmitted;
static bool s_splitGuestAutoAttempted;
'''
for signature in ['void CL_SplitGuestLog(', 'int CL_SplitGuestExpectedPlayers(', 'bool CL_IsSplitGuest(', 'static void CL_SplitGuestDrainPackets(',
    'void CL_SplitGuestRemove(', 'void CL_SplitGuestBeginMap(', 'static void CL_SplitGuestAdd_f(', 'bool CL_SplitGuestPrime(',
    'static void CL_SplitGuestRemove_f(', 'static void CL_SplitGuestStatus_f(', 'void CL_SplitGuestFrame(']:
    code+=function('src/client/splitscreen.cpp',signature)+'\n'
code+=function('src/client_mp/cl_main_mp.cpp','static void CL_SetupServerNetchan(')+'\n'
code+=function('src/server_mp/sv_main_mp.cpp','client_t *__cdecl SV_FindClientByAddress(')+'\n'
code+=function('src/qcommon/msg_mp.cpp','void __cdecl MSG_WriteShort(')+'\n'
code+=function('src/qcommon/msg_mp.cpp','int __cdecl MSG_ReadShort(')+'\n'
code+=function('src/game_mp/g_scr_main_mp.cpp','static void __cdecl GScr_GetNumConnectedPlayers_SP(')+'\n'
code+=function('src/game_mp/g_scr_main_mp.cpp','static void __cdecl GScr_GetNumExpectedPlayers_SP(')+'\n'

code+=r'''
constexpr int NS_SERVER=4;int reconnectSlot=-1;bool qportRejected=false;
const char*NET_AdrToString(netadr_t){return "loopback";}
void NET_OutOfBandPrint(int,netadr_t,const char*text){qportRejected=strstr(text,"EXE_ERR_QPORT")!=nullptr;}
void NativeReconnect(netadr_t from,int qport){
 int i=0,v24=0,v26=0;client_t*clients=nullptr,*newcl=nullptr;reconnectSlot=-1;qportRejected=false;
'''
native=(root/'src/client_mp/sv_client_mp.cpp').read_text();a=native.index('        i = 0;\n        clients = svs.clients;\n        while',native.index('void __cdecl SV_DirectConnect('));b=native.index('        startIndex = Com_GetPrivateClients();',a)
code+=native[a:b]+'return; gotnewcl: reconnectSlot=static_cast<int>(newcl-svs.clients); }\n'
code+=r'''
static void host_setup(){
    running.current.enabled=true;maxclients.current.integer=4;zombies=true;cl_maxLocalClients=2;
    clientUIActives[0].connectionState=CA_ACTIVE;
    clientUIActives[0].flags=0x55;clients[0].serverTime=5000;clients[0].serverId=42;
    clients[0].snap.ps.origin[0]=1234;clients[0].cmdNumber=77;
    clientConnections[0].qport=-1;
}
static void assert_host(int expectedTime=5000){
    assert(clientUIActives[0].connectionState==CA_ACTIVE&&clientUIActives[0].flags==0x55);
    assert(clients[0].serverTime==expectedTime&&clients[0].serverId==42);
    assert(clients[0].snap.ps.origin[0]==1234&&clients[0].cmdNumber==77);
    assert(clientConnections[0].qport==-1&&controllers[0]==0&&used[0]);
}
int main(){
    host_setup();
    CL_SplitGuestAdd_f();assert(!CL_IsSplitGuest(1)); // no menu selection, no guest
    automatic=true;
    running.current.enabled=false;CL_SplitGuestAdd_f();assert(!CL_IsSplitGuest(1));
    running.current.enabled=true;clientUIActives[0].connectionState=CA_CONNECTED;
    CL_SplitGuestAdd_f();assert(!CL_IsSplitGuest(1));clientUIActives[0].connectionState=CA_ACTIVE;
    zombies=false;CL_SplitGuestAdd_f();assert(!CL_IsSplitGuest(1));zombies=true;
    maxclients.current.integer=1;CL_SplitGuestAdd_f();assert(!CL_IsSplitGuest(1));maxclients.current.integer=4;
    cl_maxLocalClients=1;CL_SplitGuestAdd_f();assert(!CL_IsSplitGuest(1));cl_maxLocalClients=2;
    CL_SplitGuestAdd_f();assert(CL_IsSplitGuest(1)&&!CL_IsSplitGuest(0));
    assert(clientConnections[1].qport>0&&clientConnections[1].qport!=65535);
    const int firstQport=clientConnections[1].qport;
    CL_SetupServerNetchan(0,netadr_t{2,0});CL_SetupServerNetchan(1,netadr_t{2,0});
    assert(clientConnections[0].netchan.qport==-1&&clientConnections[0].netchan.sock==0);
    assert(clientConnections[1].netchan.qport==firstQport&&clientConnections[1].netchan.sock==1);
    for(int n=0;n<2;++n){svs.clients[n].header.state=3;svs.clients[n].header.netchan=clientConnections[n].netchan;}
    // Use the actual server lookup: a packet must match the qport advertised at admission.
    assert(SV_FindClientByAddress(netadr_t{2,1},clientConnections[1].netchan.qport)==&svs.clients[1]);
    assert(SV_FindClientByAddress(netadr_t{2,1},firstQport-1)==nullptr);
    assert(clientUIActives[1].connectionState==CA_CHALLENGING&&resends==1);
    assert(controllers[1]==0&&!used[1]); // shared profile, no second rendered viewport yet
    CL_SplitGuestAdd_f();assert(resends==1);assert_host();
    CL_SplitGuestFrame();assert(resends==2);assert_host();
    assert(!CL_SplitGuestPrime(0)&&pures==0&&writes==0);
    clientConnections[1].clientNum=1;
    assert(CL_SplitGuestPrime(1)&&pures==1&&writes==1);
    assert(clientUIActives[1].connectionState==CA_PRIMED);
    CL_SplitGuestFrame(); // Must send a command BEFORE receiving a snapshot.
    assert(sends==1&&clients[1].cmdNumber==1&&clients[1].cmds[1].serverTime==5000);
    assert(clients[1].cmds[1].forwardmove==0&&clients[1].cmds[1].rightmove==0);
    assert(clients[1].cmds[1].button_bits.array[0]==0&&clients[1].cmds[1].button_bits.array[1]==0);
    assert(clientUIActives[1].connectionState==CA_PRIMED);assert_host();
    auto &guest=clients[1];guest.serverId=42;guest.snap.valid=true;guest.snap.serverTime=5100;
    guest.snap.ps.weapon=17;guest.snap.ps.origin[0]=321;guest.snap.ps.origin[1]=45;
    guest.snap.ps.pm_type=5;guest.snap.ps.clientNum=1;
    clientConnections[1].serverCommandSequence=2;
    std::strcpy(clientConnections[1].serverCommands[1],"d 0 ignored-by-guest");
    std::strcpy(clientConnections[1].serverCommands[2],"a 7");
    clients[0].serverTime=5016; // Render clock advances inside the same future snapshot.
    CL_SplitGuestFrame();
    assert(clientUIActives[1].connectionState==CA_ACTIVE&&guest.cmdNumber==2);
    assert(guest.cmds[2].weapon==17&&guest.cmds[2].serverTime==5016);
    assert(predictionUpdates>0&&guest.cgameOrigin[0]==900&&guest.cgameOrigin[1]==901);
    assert(gameStates[1].clientNum==1&&gameStates[1].lastAppliedAnimCommandTime==5100);
    assert(clientConnections[1].lastExecutedServerCommand==2&&selectedWeapon==7&&weaponSelections==1);assert_host(5016);
    guest.snap.ps.pm_type=0;clients[0].serverTime=5032;
    CL_SplitGuestFrame();assert(guest.cmds[3].forwardmove==64&&guest.cmds[3].button_bits.array[0]==0x80000000u);
    assert(guest.cmds[3].serverTime==5032&&guest.cgamePredictedDataServerTime==5032);
    assert_host(5032);clients[0].serverTime=5000;
    std::strcpy(svs.clients[1].name,"erick(1)");CL_SplitGuestStatus_f();
    assert(logs.back().find("erick(1)")!=std::string::npos);
    assert(logs.back().find("321.0 45.0")!=std::string::npos);
    CL_SplitGuestRemove(true);assert(!CL_IsSplitGuest(1)&&disconnects==1&&writes==4);
    assert(clientUIActives[1].connectionState==CA_DISCONNECTED&&clients[1].cmdNumber==0);
    CL_SplitGuestRemove(true);assert(writes==4);assert_host();
    CL_SplitGuestAdd_f();assert(clientConnections[1].qport!=firstQport);
    CL_SetupServerNetchan(1,netadr_t{2,0});
    assert(clientConnections[1].netchan.qport==clientConnections[1].qport);
    cls.realtime+=30001;CL_SplitGuestFrame();assert(!CL_IsSplitGuest(1));assert_host();
    CL_SplitGuestAdd_f();CL_SplitGuestPrime(1);
    clientConnections[1].serverCommandSequence=1;std::strcpy(clientConnections[1].serverCommands[1],"w Server reason");
    CL_SplitGuestFrame();assert(!CL_IsSplitGuest(1));assert_host();
    CL_SplitGuestAdd_f();CL_SplitGuestPrime(1);clients[1].serverId=41;
    CL_SplitGuestFrame();assert(!CL_IsSplitGuest(1));assert_host();
    CL_SplitGuestAdd_f();CL_SplitGuestPrime(1);clientConnections[1].lastPacketTime=cls.realtime-31000;
    CL_SplitGuestFrame();assert(!CL_IsSplitGuest(1));assert_host();
    CL_SplitGuestAdd_f();running.current.enabled=false;CL_SplitGuestFrame();
    assert(!CL_IsSplitGuest(1));assert_host();
    running.current.enabled=true;CL_SplitGuestAdd_f();
    clients=nullptr;clientConnections=nullptr;CL_SplitGuestRemove(true);
    assert(!CL_IsSplitGuest(1));clients=clientStorage;clientConnections=connectionStorage;assert_host();
    // A restart can also interrupt admission before gamestate assigns the
    // local client number. The server-side qport/return queue still identifies it.
    host_setup();automatic=true;CL_SplitGuestAdd_f();
    assert(clientConnections[1].clientNum==0&&clientUIActives[1].connectionState==CA_CHALLENGING);
    svs.clients[1].header.state=CS_CONNECTED;
    svs.clients[1].header.netchan.qport=clientConnections[1].qport;
    svs.clients[1].header.netchan.remoteAddress={NA_LOOPBACK,1};
    const int joiningRetired=retiredGuests;CL_SplitGuestBeginMap();assert(retiredGuests==joiningRetired+1);
    // Exercise the real signed wire codec, not only an in-memory qport copy.
    // The user failure used bits 42885: joining succeeded but the first packet
    // decoded to -22651 and failed the server's address/qport lookup.
    automatic=true;
    for(unsigned bits:{1u,32767u,32768u,42885u,65535u}){
        host_setup();s_splitGuestSerial=0;
        const unsigned hostBits=(bits+65533)%65535;
        clientConnections[0].qport=static_cast<short>(hostBits);
        CL_SplitGuestAdd_f();assert(CL_IsSplitGuest(1));
        assert(static_cast<unsigned short>(clientConnections[1].qport)==bits);
        CL_SetupServerNetchan(1,netadr_t{2,0});
        svs.clients[0].header.netchan.qport=clientConnections[0].qport;
        svs.clients[1].header.state=CS_CONNECTED;
        svs.clients[1].header.netchan=clientConnections[1].netchan;
        unsigned char bytes[8];msg_t packet;MSG_Init(&packet,bytes,sizeof(bytes));
        MSG_WriteShort(&packet,clientConnections[1].netchan.qport);
        const int decoded=MSG_ReadShort(&packet);assert(!packet.overflowed);
        assert(decoded==clientConnections[1].qport);
        assert(SV_FindClientByAddress(netadr_t{2,1},decoded)==&svs.clients[1]);
        CL_SplitGuestRemove(false);
    }
    host_setup();automatic=true;CL_SplitGuestAdd_f();assert(CL_IsSplitGuest(1));
    automatic=false;CL_SplitGuestFrame();assert(!CL_IsSplitGuest(1)); // leaving the selected session removes P2
    host_setup();automatic=true;CL_SplitGuestBeginMap();
    clientUIActives[0].connectionState=CA_CONNECTED;
    CL_SplitGuestFrame();assert(!CL_IsSplitGuest(1)); // no allocation/CG re-entry during loading
    clientUIActives[0].connectionState=CA_PRIMED;
    GScr_GetNumExpectedPlayers_SP();assert(scriptResult==2);
    CL_SplitGuestFrame();assert(CL_IsSplitGuest(1)&&clientUIActives[0].connectionState==CA_PRIMED);
    const int automaticResends=resends;CL_SplitGuestFrame();assert(resends==automaticResends+1);
    for(auto &client:svs.clients){client.header.state=0;client.header.connectionState_SP=0;}
    svs.clients[0].header.state=CS_ACTIVE;svs.clients[0].header.connectionState_SP=CA_ACTIVE;
    GScr_GetNumConnectedPlayers_SP();assert(scriptResult==1);
    GScr_GetNumExpectedPlayers_SP();assert(scriptResult==2); // startup barrier stays closed
    CL_SplitGuestPrime(1);clients[1].snap.valid=true;clients[1].snap.serverTime=5000;clients[1].serverId=42;
    CL_SplitGuestFrame();assert(clientUIActives[1].connectionState==CA_ACTIVE);
    svs.clients[1].header.state=CS_ACTIVE;svs.clients[1].header.connectionState_SP=CA_ACTIVE;
    GScr_GetNumConnectedPlayers_SP();assert(scriptResult==2); // now both initial players can spawn
    // Reproduce a server-retained guest after game over. Native duplicate-join
    // admission rejects connected slots, even when the fresh qport changes.
    clientConnections[1].clientNum=1;
    svs.clients[1].header.netchan.qport=clientConnections[1].qport;
    svs.clients[1].header.netchan.remoteAddress={NA_LOOPBACK,1};
    const auto retainedHost=svs.clients[0].header;
    NativeReconnect({NA_LOOPBACK,1},clientConnections[1].qport+1);assert(qportRejected&&reconnectSlot==-1);
    const int retiredBefore=retiredGuests;
    CL_SplitGuestBeginMap();
    assert(retiredGuests==retiredBefore+1&&svs.clients[1].header.state<CS_CONNECTED);
    assert(!CL_IsSplitGuest(1)&&!s_splitGuestAutoAttempted&&automatic);
    assert(svs.clients[0].header.state==retainedHost.state);
    CL_SplitGuestFrame();assert(CL_IsSplitGuest(1));
    NativeReconnect({NA_LOOPBACK,1},clientConnections[1].qport);assert(!qportRejected&&reconnectSlot==1);
    CL_SplitGuestPrime(1);clients[1].snap.valid=true;clients[1].snap.serverTime=5000;clients[1].serverId=42;
    CL_SplitGuestFrame();assert(clientUIActives[1].connectionState==CA_ACTIVE);
    svs.clients[1].header.state=CS_ACTIVE;svs.clients[1].header.connectionState_SP=CA_ACTIVE;
    CL_SplitGuestRemove_f();const int stoppedResends=resends;
    CL_SplitGuestFrame();assert(!CL_IsSplitGuest(1)&&resends==stoppedResends);
    CL_SplitGuestBeginMap();CL_SplitGuestFrame();assert(CL_IsSplitGuest(1)); // automatic on next map
    CL_SplitGuestRemove(false);
    automatic=false;GScr_GetNumExpectedPlayers_SP();assert(scriptResult==1);
    automatic=true;systemlink=false;GScr_GetNumExpectedPlayers_SP();assert(scriptResult==1);
    systemlink=true;zombies=false;GScr_GetNumExpectedPlayers_SP();assert(scriptResult==1);
    zombies=true;maxclients.current.integer=1;GScr_GetNumExpectedPlayers_SP();assert(scriptResult==1);
    maxclients.current.integer=4;menu=true;GScr_GetNumExpectedPlayers_SP();assert(scriptResult==2);
    // Menus retain the pre-existing connected-slot counting behavior.
    menu=false;automatic=false;
    for(auto &client:svs.clients)client.header={};
    svs.clients[0].header={CS_ACTIVE,CA_ACTIVE,{0,0,{NA_LOOPBACK,0}}};
    svs.clients[1].header={CS_CONNECTED,0,{0,0,{3,0}}};
    GScr_GetNumExpectedPlayers_SP();assert(scriptResult==2);
    GScr_GetNumConnectedPlayers_SP();assert(scriptResult==1);
    svs.clients[1].header.state=CS_ACTIVE;svs.clients[1].header.connectionState_SP=CA_ACTIVE;
    GScr_GetNumConnectedPlayers_SP();assert(scriptResult==2);
    svs.clients[1].header.netchan.remoteAddress.type=NA_LOOPBACK;
    automatic=true;GScr_GetNumExpectedPlayers_SP();assert(scriptResult==2);
    svs.clients[2].header={CS_CONNECTED,0,{0,0,{3,0}}};
    GScr_GetNumExpectedPlayers_SP();assert(scriptResult==3);
    zombieMode.current.enabled=false;
    GScr_GetNumExpectedPlayers_SP();assert(scriptResult==2);
    puts("PASS: retained remote Zombies clients count during startup without double-counting loopback seats");
    puts("PASS: guest admission, initial command, snapshots, guest input delivery, signed wire qports, timeout/disconnect and host isolation");
    puts("PASS: automatic primed-stage join, two-player startup barrier, explicit removal and next-map rearming");
}
'''
cpp=out/'guest.cpp';cpp.write_text(code,encoding='utf-8')
exe=out/'guest.exe'
subprocess.run([args.compiler,'-std=c++20',str(cpp),'-o',str(exe)],check=True)
status=out/'splitguest-status.log'
if status.exists(): status.unlink()
subprocess.run([str(exe)],check=True)
text=status.read_text()
assert 'Add rejected: host is not active' in text
assert 'Status requested' in text and 'name=erick(1)' in text
assert 'First active snapshot received' in text and 'Join timed out' in text
print('PASS: persistent diagnostics written beside executable with guest state and rejection reasons')
