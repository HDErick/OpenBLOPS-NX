"""Exercise actual native browser address/selection/sort functions without the game."""
from pathlib import Path
import os,subprocess,argparse
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--compiler',default='C:/msys64/ucrt64/bin/g++.exe');a=p.parse_args()
r=Path(__file__).resolve().parents[3];out=r/'build-nx/directory-tests';out.mkdir(parents=True,exist_ok=True)
def body(path,name):
 s=(r/path).read_text();start=s.index(name);end=s.index('{',start)+1;depth=1
 while depth:
  depth+=(s[end]=='{')-(s[end]=='}');end+=1
 return s[start:end]
code=r"""
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <vector>
#ifndef __cdecl
#define __cdecl
#endif
struct netadr_t {int value=0;};
struct serverInfo_t {netadr_t adr;int ping=-1,licensetype=5,punkbuster=0,bPassword=0,clients=1,region=0,mod=0,voice=0;char hostName[32]{},mapName[32]{},gameType[16]{};};
serverInfo_t backing[3];int backingCount=3,source=0;
bool isDirectory[3]{true,false,false},passwordPopup=false;int popupCalls=0,legacyCalls=0,directoryCalls=0,lastIndex=-1,thirdPerson=0;
struct DC {int contextIndex=0;};struct uiInfo_s {DC uiDC;} ui;
struct {struct {bool enabled=false;} current;} running;
auto com_sv_running=&running;
bool menuLevel=false,menuPresent=true,communityEnabled=true,populateOnOpen=false;
int menuChecks=0,browserOpens=0,browserRequests=0,refreshCalls=0;
struct menuDef_t {int window=1;} rankedMenu,unrankedMenu;
struct FakeClient {bool refreshing=false;std::vector<int> rows;bool Refreshing(){return refreshing;}const std::vector<int> &Rows(){return rows;}} fakeClient;
auto client=&fakeClient;
bool Com_IsMenuLevel(const char *){++menuChecks;return menuLevel;}
uiInfo_s *UI_GetInfo(int){return &ui;}
menuDef_t *Menus_FindByName(DC *,const char *name){if(!menuPresent)return nullptr;return std::strcmp(name,"server_browser_unranked")==0?&unrankedMenu:&rankedMenu;}
bool Window_IsVisible(int,int *w){return *w!=0;}
void Menus_CloseByName(int,DC *,const char *){}
void UI_StartServerRefresh(int,int,int){++refreshCalls;fakeClient.refreshing=true;}
bool Directory_Source(int source){return communityEnabled&&source==0;}
void OpenBrowser(){++browserRequests;}
void Com_Printf(int,const char *,...){}
bool Directory_PopulateServers(int,int);
struct {struct {int integer=0;} current;} sourceDvar;
auto ui_netSource=&sourceDvar;
struct {struct {int currentServer=0,numDisplayServers=3,serverCount=3,displayServers[3]{0,1,2};} serverStatus;} sharedUiInfo;
char info_7[1024],info_1[1024];int lastInfoIndex=-1;
bool CL_GetServerList(int s,serverInfo_t **servers,int **count){*servers=backing;*count=&backingCount;return s==0;}
bool Directory_IsRow(int s,int i){return s==0&&i>=0&&i<backingCount&&isDirectory[i];}
char *NET_AdrToString(netadr_t a){static char result[32];std::snprintf(result,sizeof(result),"127.0.0.1:%d",a.value);return result;}
void I_strncpyz(char *to,const char *from,int n){if(n>0){std::strncpy(to,from,n);to[n-1]=0;}}
serverInfo_t *LAN_GetServerPtr(int s,int i){return s==0&&i>=0&&i<backingCount?backing+i:nullptr;}
int LAN_CompareHostname(const char *a,const char *b){return std::strcmp(a,b);}
int I_stricmp(const char *a,const char *b){return std::strcmp(a,b);}
char *UI_GetMapDisplayName(const char *p){return const_cast<char *>(p);}
char *UI_GetGameTypeDisplayName(const char *p){return const_cast<char *>(p);}
int Dvar_GetInt(const char *name){return std::strcmp(name,"ui_netSource")==0?sourceDvar.current.integer:0;}
void Dvar_SetIntByName(const char *,int value){sourceDvar.current.integer=value;}
void Live_JoinWagerFromInvite(){assert(false);}
uiInfo_s *UI_UIContext_GetInfo(int){return &ui;}
int builds=0;
void UI_BuildServerDisplayList(int,uiInfo_s *,int){++builds;sharedUiInfo.serverStatus.numDisplayServers=backingCount;}
int LAN_GetServerCount(int){return backingCount;}
void UI_UpdateDisplayServers(int,uiInfo_s *);
void LAN_GetServerInfo(int,int i,char *to,int){lastInfoIndex=i;std::strcpy(to,backing[i].bPassword?"1":"0");}
char *Info_ValueForKey(char *info,const char *){return info;}
bool Menu_IsMenuOpenAndVisible(int,const char *){return passwordPopup;}
bool Directory_PromptJoinPassword(int,int){return false;}
void Menus_OpenByName(int,DC *,const char *name){
 if(std::strcmp(name,"join_password_popmenu")==0){++popupCalls;return;}
 ++browserOpens;if(populateOnOpen)Directory_PopulateServers(0,0);
}
void CG_SetThirdPerson(int){++thirdPerson;}
void Dvar_SetStringByName(const char *,char *){}
bool Directory_Join(int s,int i){if(!Directory_IsRow(s,i))return false;++directoryCalls;lastIndex=i;return true;}
void CL_Connect(serverInfo_t *server){++legacyCalls;lastIndex=int(server-backing);}
"""
code+=body('src/ui/ui_main.cpp','void __cdecl UI_UpdateDisplayServers(')
code+=body('src/openblops_directory/directory_engine.cpp','const char *Directory_BrowserMenu(')
code+=body('src/openblops_directory/directory_engine.cpp','bool TryOpenBrowser(')
code+=body('src/openblops_directory/directory_engine.cpp','bool Directory_PopulateServers(')
code+=body('src/ui/ui_feeders.cpp','void __cdecl UI_FeederSelection_Servers(')
code+=body('src/client_mp/cl_ui_pc_mp.cpp','void __cdecl LAN_GetServerAddressString(')
code+=body('src/client_mp/cl_ui_pc_mp.cpp','int __cdecl LAN_CompareServers(')
code+=body('src/ui_mp/ui_main_mp.cpp','void __cdecl UI_RunMenuScript_JoinServer(')
code+=r"""
int main(){
 char text[64]="unchanged";
 backing[0].adr.value=3074;isDirectory[0]=false;
 LAN_GetServerAddressString(0,0,text,sizeof(text));assert(std::string(text)=="127.0.0.1:3074");
 LAN_GetServerAddressString(0,-1,text,sizeof(text));assert(!text[0]);
 LAN_GetServerAddressString(0,3,text,sizeof(text));assert(!text[0]);
 text[0]='x';LAN_GetServerAddressString(0,0,text,0);assert(text[0]=='x');LAN_GetServerAddressString(0,0,nullptr,32);
 isDirectory[0]=true;LAN_GetServerAddressString(0,0,text,sizeof(text));assert(!text[0]);
 UI_RunMenuScript_JoinServer(0,0);assert(directoryCalls==1&&legacyCalls==0&&lastIndex==0);
 sharedUiInfo.serverStatus.currentServer=-1;UI_RunMenuScript_JoinServer(0,0);assert(directoryCalls==1);
 sharedUiInfo.serverStatus.currentServer=3;UI_RunMenuScript_JoinServer(0,0);assert(directoryCalls==1);
 sharedUiInfo.serverStatus.currentServer=0;sharedUiInfo.serverStatus.displayServers[0]=-1;UI_RunMenuScript_JoinServer(0,0);assert(directoryCalls==1);
 sharedUiInfo.serverStatus.displayServers[0]=3;UI_RunMenuScript_JoinServer(0,0);assert(directoryCalls==1);
 sharedUiInfo.serverStatus.displayServers[0]=1;UI_RunMenuScript_JoinServer(0,0);assert(legacyCalls==1&&lastIndex==1);
 // Selected display row zero maps to backing row TWO, not sidecar zero.
 sharedUiInfo.serverStatus.displayServers[0]=2;isDirectory[2]=true;backing[2].bPassword=1;
 UI_RunMenuScript_JoinServer(0,0);assert(popupCalls==1&&directoryCalls==1);
 passwordPopup=true;UI_RunMenuScript_JoinServer(0,0);assert(directoryCalls==2&&lastIndex==2&&legacyCalls==1);
 sharedUiInfo.serverStatus.displayServers[0]=2;
 UI_FeederSelection_Servers(0,2,0);assert(lastInfoIndex==2&&sharedUiInfo.serverStatus.currentServer==0);
 UI_FeederSelection_Servers(0,2,-1);UI_FeederSelection_Servers(0,2,3);assert(lastInfoIndex==2);
 backing[0].ping=-1;backing[1].ping=40;
 assert(LAN_CompareServers(0,8,0,0,1)>0);assert(LAN_CompareServers(0,8,1,0,1)>0);
 assert(LAN_CompareServers(0,8,0,1,0)<0);assert(LAN_CompareServers(0,8,1,1,0)<0);
 assert(LAN_CompareServers(0,8,0,-1,0)==0);
 for(const char *old:{"server_browser_ranked","server_browser_wager","server_browser_friends","server_browser_favorites","server_browser_history"})
  assert(std::string(Directory_BrowserMenu(old))=="server_browser_unranked");
 assert(std::string(Directory_BrowserMenu("main_text"))=="main_text");
 assert(Directory_BrowserMenu(nullptr)==nullptr);
 // Reproduce the PC log: no running server, empty map, valid browser assets.
 running.current.enabled=false;menuLevel=false;sourceDvar.current.integer=1;
 populateOnOpen=true;fakeClient.refreshing=false;
 assert(Directory_PopulateServers(0,0)&&browserRequests==1);
 assert(TryOpenBrowser());assert(sourceDvar.current.integer==0&&refreshCalls==1&&browserOpens==1);
 assert(menuChecks==0); // Never require a nonexistent PC menu map.
 running.current.enabled=true;menuLevel=false;assert(!TryOpenBrowser());
 menuLevel=true;fakeClient.refreshing=false;assert(TryOpenBrowser());
 menuPresent=false;running.current.enabled=false;assert(!TryOpenBrowser());
 // Mouse/focus re-population must not clear cached rows or start another RPC.
 communityEnabled=true;sourceDvar.current.integer=0;fakeClient.refreshing=false;fakeClient.rows={1};
 const int fetched=refreshCalls;
 for(int i=0;i<10;++i)assert(Directory_PopulateServers(0,0));
 assert(refreshCalls==fetched);
 // The legacy refresh UI may already be stopped with zero displayed rows.
 sharedUiInfo.serverStatus.serverCount=0;sharedUiInfo.serverStatus.numDisplayServers=0;
 const int beforeBuild=builds;UI_UpdateDisplayServers(0,&ui);
 assert(builds==beforeBuild+1&&sharedUiInfo.serverStatus.numDisplayServers==backingCount);
 sharedUiInfo.serverStatus.numDisplayServers=0;Directory_PopulateServers(0,0);
 assert(sharedUiInfo.serverStatus.numDisplayServers==backingCount&&refreshCalls==fetched);
 fakeClient.refreshing=true;Directory_PopulateServers(0,0);assert(refreshCalls==fetched);
 communityEnabled=false;assert(!Directory_PopulateServers(0,0));
 assert(std::string(Directory_BrowserMenu("server_browser_ranked"))=="server_browser_ranked");
 puts("PASS: actual native browser row zero, bounds, sidecar selection, password popup, legacy Join, cached-list recovery and unknown-ping sorting");
}
"""
env=os.environ.copy();env['PATH']=str(Path(a.compiler).parent)+os.pathsep+env.get('PATH','')
for mode in ('KISAK_MP','KISAK_SP'):
 fixture=code.replace('bool TryOpenBrowser(', 'bool Directory_ZombiesPrepareMenus(DC *){return menuPresent;}\nbool TryOpenBrowser(',1)
 cpp=out/('browser-'+mode+'.cpp');cpp.write_text(fixture);exe=cpp.with_suffix('.exe')
 subprocess.run([a.compiler,'-std=c++20','-Wall','-Wextra','-Werror','-D'+mode,str(cpp),'-o',str(exe)],check=True,env=env)
 subprocess.run([str(exe)],check=True,env=env)
