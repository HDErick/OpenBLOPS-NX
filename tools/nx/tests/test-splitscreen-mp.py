"""Exercise private MP activation, P2 native-menu ownership and native server menu responses."""
from pathlib import Path
import argparse,re,subprocess,zlib
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--compiler',default='/c/msys64/ucrt64/bin/g++.exe');args=p.parse_args()
root=Path(__file__).resolve().parents[3];out=root/'build-nx/splitscreen-mp-tests';out.mkdir(parents=True,exist_ok=True)
def function(n,sig):
 s=(root/n).read_text();a=s.index(sig);b=s.index('{',a)+1;d=1
 while d:d+=(s[b]=='{')-(s[b]=='}');b+=1
 return s[a:b]
code=r"""
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <sstream>
#include <cstdarg>
#include <iomanip>
#define __cdecl
#define KISAK_EXPERIMENTAL_SPLITSCREEN 1
constexpr int CA_ACTIVE=10,GPAD_PORT_COUNT=4,UIMENU_NONE=0,UIMENU_INGAME=2;
enum GamePadButton{GPAD_A,GPAD_B,GPAD_START,GPAD_UP,GPAD_DOWN,GPAD_LEFT,GPAD_RIGHT};
enum Stick{GPAD_LX,GPAD_LY};
enum Keys{K_ESCAPE=27,K_BUTTON_A=1,K_BUTTON_B=2,K_UPARROW=154,K_DOWNARROW,K_LEFTARROW,K_RIGHTARROW};
bool automatic=true,systemlink=true,online=false,guest=true,foreground=true,pad=true,gpad=true;std::string map="mp_nuked";
int port=1,connection[2]{10,10};float buttons[7]{},sticks[2]{};
bool CL_SplitScreenSessionRequested(){return automatic;}
bool Dvar_GetBool(const char*n){return !strcmp(n,"splitguest_auto")?automatic:!strcmp(n,"systemlink")?systemlink:!strcmp(n,"gpad_enabled")?gpad:online;}
int Dvar_GetInt(const char*){return port;}
bool CL_IsSplitGuest(int n){return n==1&&guest;}
int CL_GetLocalClientConnectionState(int n){return connection[n];}
bool IN_IsForegroundWindow(){return foreground;}
bool GPad_IsActive(int n){return pad&&n==port;}
double GPad_GetButton(int n,GamePadButton b){assert(n==port);return buttons[b];}
double GPad_GetStick(int n,Stick s){assert(n==port);return sticks[s];}
const char*Dvar_GetString(const char*){return map.c_str();}
bool viewportReady=false;bool CL_SplitGuestHasViewport(){return viewportReady;}
std::vector<std::vector<std::string>>cmdStack;
void Cmd_TokenizeStringNoEval(const char*s){std::istringstream in(s);std::string token;cmdStack.emplace_back();while(in>>std::quoted(token))cmdStack.back().push_back(token);}
int Cmd_Argc(){return cmdStack.back().size();}
const char*Cmd_Argv(int n){return cmdStack.back().at(n).c_str();}
void Cmd_EndTokenizedString(){cmdStack.pop_back();}
struct cg_s{char scriptMainMenu[256]="host_menu";}games[2];
cg_s*CG_GetLocalClientGlobals(int n){return &games[n];}
char*CG_ScriptMainMenu(int);

struct{struct{int integer=2;}current;}slots;auto com_maxclients=&slots;
struct{int serverId=42;}storage[2];auto clients=storage;
struct{int realtime=100,serverId=42;}cls;
std::string menus[32];char*CL_GetConfigString(int n){assert(n>=2548&&n<2580);return menus[n-2548].data();}
int I_stricmp(const char*a,const char*b){return strcmp(a,b);}
const char*I_stristr(const char*a,const char*b){return strstr(a,b);}
void I_strncpyz(char*a,const char*b,unsigned n){snprintf(a,n,"%s",b);}
std::vector<std::string> commands,events,hostCommands;std::vector<int> keys;
void CL_AddReliableCommand(int n,const char*c){assert(n==1);commands.emplace_back(c);}
void CL_SplitGuestLog(const char*s){events.emplace_back(s);}
struct ScreenPlacement {int y=0,height=360;}scrPlaceView[2]{{0,360},{360,361}},scrPlaceFull{0,721};
int menuDraws=0;bool CL_SplitGuestDrawingMenu();
void UI_UpdateTime(int n,int){assert(n==0);}
void UI_Refresh(int n){assert(n==0&&CL_SplitGuestDrawingMenu());assert(scrPlaceView[0].y==360&&scrPlaceView[0].height==361);assert(scrPlaceFull.y==360);++menuDraws;}
struct UiContext{};struct uiInfo_s{UiContext uiDC;};uiInfo_s info;
struct Window{const char*name=nullptr;};struct menuDef_t{Window window;};struct itemDef_s{menuDef_t*parent;};
menuDef_t focusedMenu;bool focus=false;int catcher=0;
auto UI_GetInfo(int n){assert(n==0);return &info;}
bool Key_IsCatcherActive(int n,int mask){assert(n==0);return (catcher&mask)!=0;}
menuDef_t*Menu_GetFocused(UiContext*){return focus?&focusedMenu:nullptr;}
menuDef_t*Menus_FindByName(UiContext*,const char*n){return n&&*n?&focusedMenu:nullptr;}
void UI_SetActiveMenu(int n,int m){assert(n==0);focus=m!=UIMENU_NONE;catcher=focus?16:0;if(focus)focusedMenu.window.name=CG_ScriptMainMenu(0);}
int UI_PopupScriptMenu(int n,const char*name,bool mouse){assert(n==0&&!mouse);focus=true;catcher=16;focusedMenu.window.name=name;return 1;}
void UI_ClosePopupScriptMenu(int n,bool allow){assert(n==0&&!allow);focus=false;catcher=0;}
void UI_CloseInGameMenu(int n){UI_SetActiveMenu(n,UIMENU_NONE);}
bool CL_SplitGuestMenuAcceptsUiInput();
void Script_ScriptMenuResponse(int,UiContext*,itemDef_s*,const char**);
const char*selectedResponse="smg_mp";
void UI_KeyEvent(int n,int key,int down){
 assert(n==0);if(!CL_SplitGuestMenuAcceptsUiInput())return;
 if(down){keys.push_back(key);if(key==K_BUTTON_A){itemDef_s item{&focusedMenu};const char*arg=selectedResponse;Script_ScriptMenuResponse(0,&info.uiDC,&item,&arg);UI_SetActiveMenu(0,UIMENU_NONE);}
 else if(key==K_BUTTON_B||key==K_ESCAPE)UI_SetActiveMenu(0,UIMENU_NONE);}
}
bool UI_AllowScriptMenuResponse(int){return true;}
bool String_Parse(const char**args,char*out,unsigned n){snprintf(out,n,"%s",*args);return true;}
const char*va(const char*fmt,...){static char b[512];va_list a;va_start(a,fmt);vsnprintf(b,sizeof(b),fmt,a);va_end(a);return b;}
void Cbuf_AddText(int n,const char*s){assert(n==0);hostCommands.emplace_back(s);}
struct gentity_s{int number=0;}guestEntity{1};
int sv_serverId_value=42;std::vector<std::string> args;
int SV_Cmd_Argc(){return args.size();}
void SV_Cmd_ArgvBuffer(int n,char*b,int size){snprintf(b,size,"%s",args.at(n).c_str());}
void SV_GetConfigstring(int n,char*b,int size){snprintf(b,size,"%s",CL_GetConfigString(n));}
constexpr int SCRIPTINSTANCE_SERVER=0;struct{int menuresponse=123;}scr_const;
std::vector<std::string> scriptArgs;int notifications=0;
void Scr_AddString(const char*s,int){scriptArgs.emplace_back(s);}
void Scr_Notify(gentity_s*ent,int event,unsigned n){assert(ent->number==1&&event==123&&n==2);++notifications;}
void deliver(const std::string&cmd){args.clear();std::istringstream in(cmd);std::string part;while(in>>part)args.push_back(part);scriptArgs.clear();}
"""
source=(root/'src/client/splitscreen_menu.cpp').read_text()
code+=re.sub(r'^\s*#include.*\n','',source,flags=re.M)+'\n'
code+=function('src/client/splitscreen.cpp','int CL_SplitGuestExpectedPlayers(')+'\n'
code+=function('src/ui/ui_shared.cpp','void __cdecl Script_ScriptMenuResponse(')+'\n'
code+=function('src/game_mp/g_cmds_mp.cpp','void __cdecl Cmd_MenuResponse_f(')+'\n'
code+=function('src/cgame_mp/cg_newDraw_mp.cpp','char *__cdecl CG_ScriptMainMenu(')+'\n'
code+='struct {struct{bool enabled=false;}current;}hudBehind;auto cl_hudDrawsBehindUI=&hudBehind;struct{int keyCatchers=16;}uiActive;auto CL_GetLocalClientUIGlobals(int n){assert(n==0);return &uiActive;}bool CL_LocalClient_IsCUIFlagSet(int,int){return false;}\n'
code+=function('src/client_mp/cl_main_mp.cpp','bool __cdecl CL_ShouldDisplayHud(')+'\n'


code+='bool Menus_AnyFullScreenVisible(UiContext*){return focus;}\\n'.replace('\\n','\n')
code+=function('src/ui/ui_main.cpp','int __cdecl UI_IsFullscreen(').replace('UI_IsFullscreen(', 'Native_UI_IsFullscreen(')+'\\n'.replace('\\n','\n')

code+=r"""
void tick(){cls.realtime+=16;CL_SplitGuestMenuFrame();}
void press(GamePadButton b){buttons[b]=1;tick();buttons[b]=0;tick();}
int main(){
 assert(CL_SplitGuestExpectedPlayers()==2);online=true;assert(CL_SplitGuestExpectedPlayers()==1);online=false;
 systemlink=false;assert(CL_SplitGuestExpectedPlayers()==1);systemlink=true;map="zombie_theater";assert(CL_SplitGuestExpectedPlayers()==1);
 map="mp_nuked";slots.current.integer=1;assert(CL_SplitGuestExpectedPlayers()==1);slots.current.integer=2;
 automatic=false;assert(CL_SplitGuestExpectedPlayers()==1);automatic=true;
 menus[2]="team_marinesopfor";menus[3]="ingame";menus[7]="changeclass";menus[8]="class_marines";menus[9]="class_opfor";menus[10]="scoreboard";
 tick();UI_SetActiveMenu(0,UIMENU_INGAME);assert(Native_UI_IsFullscreen(0)&&!CL_ShouldDisplayHud(0));
 viewportReady=true;assert(!Native_UI_IsFullscreen(0)); // initial P1 prompt also stays in its half
 CL_SplitGuestHandleMenuCommand("v \"g_scriptMainMenu\" \"class_opfor\"");
 assert(!strcmp(CL_SplitGuestScriptMainMenu(),"class_opfor")&&cmdStack.empty());

 CL_SplitGuestHandleMenuCommand("t 7");tick();assert(!CL_SplitGuestOwnsMenu()&&commands.empty()); // wait for P1
 UI_SetActiveMenu(0,UIMENU_NONE);tick();assert(CL_SplitGuestOwnsMenu()&&focus&&commands.empty());assert(!Native_UI_IsFullscreen(0));
 assert(!CL_SplitGuestMenuAcceptsUiInput()&&CL_SplitGuestMenuBlocksInput()&&CL_ShouldDisplayHud(0));
 CL_SplitGuestDrawMenu();assert(menuDraws==1&&scrPlaceView[0].y==0&&scrPlaceFull.y==0&&!CL_SplitGuestDrawingMenu());

 UI_KeyEvent(0,K_BUTTON_A,1);assert(commands.empty());
 // A delayed onClose from P1 must not cancel P2's pending initial class.
 itemDef_s oldItem{&focusedMenu};const char*oldResponse="back";
 Script_ScriptMenuResponse(0,&info.uiDC,&oldItem,&oldResponse);assert(commands.empty()&&hostCommands.empty());
 // P1 cannot select P2's class
 press(GPAD_DOWN);assert(keys.back()==K_DOWNARROW);
 press(GPAD_A);assert(commands.size()==1&&commands.back()=="mr 42 7 smg_mp");
 deliver(commands.back());Cmd_MenuResponse_f(&guestEntity);
 assert(notifications==1&&scriptArgs[0]=="smg_mp"&&scriptArgs[1]=="changeclass"&&hostCommands.empty());
 assert(!CL_SplitGuestOwnsMenu());tick();assert(!CL_SplitGuestMenuBlocksInput());
 // P1's normal UI script keeps its existing command buffer and class.
 focusedMenu.window.name="changeclass";itemDef_s hostItem{&focusedMenu};const char*arg="assault_mp";
 Script_ScriptMenuResponse(0,&info.uiDC,&hostItem,&arg);assert(hostCommands.size()==1&&hostCommands[0]=="cmd mr 42 7 assault_mp\n");
 press(GPAD_START);assert(CL_SplitGuestOwnsMenu()&&focus&&!strcmp(focusedMenu.window.name,"class_opfor"));
 // Start backs out without selecting a class, including Nintendo's A/B layout.
 press(GPAD_START);assert(!CL_SplitGuestOwnsMenu()&&!focus&&commands.size()==1);
 assert(keys.back()==K_ESCAPE);press(GPAD_START);assert(CL_SplitGuestOwnsMenu());
 // A later class request uses the real script-menu index, not the host's index.
 CL_SplitGuestHandleMenuCommand("t\t7 0");tick();assert(!strcmp(focusedMenu.window.name,"changeclass"));
 selectedResponse="sniper_mp";press(GPAD_A);assert(commands.back()=="mr 42 7 sniper_mp");
 deliver(commands.back());Cmd_MenuResponse_f(&guestEntity);assert(notifications==2&&scriptArgs[0]=="sniper_mp");
 // Malformed/unknown prompts do not steal UI, nor generate automatic selections.
 tick();for(auto bad:{"t -1","t 32","t 7x","t nope","t 10","v 7","t"})CL_SplitGuestHandleMenuCommand(bad);
 tick();assert(!CL_SplitGuestOwnsMenu()&&commands.size()==2);
 CL_SplitGuestHandleMenuCommand("t 2");tick();selectedResponse="autoassign";press(GPAD_A);
 assert(commands.back()=="mr 42 2 autoassign");deliver(commands.back());Cmd_MenuResponse_f(&guestEntity);assert(notifications==3);
 // Server close, focus loss, stale map and removal cannot send replies to P1.
 press(GPAD_START);CL_SplitGuestHandleMenuCommand("u");tick();assert(!CL_SplitGuestOwnsMenu()&&!focus);
 press(GPAD_START);clients[1].serverId=43;tick();assert(!CL_SplitGuestOwnsMenu()&&!focus&&commands.size()==3);
 clients[1].serverId=42;tick();press(GPAD_START);assert(CL_SplitGuestOwnsMenu());
 auto count=commands.size();assert(CL_SplitGuestMenuResponse("missing","smg_mp"));assert(CL_SplitGuestMenuResponse("changeclass","bad\ncmd"));assert(commands.size()==count);
 foreground=false;tick();assert(!CL_SplitGuestOwnsMenu()&&!focus);foreground=true;tick();
 press(GPAD_START);guest=false;tick();assert(!CL_SplitGuestOwnsMenu()&&!focus&&hostCommands.size()==1);
 deliver("mr 41 7 assault_mp");Cmd_MenuResponse_f(&guestEntity);assert(notifications==3&&scriptArgs.empty());
 puts("PASS: private MP activation; queued P2 native menus; independent class/team responses through native UI/server handlers; controller navigation; P1 isolation; teardown/stale-input guards");
}
"""
cpp=out/'mp.cpp';cpp.write_text(code);exe=cpp.with_suffix('.exe');subprocess.run([args.compiler,'-std=c++17','-w',str(cpp),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
zone=Path('F:/pluto_t5_full_game/kbz/common_mp.kbz')
if zone.exists():
 b=zone.read_bytes()
 def raw(name):
  a=b.index(name.encode()+b'\0');return zlib.decompress(b[a+len(name)+9:]).decode(errors='replace')
 menu=raw('maps/mp/gametypes/_menus.gsc');classes=raw('maps/mp/gametypes/_class.gsc')
 for name in ['team_marinesopfor','changeclass']:assert '"'+name+'"' in menu
 for name in ['assault_mp','smg_mp','sniper_mp']:assert 'level.classMap["'+name+'"]' in classes
 print('PASS: native menu/class identifiers match the locally supplied MP scripts')
