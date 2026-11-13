"""Exercise two-peer loadout-only synchronization without launching the game."""
from pathlib import Path
import argparse,os,re,subprocess
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--compiler',default='C:/msys64/ucrt64/bin/g++.exe');a=p.parse_args()
r=Path(__file__).resolve().parents[3];out=r/'build-nx/remote-class-tests';out.mkdir(parents=True,exist_ok=True)
env=os.environ.copy();env['PATH']=str(Path(a.compiler).parent)+os.pathsep+env.get('PATH','')
def function(path,signature):
 s=(r/path).read_text();start=s.index(signature);end=s.index('{',start)+1;depth=1
 while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
 return s[start:end]
code=r'''
#include <live/custom_class_transport.h>
#include <cassert>
#include <cstdio>
#include <cstdarg>
#include <stdexcept>
#include <string>
#include <deque>
#ifndef __cdecl
#define __cdecl
#endif
#define KISAK_MP 1
#define OPENBLOPS_NO_STEAM_AUTH 1
constexpr unsigned STATS_BUFFER_SIZE=0xa000,MAX_LOCAL_CLIENTS=2;
constexpr int STATS_LOCATION_NORMAL=0,STATS_LOCATION_GLOBAL=6,NA_LOOPBACK=2,CS_CONNECTED=3,CS_CLIENTLOADING=4,CS_RECONNECTING=3;
constexpr long long ALL_STATS_PACKETS_RECEIVED=0xfffffffffLL;
enum connstate_t{CA_CHALLENGING=4,CA_SENDINGSTATS=5,CA_CONNECTED=6};
enum netsrc_t{NS_CLIENT0,NS_SERVER};
struct netadr_t{int endpoint=0,type=4;};
bool NET_CompareAdr(netadr_t a,netadr_t b){return a.endpoint==b.endpoint&&a.type==b.type;}
struct msg_t{unsigned char*data;int cursize;};
struct ddlMemberDef_t{int size=32781;};
struct ddlDef_t{int version=100,size=324000;};
struct ddlState_t{int absoluteOffset=117;ddlMemberDef_t*member;ddlDef_t*ddl;};
ddlMemberDef_t member;ddlDef_t schema;ddlState_t cac{117,&member,&schema};
ddlState_t*LiveStats_GetCacDDLState(){return &cac;}ddlDef_t*LiveStats_GetStatsDDL(){return &schema;}
struct persistentStats{unsigned char statsBuffer[STATS_BUFFER_SIZE]{};} normal,global;
persistentStats*LiveStorage_GetStatsBuffer(int n,int location,int){assert(!n);return location==STATS_LOCATION_GLOBAL?&global:&normal;}
int Com_LocalClient_GetControllerIndex(int n){assert(!n);return 0;}netsrc_t Com_LocalClient_GetNetworkID(int n){assert(!n);return NS_CLIENT0;}
struct client_t{struct{int state=CS_CONNECTED;struct{netadr_t remoteAddress;int qport=1234;}netchan;}header;
 bool bIsTestClient=false,bIsDemoClient=false;int challenge=5678,statsValidated=0,lastPacketTime=0;
 long long statPacketsReceived=0;char userinfo[8]="1",name[8]="peer";
 unsigned char stats[STATS_BUFFER_SIZE]{},globalStats[STATS_BUFFER_SIZE]{},globalStatsStable[STATS_BUFFER_SIZE]{};};
client_t clients[4];struct{client_t*clients=::clients;int time=0;}svs;
struct{struct{int integer=4;}current;}maxClients;auto*com_maxclients=&maxClients;
struct Connection{netadr_t serverAddress{100};int qport=1234,challenge=5678,lastPacketTime=0,lastPacketSentTime=0;}connection;
Connection*CL_GetLocalClientConnection(int n){assert(!n);return &connection;}
connstate_t state=CA_CHALLENGING;connstate_t CL_GetLocalClientConnectionState(int n){assert(!n);return state;}
void CL_SetLocalClientConnectionState(int n,connstate_t s){assert(!n);state=s;}
struct{int realtime=0;}cls;
const char*Info_ValueForKey(char*s,const char*){return s;}
constexpr int ERR_DROP=1;
void Com_Error(int,const char*s){throw std::runtime_error(s);}void Com_Printf(int,const char*,...){}
bool DDL_AssociateBuffer(char*b,int size,ddlDef_t*d){assert(size==int(STATS_BUFFER_SIZE));memcpy(b+4,&d->version,4);return true;}
struct Wire{netadr_t to;std::vector<unsigned char>data;};std::deque<Wire>toServer,toClient;
void NET_OutOfBandData(netsrc_t source,netadr_t to,const unsigned char*data,int size){
 assert(size+4<=1200);std::vector<unsigned char>packet(4,255);packet.insert(packet.end(),data,data+size);
 (source==NS_SERVER?toClient:toServer).push_back({to,std::move(packet)});
}
'''
s=(r/'src/live/custom_class_sync.cpp').read_text();code+=re.sub(r'^#include[^\n]*\n','',s,flags=re.M)
code+=function('src/client_mp/sv_client_mp.cpp','static void SV_InitLocalClientStats(')
code+=r'''
msg_t Message(Wire&w){return {w.data.data(),int(w.data.size())};}
int main(){
 // Only exact CAC bits travel, including unaligned first/last bytes.
 for(unsigned i=0;i<STATS_BUFFER_SIZE;++i){normal.statsBuffer[i]=(i*17+5)&255;global.statsBuffer[i]=(i*31+19)&255;}
 unsigned start,bits,bytes;assert(ClassLayout(start,bits,bytes));
 auto expected=customclasses::Pack(normal.statsBuffer,start,bits),expectedGlobal=customclasses::Pack(global.statsBuffer,start,bits);
 auto*peer=&clients[1];netadr_t peerAddress{200};peer->header.netchan.remoteAddress=peerAddress;
 assert(SV_CustomClassSyncConnect(peer)&&SV_CustomClassSyncPending(peer));
 CL_CustomClassSyncBegin(0);assert(state==CA_SENDINGSTATS);
 assert(sending[0].data.size()==2*bytes&&sending[0].data.size()<STATS_BUFFER_SIZE);
 assert((sending[0].data[bytes-1]>>(bits%8))==0); // No adjacent profile bits.
 // Wrong endpoint, challenge and slot cannot allocate or modify any receiver.
 CL_CustomClassSyncPump(0);assert(!toServer.empty());auto probe=toServer.front();auto m=Message(probe);
 assert(SV_CustomClassSyncPacket(netadr_t{201},&m));assert(receiving[1].receiver.data.empty()&&toClient.empty());
 auto wrong=probe;wrong.data[4+strlen("obclasses\n")+4]^=1;m=Message(wrong);
 assert(SV_CustomClassSyncPacket(peerAddress,&m));assert(receiving[1].receiver.data.empty());
 toServer.clear();bool droppedData=false,droppedFinalAck=false;int completedAt=-1;
 for(int tick=0;tick<4000&&state!=CA_CONNECTED;++tick){
  cls.realtime=svs.time=tick*16;CL_CustomClassSyncPump(0);
  // Deliver batches backwards, duplicate some packets and lose one data packet.
  while(!toServer.empty()){
   auto wire=std::move(toServer.back());toServer.pop_back();m=Message(wire);
   customclasses::Header h;const unsigned char*d;size_t n;
   assert(customclasses::Decode(false,wire.data.data()+4,wire.data.size()-4,h,d,n));
   if(!droppedData&&h.index==1){droppedData=true;continue;}
   assert(SV_CustomClassSyncPacket(peerAddress,&m));if(h.index%3==0)assert(SV_CustomClassSyncPacket(peerAddress,&m));
  }
  while(!toClient.empty()){
   auto wire=std::move(toClient.back());toClient.pop_back();m=Message(wire);
   customclasses::Header h;const unsigned char*d;size_t n;
   assert(customclasses::Decode(true,wire.data.data()+4,wire.data.size()-4,h,d,n));
   // Lost completion ACK forces retransmission after server validation.
   if(h.index==customclasses::Count(sending[0].header.size)&&!droppedFinalAck){droppedFinalAck=true;continue;}
   auto oldState=state;assert(CL_CustomClassSyncAck(0,netadr_t{999},&m));assert(state==oldState);
   assert(CL_CustomClassSyncAck(0,connection.serverAddress,&m));
  }
  if(state==CA_CONNECTED)completedAt=tick;
 }
 assert(completedAt>=0&&droppedData&&droppedFinalAck&&SV_CustomClassSyncReady(peer)&&!SV_CustomClassSyncPending(peer));
 assert(peer->statsValidated&&peer->statPacketsReceived==ALL_STATS_PACKETS_RECEIVED);
 assert(customclasses::Pack(peer->stats,start,bits)==expected&&customclasses::Pack(peer->globalStats,start,bits)==expectedGlobal);
 // All unrelated bytes remain server-local zero/default; never copy host data.
 for(unsigned bit=320;bit<STATS_BUFFER_SIZE*8;++bit)if(bit<start||bit>=start+bits)
  assert(!(peer->stats[bit/8]&(1u<<(bit%8)))&&!(peer->globalStats[bit/8]&(1u<<(bit%8))));
 for(unsigned i=0;i<STATS_BUFFER_SIZE;++i)assert(!clients[2].stats[i]);
 auto before=customclasses::Pack(peer->stats,start,bits);SV_InitLocalClientStats(peer);
 assert(customclasses::Pack(peer->stats,start,bits)==before); // Gamestate resend preserves CAC.
 peer->header.state=CS_CLIENTLOADING;auto late=probe;m=Message(late);toClient.clear();SV_CustomClassSyncPacket(peerAddress,&m);assert(toClient.empty());
 SV_CustomClassSyncReset(1);assert(!SV_CustomClassSyncReady(peer)&&!SV_CustomClassSyncPending(peer));
 peer->header.state=CS_CONNECTED;assert(SV_CustomClassSyncConnect(peer));assert(!SV_CustomClassSyncReady(peer));
 // Old connections, bots and loopback keep their existing behavior.
 peer->userinfo[0]=0;assert(!SV_CustomClassSyncConnect(peer));peer->userinfo[0]='1';peer->bIsTestClient=true;assert(!SV_CustomClassSyncConnect(peer));
 peer->bIsTestClient=false;peer->header.netchan.remoteAddress.type=NA_LOOPBACK;assert(!SV_CustomClassSyncConnect(peer));
 // Truncated packets, impossible lengths, checksum errors and stale ACKs.
 customclasses::Receiver corrupt;auto h=sending[0].header;h.index=0;
 assert(!corrupt.Receive(h,probe.data.data(),1,h.size));h.size=0xffffffff;assert(!corrupt.Receive(h,probe.data.data(),1,2*bytes));
 auto payload=expected;payload[0]^=1;customclasses::Sender tx;tx.Begin(payload,7,8,100);customclasses::Receiver rx;
 tx.Pump(0,[&](const std::vector<unsigned char>&packet){customclasses::Header k;const unsigned char*d;size_t n;assert(customclasses::Decode(false,packet.data(),packet.size(),k,d,n));k.checksum^=1;rx.Receive(k,d,n,k.size);});
 // Exhaust the remaining chunks while preserving the intentionally wrong hash.
 for(unsigned i=4;i<customclasses::Count(tx.header.size);++i){auto k=tx.header;k.index=i;k.checksum^=1;unsigned offset=i*customclasses::ChunkBytes;rx.Receive(k,payload.data()+offset,std::min(customclasses::ChunkBytes,k.size-offset),k.size);}
 assert(rx.failed&&!rx.complete&&rx.Ack().index==customclasses::Rejected);
 puts("PASS: joining peer's exact CAC bits, retries/reordering/duplicates, lost final ACK, endpoint/challenge isolation, no unrelated profile data, pre-spawn readiness, gamestate preservation, reconnect and malformed packets");
}
'''
cpp=out/'remote.cpp';exe=cpp.with_suffix('.exe');cpp.write_text(code)
subprocess.run([a.compiler,'-std=c++17','-Wall','-Wextra','-Werror','-I'+str(r/'src'),str(cpp),'-o',str(exe)],check=True,env=env)
subprocess.run([str(exe)],check=True,env=env)
# Integration hooks must run before native stats/service processing and gamestate.
client=(r/'src/client_mp/cl_main_mp.cpp').read_text();server=(r/'src/client_mp/sv_client_mp.cpp').read_text()
assert 'Info_SetValueForKey(info, "openblops_classes", "1")' in client
assert 'Cmd_Argv(2), "obclasses1"' in client and 'CL_CustomClassSyncBegin(localClientNum)' in client
assert 'connectResponse \\"%s\\" obclasses1' in server
assert function('src/client_mp/sv_client_mp.cpp','void __cdecl SV_SendClientGameState(').index('SV_CustomClassSyncPending')<function('src/client_mp/sv_client_mp.cpp','void __cdecl SV_SendClientGameState(').index('CS_CLIENTLOADING')
print('PASS: capability negotiation and pre-gamestate synchronization hooks wired')
