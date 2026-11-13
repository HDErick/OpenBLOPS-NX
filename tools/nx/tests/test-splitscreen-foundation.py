"""Test actual local-client foundation code with one/two slots; no game launch."""
from pathlib import Path
import argparse
import re
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler', default='/c/msys64/ucrt64/bin/g++.exe')
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
out = root / 'build-nx/splitscreen-foundation-tests'
out.mkdir(parents=True, exist_ok=True)

def read(path):
    return (root/path).read_text(encoding='utf-8', errors='replace')

def function(path, signature):
    source=read(path); start=source.index(signature); end=source.index('{',start)+1; depth=1
    while depth:
        depth += (source[end]=='{')-(source[end]=='}'); end+=1
    return source[start:end]

def without_includes(source):
    return re.sub(r'^\s*#(?:include[^\n]*|pragma once)\n','',source,flags=re.M)

msg=read('src/qcommon/msg_mp.h'); start=msg.index('enum netsrc_t'); end=msg.index('};',start)+2
code=r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include <stdexcept>
#include <algorithm>
#include <universal/local_client_limits.h>
#ifndef __cdecl
#define __cdecl
#endif
#define __int8 char
#define __int16 short
#define __debugbreak() throw std::runtime_error("unexpected engine assertion")
#define iassert(value) assert(value)
#define bcassert(index,count) do { if((unsigned)(index)>=(unsigned)(count)) throw std::out_of_range("client index"); } while(0)
template<class... T> bool Assert_MyHandler(T...){return false;}
'''+msg[start:end]+r'''
enum connstate_t {CA_DISCONNECTED,CA_CINEMATIC,CA_UICINEMATIC,CA_LOGO,CA_CONNECTING,
    CA_CHALLENGING,CA_CONNECTED,CA_SENDINGSTATS,CA_LOADING,CA_PRIMED,CA_ACTIVE};
struct clientUIActive_t {int flags=0,keyCatchers=0;connstate_t connectionState=CA_DISCONNECTED;int nextScrollTime=0;};
clientUIActive_t clientUIActives[MAX_LOCAL_CLIENTS];
struct clientActive_t {int marker;};
struct clientConnection_t {unsigned char bytes[720800];};
std::vector<clientConnection_t> connectionStorage(MAX_LOCAL_CLIENTS);
clientConnection_t *clientConnections=connectionStorage.data();
struct LargeLocal {std::vector<unsigned char> bytes;explicit LargeLocal(int n):bytes(n){} unsigned char *GetBuf(){return bytes.data();}};
'''
code+=without_includes(read('src/qcommon/com_clients.h'))+'\n'
code+=without_includes(read('src/qcommon/com_clients.cpp'))+'\n'
code+=without_includes(read('src/client/client.h'))+'\n'
code+=function('src/cgame_mp/cg_main_mp.cpp','clientConnection_t *__cdecl CL_GetLocalClientConnection(')+'\n'
code+=function('src/cgame_mp/cg_newDraw_mp.cpp','clientUIActive_t *__cdecl CL_GetLocalClientUIGlobals(')+'\n'
code+=function('src/cgame_mp/cg_consolecmds_mp.cpp','bool __cdecl CL_LocalClient_IsCUIFlagSet(')+r'''
clientActive_t localStates[MAX_LOCAL_CLIENTS];
clientActive_t *clients=localStates;
constexpr int NA_BOT=0,NA_LOOPBACK=2;
constexpr unsigned MAX_PACKETLEN=1264;
struct netadr_t {int type=NA_LOOPBACK;unsigned char ip[4]{};unsigned short port=0,pad=0;int addrHandleIndex=0;};
struct msg_t {unsigned char *data;int maxsize,cursize=0;netsrc_t targetLocalNetID=NS_PACKET;};
struct loopmsg_t {unsigned char data[1264];int datalen,port;};
struct loopback_t {loopmsg_t msgs[16]{};volatile unsigned get=0,send=0;};
loopback_t loopbacks[NS_PACKET];
unsigned _InterlockedExchangeAdd(volatile unsigned *p,unsigned n){const unsigned before=*p;*p=before+n;return before;}
'''
for signature in ['int __cdecl NET_GetLoopPacket_Real(','void __cdecl NET_SendLoopPacket(']:
    code+=function('src/qcommon/net_chan_mp.cpp',signature)+'\n'
code+='#include <client/splitscreen.h>\n'
code+=r'''
bool uiFullscreen[MAX_LOCAL_CLIENTS]{};
bool UI_IsFullscreen(int client){return uiFullscreen[client];}
'''
for signature in ['int __cdecl CL_LocalClient_GetActiveCount(', 'char __cdecl CL_LocalClient_IsActive(',
    'char __cdecl CL_LocalClient_IsFirstActive(', 'char __cdecl CL_AllLocalClientsDisconnected(',
    'char __cdecl CL_AnyLocalClientStateActive(', 'char __cdecl CL_AnyLocalClientChallenging(',
    'int __cdecl CL_LocalActiveIndexFromClientNum(']:
    code+=function('src/client/splitscreen.cpp',signature)+'\n'
code+=r'''
struct {struct {bool enabled=true;} current;} running;
auto com_sv_running=&running;
std::vector<int> receivedClients;
int Sys_Milliseconds(){return 0;}
bool IsDedicatedServer(){return false;}
void MSG_Init(msg_t *msg,unsigned char *data,int size){msg->data=data;msg->maxsize=size;}
int NET_GetLoopPacket(netsrc_t sock,netadr_t *from,msg_t *msg){return NET_GetLoopPacket_Real(sock,from,msg);}
void CL_PacketEvent(int client,netadr_t,msg_t *msg,int,bool){assert(msg->data[0]==client+60);receivedClients.push_back(client);}
bool NET_GetDeferredClientPacket(netadr_t *,msg_t *){return false;}
bool NET_GetClientPacket(netadr_t *,msg_t *){return false;}
bool Sys_SocketPool_GetPacket(netadr_t *,msg_t *){return false;}
void Sys_CheckForNATOverflow(){}
void Com_PacketEventLoop(int,msg_t *);
void Com_DispatchClientPacketEvent(netadr_t,msg_t *){}
'''
code+=function('src/qcommon/common.cpp','void Com_ClientPacketEvent()')+'\n'
code+=function('src/qcommon/common.cpp','void __cdecl Com_PacketEventLoop(')+'\n'
code+=r'''
static void metadata_tests(){
    Com_InitClientGameStates();
    assert(Com_LocalClients_GetUsedControllerCount()==0);
    assert(CL_LocalClient_GetActiveCount()==1 && CL_LocalClient_IsFirstActive(0));
    Com_LocalClients_CompressClients();
    for(int i=0;i<MAX_LOCAL_CLIENTS;++i){
        assert(!Com_LocalClient_IsBeingUsed(i)&&!Com_LocalClient_IsPrimary(i));
        assert(Com_LocalClient_GetControllerIndex(i)==i);
        assert(Com_LocalClient_GetUIContextIndex(i)==0);
        assert(Com_LocalClient_GetNetworkID(i)==i&&Com_LocalClient_GetNetworkID(i)!=NS_SERVER);
        assert(Com_ControllerIndex_GetLocalClientNum(i)==i);
        Com_LocalClient_SetBeingUsed(i,true);
        localStates[i].marker=100+i;
        assert(CL_GetLocalClientGlobals(i)==&localStates[i]);
        assert(CL_GetLocalClientConnection(i)==&clientConnections[i]);
        assert(CL_GetLocalClientUIGlobals(i)==&clientUIActives[i]);
        clientUIActives[i].flags=1<<i;
        assert(CL_LocalClient_IsCUIFlagSet(i,1<<i));
        assert(!CL_LocalClient_IsCUIFlagSet(i,1<<(i+1)));
        assert(CL_GetLocalClientGlobals(i)->marker==100+i);
        clientUIActives[i].connectionState=i==0?CA_ACTIVE:CA_PRIMED;
        assert(CL_GetLocalClientConnectionState(i)==(i==0?CA_ACTIVE:CA_PRIMED));
    }
    assert(Com_LocalClients_GetUsedControllerCount()==MAX_LOCAL_CLIENTS);
    assert(CL_LocalClient_GetActiveCount()==MAX_LOCAL_CLIENTS);
    for(int i=0;i<MAX_LOCAL_CLIENTS;++i){
        assert(CL_LocalActiveIndexFromClientNum(i)==i);
        assert(CL_LocalClient_IsFirstActive(i)==(i==0));
    }
    assert(CL_AnyLocalClientStateActive() && !CL_AllLocalClientsDisconnected());
#if MAX_LOCAL_CLIENTS==2
    clientUIActives[0].connectionState=CA_DISCONNECTED;
    clientUIActives[1].connectionState=CA_CHALLENGING;
    uiFullscreen[0]=true;
    assert(CL_AnyLocalClientChallenging() && !CL_AllLocalClientsDisconnected());
    uiFullscreen[0]=false;
#endif
    Com_LocalClient_SetPrimary(0,true);assert(Com_LocalClients_GetPrimary()==0);
    Com_LocalClients_AssignUIContextsForInGame();
    for(int i=0;i<MAX_LOCAL_CLIENTS;++i) assert(Com_LocalClient_GetUIContextIndex(i)==i);
    Com_LocalClients_AssignUIContextsForFrontEnd();
    for(int i=0;i<MAX_LOCAL_CLIENTS;++i) assert(Com_LocalClient_GetUIContextIndex(i)==0);
#if MAX_LOCAL_CLIENTS == 2
    Com_LocalClient_SetPrimary(1,true);
    assert(Com_LocalClients_GetPrimary()==1&&!Com_LocalClient_IsPrimary(0));
    Com_LocalClient_SetBeingUsed(1,false);
    assert(Com_LocalClient_IsBeingUsed(0)&&Com_LocalClients_GetUsedControllerCount()==1);
    std::memset(clientConnections[0].bytes,0xAA,sizeof(clientConnection_t));
    std::memset(clientConnections[1].bytes,0xBB,sizeof(clientConnection_t));
    SwapClients(0,1);
    for(auto c:clientConnections[0].bytes) assert(c==0xBB);
    for(auto c:clientConnections[1].bytes) assert(c==0xAA);
    assert(!Com_LocalClient_IsBeingUsed(0)&&Com_LocalClient_IsBeingUsed(1));
    assert(Com_LocalClient_GetNetworkID(0)==1&&Com_LocalClient_GetNetworkID(1)==0);
    Com_InitClientGameStates();assert(Com_LocalClients_GetUsedControllerCount()==0);
#endif
    for(int index:{-1,MAX_LOCAL_CLIENTS}){
        bool rejected=false;
        try{CL_GetLocalClientGlobals(index);}catch(const std::out_of_range&){rejected=true;}
        assert(rejected);rejected=false;
        try{Com_LocalClient_GetNetworkID(index);}catch(const std::out_of_range&){rejected=true;}
        assert(rejected);
    }
    clients=nullptr;assert(CL_GetLocalClientGlobals(0)==nullptr);clients=localStates;
}
static void queue_tests(){
    static_assert(NS_SERVER==MAX_LOCAL_CLIENTS&&NS_PACKET==MAX_LOCAL_CLIENTS+1);
    unsigned char buffer[1264];msg_t msg{buffer,sizeof(buffer)};netadr_t from,to;
    for(int i=0;i<MAX_LOCAL_CLIENTS;++i){
        unsigned char command=10+i;
        NET_SendLoopPacket(static_cast<netsrc_t>(i),1,&command,to);
    }
    for(int i=0;i<MAX_LOCAL_CLIENTS;++i){
        assert(NET_GetLoopPacket_Real(NS_SERVER,&from,&msg));
        assert(buffer[0]==10+i&&from.port==i&&from.type==NA_LOOPBACK);
        assert(msg.targetLocalNetID==NS_SERVER);
        unsigned char snapshot=30+i;NET_SendLoopPacket(NS_SERVER,1,&snapshot,from);
    }
    assert(!NET_GetLoopPacket_Real(NS_SERVER,&from,&msg));
    for(int i=0;i<MAX_LOCAL_CLIENTS;++i){
        assert(NET_GetLoopPacket_Real(static_cast<netsrc_t>(i),&from,&msg));
        assert(buffer[0]==30+i&&msg.targetLocalNetID==i&&from.port==0);
        assert(!NET_GetLoopPacket_Real(static_cast<netsrc_t>(i),&from,&msg));
    }
    to.port=NS_SERVER;
    NET_SendLoopPacket(NS_SERVER,1,buffer,to);
    NET_SendLoopPacket(NS_PACKET,1,buffer,to);
    NET_SendLoopPacket(NS_CLIENT1,1265,buffer,to);
    assert(!NET_GetLoopPacket_Real(NS_SERVER,&from,&msg));
    assert(!NET_GetLoopPacket_Real(NS_PACKET,&from,&msg));
    for(int i=0;i<MAX_LOCAL_CLIENTS;++i){
        to.port=i;
        for(unsigned char n=0;n<20;++n) NET_SendLoopPacket(NS_SERVER,1,&n,to);
    }
    for(int i=0;i<MAX_LOCAL_CLIENTS;++i){
        for(int n=4;n<20;++n){
            assert(NET_GetLoopPacket_Real(static_cast<netsrc_t>(i),&from,&msg));assert(buffer[0]==n);
        }
        assert(!NET_GetLoopPacket_Real(static_cast<netsrc_t>(i),&from,&msg));
    }
    to.port=0;NET_SendLoopPacket(NS_SERVER,2,buffer,to);msg.maxsize=1;
    assert(!NET_GetLoopPacket_Real(NS_CLIENT1,&from,&msg));
    Com_InitClientGameStates();
    for(int i=0;i<MAX_LOCAL_CLIENTS;++i){
        to.port=i;unsigned char packet=60+i;
        NET_SendLoopPacket(NS_SERVER,1,&packet,to);
    }
    Com_ClientPacketEvent();
    assert(receivedClients.size()==MAX_LOCAL_CLIENTS);
    for(int i=0;i<MAX_LOCAL_CLIENTS;++i) assert(receivedClients[i]==i);
}
static void viewport_tests(){
    LocalClientViewport a,b;
    assert(CL_CalculateLocalClientViewport(1280,720,0,1,&a));
    assert(a.x==0&&a.y==0&&a.width==1280&&a.height==720);
    for(int height:{720,721}){
        assert(CL_CalculateLocalClientViewport(1280,height,0,2,&a));
        assert(CL_CalculateLocalClientViewport(1280,height,1,2,&b));
        assert(a.y==0&&a.height==height/2&&b.y==a.height);
        assert(a.height+b.height==height&&a.width==b.width&&b.width==1280);
    }
    assert(!CL_CalculateLocalClientViewport(1280,720,2,2,&a));
    assert(!CL_CalculateLocalClientViewport(1280,720,-1,2,&a));
    assert(!CL_CalculateLocalClientViewport(1280,720,0,3,&a));
    assert(!CL_CalculateLocalClientViewport(0,720,0,2,&a));
    assert(!CL_CalculateLocalClientViewport(1280,1,0,2,&a));
    assert(!CL_CalculateLocalClientViewport(1280,720,0,2,nullptr));
}
int main(){metadata_tests();queue_tests();viewport_tests();puts("PASS: client-state isolation, metadata reset, connection copies, loopback routing and horizontal viewports");}
'''
cpp=out/'foundation.cpp';cpp.write_text(code,encoding='utf-8')
for count in (1,2):
    for mode,defines in [('mp',[]),('sp',['-DKISAK_SP'])]:
        exe=out/f'{mode}-{count}-clients.exe'
        subprocess.run([args.compiler,'-std=c++20',f'-DMAX_LOCAL_CLIENTS={count}',*defines,'-I'+str(root/'src'),str(cpp),'-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True)
        print(f'{mode}, {count} local clients: passed',flush=True)

# Actual entity/viewmodel/effect accessors and model handle allocation/free.
models=r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <universal/local_client_limits.h>
#ifndef __cdecl
#define __cdecl
#endif
#define __int8 char
#define __int16 short
#define iassert(value) assert(value)
// Match the engine assertion's less-than comparison; call sites cast signed indices.
#define bcassert(index,count) assert((index)<(count))
#define ARRAY_COUNT(array) (sizeof(array)/sizeof((array)[0]))
#define __debugbreak() assert(false)
template<class... T> bool Assert_MyHandler(T...){return false;}
constexpr int MAX_LOCAL_CENTITIES=1536,CRITSECT_DOBJ_ALLOC=1,ERR_DROP=1;
struct DObj {int owner=-1;bool freed=false;};
struct DObjModel_s {};
struct XAnimTree_s {};
struct XAnim_s {};
struct weaponInfo_s {int marker=0;};
struct centity_s {int marker=0;};
struct fake_centity_s {centity_s cent;};
struct cg_s {struct {unsigned clientNum=0;} predictedPlayerState;};
struct cgs_t {int marker=0;};
cg_s cgStorage[MAX_LOCAL_CLIENTS];cg_s *cgArray=cgStorage;
cgs_t cgsStorage[MAX_LOCAL_CLIENTS];cgs_t *cgsArray=cgsStorage;
centity_s entities[MAX_LOCAL_CLIENTS][1024];centity_s *cg_entitiesArray[MAX_LOCAL_CLIENTS];
fake_centity_s fakeEntities[MAX_LOCAL_CLIENTS*512];fake_centity_s *cg_fakeEntitiesArray=fakeEntities;
'''
for sig in ['inline centity_s *__cdecl CG_GetEntity(', 'inline cg_s *CG_GetLocalClientGlobals(', 'inline cgs_t *CG_GetLocalClientStaticGlobals(']:
    models+=function('src/cgame_mp/cg_local_mp.h',sig)+'\n'
models+=without_includes(read('src/cgame/cg_local.h'))+r'''
ViewModelInfo viewModels[MAX_LOCAL_CLIENTS];ViewModelInfo *cg_viewModelArray=viewModels;
weaponInfo_s weapons[MAX_LOCAL_CLIENTS][2048];weaponInfo_s *cg_weaponsArray[MAX_LOCAL_CLIENTS];
struct FxSystemContainer {int marker=0;};
struct FxSystemBuffers {int marker=0;};
FxSystemContainer systems[MAX_LOCAL_CLIENTS];FxSystemContainer *fx_systemPool=systems;
FxSystemBuffers buffers[MAX_LOCAL_CLIENTS];FxSystemBuffers *fx_systemBufferPool=buffers;

int criticalDepth=0;
void Sys_EnterCriticalSection(int){++criticalDepth;}
void Sys_LeaveCriticalSection(int){assert(criticalDepth==1);--criticalDepth;}
void Com_Memset(unsigned *to,int value,unsigned size){std::memset(to,value,size);}
void Com_Error(int,const char *){throw std::runtime_error("DObj allocation failed");}
void DObjCreateExt(DObjModel_s *,unsigned short,XAnimTree_s *,unsigned char *out,unsigned,int,bool,unsigned char owner){auto obj=reinterpret_cast<DObj *>(out);obj->owner=owner;obj->freed=false;}
void DObjFree(DObj *obj){obj->freed=true;}
'''
models+=without_includes(read('src/qcommon/dobj_management.cpp').split('DObj *__cdecl Com_GetServerDObj(',1)[0])+'\n'
for sig in ['FxSystemContainer *__cdecl FX_GetSystem(', 'FxSystemBuffers *__cdecl FX_GetSystemBuffers(']:
    models+=function('src/EffectsCore/fx_system.cpp',sig)+'\n'
# Creation needs the lookup and allocator declared first.
for sig in ['void __cdecl Com_InitDObj(', 'DObj *__cdecl Com_GetClientDObj(', 'int __cdecl Com_GetFreeDObjIndex(', 'DObj *__cdecl Com_ClientDObjCreate(', 'void __cdecl Com_SafeClientDObjFree(']:
    models+=function('src/qcommon/dobj_management.cpp',sig)+'\n'
models+=r'''
int main(){
    Com_InitDObj();DObjModel_s model;
    for(int client=0;client<MAX_LOCAL_CLIENTS;++client){
        cg_entitiesArray[client]=entities[client];cg_weaponsArray[client]=weapons[client];
        assert(CG_GetEntity(client,0)==&entities[client][0]);
        assert(CG_GetEntity(client,1024)==&fakeEntities[client*512].cent);
        assert(CG_GetEntity(client,1535)==&fakeEntities[client*512+511].cent);
        assert(CG_GetLocalClientGlobals(client)==&cgStorage[client]);
        assert(CG_GetLocalClientStaticGlobals(client)==&cgsStorage[client]);
        assert(CG_GetLocalClientViewModelInfo(client)==&viewModels[client]);
        assert(CG_GetLocalClientWeaponInfo(client,123)==&weapons[client][123]);
        assert(FX_GetSystem(client)==&systems[client]);
        assert(FX_GetSystemBuffers(client)==&buffers[client]);
        for(unsigned handle:{0u,1536u}){
            auto obj=Com_ClientDObjCreate(&model,1,nullptr,handle,client);
            assert(obj&&obj->owner==client&&Com_GetClientDObj(handle,client)==obj);
        }
    }
#if MAX_LOCAL_CLIENTS==2
    assert(Com_GetClientDObj(0,0)!=Com_GetClientDObj(0,1));
    assert(Com_GetClientDObj(1536,0)!=Com_GetClientDObj(1536,1));
    auto host=Com_GetClientDObj(1536,0);
    auto guest=Com_GetClientDObj(1536,1);
    Com_SafeClientDObjFree(1536,1);
    assert(guest->freed&&!host->freed&&Com_GetClientDObj(1536,0)==host);
    assert(!Com_GetClientDObj(1536,1));
    auto replacement=Com_ClientDObjCreate(&model,1,nullptr,1536,1);
    assert(replacement!=host&&!host->freed);
#endif
    for(int client=0;client<MAX_LOCAL_CLIENTS;++client){
        Com_SafeClientDObjFree(0,client);Com_SafeClientDObjFree(1536,client);
        Com_SafeClientDObjFree(1536,client); // Repeated frees must not alter the count.
    }
    assert(objFreeCount==3071&&criticalDepth==0);
    for(auto index:clientObjMap) assert(index==0);
    cg_viewModelArray=nullptr;fx_systemPool=nullptr;fx_systemBufferPool=nullptr;
    assert(!CG_GetLocalClientViewModelInfo(0)&&!FX_GetSystem(0)&&!FX_GetSystemBuffers(0));
    puts("PASS: independent real/fake entities, weapons, viewmodels, FX and model creation/free");
}
'''
cpp=out/'models.cpp';cpp.write_text(models,encoding='utf-8')
for count in (1,2):
    exe=out/f'models-{count}-clients.exe'
    subprocess.run([args.compiler,'-std=c++20',f'-DMAX_LOCAL_CLIENTS={count}','-I'+str(root/'src'),str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
    print(f'model isolation, {count} local clients: passed',flush=True)
