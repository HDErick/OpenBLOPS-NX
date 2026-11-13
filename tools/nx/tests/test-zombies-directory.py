"""Exercise the actual Zombies directory menus and co-op launch without starting the game."""
from pathlib import Path
import argparse
import os
import re
import struct
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--compiler', default='C:/msys64/ucrt64/bin/g++.exe')
a = p.parse_args()
root = Path(__file__).resolve().parents[3]
out = root / 'build-nx/zombies-directory-tests'
out.mkdir(parents=True, exist_ok=True)
env = dict(os.environ)
env['PATH'] = str(Path(a.compiler).parent) + os.pathsep + env.get('PATH', '')

# Check the real shipped entry points and templates, rather than assuming MP
# menu names/actions also exist in the SP frontend zone.
parser = (root / 'tools/nx/tests/test-splitscreen-frontend.py').read_text()
parser = parser[parser.index('def menu_items('):parser.index('\nsp=menu_items')]
parser = parser.replace('result.append(dict(', '''td=ptr(item,224);exp=ptr(td,64);rp=ptr(exp,16);keys=[]
   if exp and rp and num(item,176) in (1,3):
    keys=[string(ptr(rp,k*24+16)) for k in range(num(exp,12)) if num(rp,k*24)==0 and num(rp,k*24+8)==2]
   result.append(dict(expression_keys=keys,''')
scope = dict(Path=Path, struct=struct)
exec(parser, scope)
rows = scope['menu_items']('frontend', 'menu_xboxlive')
assert sum('@MPUI_FIND_MATCH_CAPS' in x['expression_keys'] for x in rows) == 4
assert sum('@MPUI_CUSTOM_MATCH_CAPS' in x['expression_keys'] for x in rows) == 4
templates = scope['menu_items']('frontend', 'mods_menu')
assert any(x['type'] == 4 and x['name'] == 'modlist' for x in templates)
assert any(x['type'] == 3 for x in templates)
picker = scope['menu_items']('frontend', 'levels_zombie')
assert any('startlevel 0' in action for x in picker for action in x['actions'])
print('PASS: shipped Zombies Find/Private layers, SP browser templates and native map-picker callback', flush=True)

code = r'''
#include <openblops_directory/directory_engine.h>
#include <openblops_directory/directory_protocol.h>
#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <list>
#include <map>
#include <string>
#include <vector>
struct rectDef_s {float x=0,y=0,w=0,h=0;int horzAlign=0,vertAlign=0;};
struct Material {const char *name;};
const char *Material_GetName(Material *material){return material->name;}
struct windowDef_t {const char *name=nullptr,*group=nullptr;rectDef_s rect,rectClient;int staticFlags=0,dynamicFlags[1]{},ownerDraw=0,style=0,border=0;Material *background=nullptr;float foreColor[4]{},backColor[4]{},borderColor[4]{},borderSize=0;};
struct Rpn {int type=0;struct {struct {int dataType=2;struct {const char *string=nullptr;} internals;} constant;} data;};
struct ExpressionStatement {Rpn *rpn=nullptr;int numRpn=0;};
constexpr int VAL_STRING=2;
struct textExp_s {ExpressionStatement textExp;};
struct GenericEventScript {const char *action=nullptr;GenericEventScript *next=nullptr;};
struct GenericEventHandler {const char *name=nullptr;GenericEventScript *eventScript=nullptr;GenericEventHandler *next=nullptr;};
struct columnInfo_s {int elementStyle=0,maxChars=0;rectDef_s rect;};
struct listBoxDef_s {float special=0,elementWidth=0,elementHeight=0;int numColumns=0,cursorPos[1]{},startPos[1]{},endPos[1]{},rowCount=0,maxRows=0,notselectable=0,noScrollBars=0;void *rows=nullptr;columnInfo_s columnInfo[16];};
struct editFieldDef_s {int maxChars=0,maxPaintChars=0;};
struct multiDef_s {int count=0;const char *dvarList[32]{};float dvarValue[32]{};};
struct focusItemDef_s {union {listBoxDef_s *listBox;editFieldDef_s *editField;multiDef_s *multi;}focusTypeData{};};
struct textDef_s {const char *text=nullptr;textExp_s *textExpData=nullptr;rectDef_s textRect[1];struct {focusItemDef_s *focusItemDef=nullptr;} textTypeData;int alignment=0,textAlignMode=0;float textalignx=0,textaligny=0,textscale=0;};
struct itemDef_s {windowDef_t window;int type=0,dataType=0,ui3dWindowId=0;const char *dvar=nullptr,*dvarTest=nullptr,*enableDvar=nullptr;int dvarFlags=0;ExpressionStatement visibleExp,forecolorAExp;unsigned long long showBits=0,hideBits=0;void *rectExpData=nullptr,*animInfo=nullptr;menuDef_t *parent=nullptr;GenericEventHandler *onEvent=nullptr;struct {textDef_s *textDef=nullptr;}typeData;};
struct menuDef_t {windowDef_t window;rectDef_s initialRectInfo;int ui3dWindowId=0,fullScreen=0,cursorItem[1]{},openFadingTime=0,closeFadingTime=0,openSlideDirection=0,closeSlideDirection=0,fadeTimeCounter=0,slideTimeCounter=0,itemCount=0;void *onKey=nullptr;GenericEventHandler *onEvent=nullptr;ExpressionStatement visibleExp,rectXExp,rectYExp;unsigned long long showBits=0,hideBits=0;itemDef_s **items=nullptr;};
struct UiContext {menuDef_t *Menus[600]{};int menuCount=0,contextIndex=0;};
struct UI {UiContext uiDC;}ui;
struct {int numunrankedservers=0;struct {int clients=0;}unrankedServers[256];}cls;
struct {struct {bool enabled=true;}current;}running;
auto *com_sv_running=&running;
std::map<std::string,std::string> vars{{"mapname","frontend"},{"bo1_directory_name","Co-op host"}};
std::vector<std::string> commands,scripts,opened;
menuDef_t *focused=nullptr;
int splitResets=0,refreshes=0,joins=0,cancelJoins=0,cancelRefreshes=0,accepts=0,cancels=0;
bool pendingPassword=false;
int I_stricmp(const char *a,const char *b){return strcasecmp(a,b);}
int I_strnicmp(const char *a,const char *b,int n){return strncasecmp(a,b,n);}
struct StringTable {int columnCount=13;} maps;
union XAssetHeader {StringTable *stringTable;};
struct {int mapCount=1;struct {const char *mapLoadName,*mapName;}mapList[1]{{"vorkuta","Vorkuta"}};}sharedUiInfo;
void StringTable_GetAsset(const char *name,XAssetHeader *asset){assert(!strcmp(name,"maps/mapsTable.csv"));asset->stringTable=&maps;}
const char *StringTable_Lookup(const StringTable *table,int keyColumn,const char *map,int nameColumn){
 assert(table==&maps&&keyColumn==0&&nameColumn==3);
 if (!strcmp(map,"zombie_theater")) return "MENU_ZOMBIE_THEATER_CAPS";
 if (!strcmp(map,"zombie_pentagon")) return "MENU_ZOMBIE_PENTAGON_CAPS";
 return "";
}
char *UI_SafeTranslateString(const char *key){
 if (!strcmp(key,"MENU_ZOMBIE_THEATER_CAPS")) return const_cast<char *>("KINO DER TOTEN");
 if (!strcmp(key,"MENU_ZOMBIE_PENTAGON_CAPS")) return const_cast<char *>("\"FIVE\"");
 return const_cast<char *>(key);
}
#ifndef __cdecl
#define __cdecl
#endif
bool Item_IsTextDefType(const itemDef_s *i){return i->type!=0;}
void I_strncpyz(char *d,const char *s,size_t n){snprintf(d,n,"%s",s);}
void Dvar_SetStringByName(const char *n,const char *v){vars[n]=v;}
const char *Dvar_GetString(const char *n){return vars[n].c_str();}
void Dvar_SetBoolByName(const char *n,bool v){vars[n]=v?"1":"0";}
void Dvar_SetIntByName(const char *n,int v){vars[n]=std::to_string(v);}
int Dvar_GetInt(const char *n){return atoi(vars[n].c_str());}
void _Dvar_RegisterString(const char *n,const char *v,int,const char*){vars.emplace(n,v);}
void _Dvar_RegisterInt(const char *n,int v,int,int,int,const char*){vars.emplace(n,std::to_string(v));}
UI *UI_GetInfo(int){return &ui;}
bool Com_IsMenuLevel(const char*){return vars["mapname"]=="frontend";}
bool Com_IsZombieMap(const char *map){return !strncmp(map,"zombie_",7);}
bool Dvar_GetBool(const char *name){return vars[name]=="1";}
bool guestConnected=false,splitSelected=false;
bool CL_IsSplitGuest(int){return guestConnected;}
bool CL_SplitScreenSessionRequested(){return splitSelected;}
void CL_SplitScreenResetSession(){++splitResets;}
void Com_Printf(int,const char*,...){}
char *va(const char *f,...){static char b[1024];va_list a;va_start(a,f);vsnprintf(b,sizeof(b),f,a);va_end(a);return b;}
void Cbuf_AddText(int,const char *s){commands.emplace_back(s);}
const char *Directory_Status(){return "Ready to join";}
void Directory_CancelJoin(){++cancelJoins;}
void Directory_CancelRefresh(){++cancelRefreshes;}
bool Directory_AcceptJoinPassword(){if(!pendingPassword)return false;pendingPassword=false;++accepts;return true;}
void Directory_CancelJoinPassword(){if(pendingPassword)++cancels;pendingPassword=false;vars["password"]="";vars["ui_directory_joinPassword"]="";}
void UI_StartServerRefresh(int,int,int){++refreshes;}
void UI_RunMenuScript_JoinServer(int,int){++joins;}
bool Directory_Source(int source){return source==0;}
bool Directory_HasPasswordPrompt(){return pendingPassword;}
bool Window_HasFocus(int,const windowDef_t *window){return (window->dynamicFlags[0]&2)!=0;}
itemDef_s *Menu_FindItemByName(menuDef_t *menu,const char *name){
 for(int i=0;i<menu->itemCount;++i)
  if(menu->items[i]->window.name&&!strcmp(menu->items[i]->window.name,name))return menu->items[i];
 return nullptr;
}
listBoxDef_s *Item_GetListBoxDef(itemDef_s *item){return item->typeData.textDef->textTypeData.focusItemDef->focusTypeData.listBox;}
int rows=2,selectedRow=-1,focusMoves=0;
int UI_FeederCount(int,int,float,listBoxDef_s*){return rows;}
void UI_FeederSelection(int,int,float,int row){selectedRow=row;}
itemDef_s *Menu_SetPrevCursorItem(int,UiContext*,menuDef_t*,int){--focusMoves;return nullptr;}
itemDef_s *Menu_SetNextCursorItem(int,UiContext*,menuDef_t*,int,int){++focusMoves;return nullptr;}
'''
code += (root / 'src/ui/keycodes.h').read_text().replace('#pragma once', '').replace(': __int32', ': int')
code += r'''
int Item_ListBox_HandleKey(int,UiContext*,itemDef_s *item,int key,int down,int force){
 assert(force&&down);
 auto &row=Item_GetListBoxDef(item)->cursorPos[0];
 if(rows)row=std::clamp(row+(key==K_UPARROW?-1:1),0,rows-1);
 return 1;
}
#ifdef KISAK_NX
struct SwkbdConfig {bool password=false;int limit=0;};
using Result=int;
#define R_FAILED(result) ((result)!=0)
#define R_SUCCEEDED(result) ((result)==0)
int keyboardResult=0,keyboardsClosed=0;
const char *keyboardText="pad password";
Result swkbdCreate(SwkbdConfig*,int){return 0;}
void swkbdConfigMakePresetPassword(SwkbdConfig *keyboard){keyboard->password=true;}
void swkbdConfigMakePresetDefault(SwkbdConfig*){}
void swkbdConfigSetHeaderText(SwkbdConfig*,const char*){}
void swkbdConfigSetInitialText(SwkbdConfig*,const char*){}
void swkbdConfigSetStringLenMax(SwkbdConfig *keyboard,int limit){keyboard->limit=limit;}
Result swkbdShow(SwkbdConfig *keyboard,char *text,size_t size){
 assert(keyboard->password&&keyboard->limit==32);
 snprintf(text,size,"%s",keyboardText);
 return keyboardResult;
}
void swkbdClose(SwkbdConfig*){++keyboardsClosed;}
void Com_PrintWarning(int,const char*,...){}
int Menu_GetCursorItem(int,menuDef_t *menu){return menu->cursorItem[0];}
bool Item_IsTextField(const itemDef_s *item){return item->type==5;}
#endif
menuDef_t *Menus_FindByName(UiContext *dc,const char *n){for(int i=0;i<dc->menuCount;++i)if(!strcmp(dc->Menus[i]->window.name,n))return dc->Menus[i];return nullptr;}
int Menus_OpenByName(int,UiContext *dc,const char *n){auto *m=Menus_FindByName(dc,n);assert(m);Directory_MenuOpening(0,dc,m);focused=m;opened.emplace_back(n);return 1;}
void Menus_CloseByName(int,UiContext*,const char *n){if(focused&&!strcmp(focused->window.name,n))focused=nullptr;}
void Menus_CloseAll(int,UiContext*){
 focused=nullptr;
 // The shipped frontend can regain focus during close callbacks.
 vars["onlinegame"]="1";
 vars["g_password"]="callback-reset";
 vars["bo1_directory_name"]="callback-reset";
}
void Item_RunScript(int,UiContext*,itemDef_s*,const char *s){scripts.emplace_back(s);}
'''
ui_source = (root / 'src/ui/ui_main.cpp').read_text()
code += ui_source[ui_source.index('char *__cdecl UI_GetMapDisplayName('):
                  ui_source.index('char *__cdecl UI_GetMapDisplayNameFromPartialLoadNameMatch(')]
source = (root / 'src/openblops_directory/directory_zombies.cpp').read_text()
code += re.sub(r'^#include[^\n]*\n', '', source, flags=re.M)
engine = (root / 'src/openblops_directory/directory_engine.cpp').read_text()
controller = engine.index('#ifdef KISAK_NX\nstatic void EditDirectoryText(')
code += engine[controller:engine.index('bool Directory_UpdateBrowserCounts()', controller)]
for signature in ['static bool Directory_IsSplitScreenHost(', 'static bool Directory_IsCompatibleHostMap(']:
    start = engine.index(signature)
    end = engine.index('{', start) + 1
    depth = 1
    while depth:
        depth += (engine[end] == '{') - (engine[end] == '}')
        end += 1
    code += engine[start:end] + '\n'
condition = re.search(r'bool hosting = (.*?);', engine, re.S).group(1)
code += '''
enum {SS_GAME=2};struct {int state=SS_GAME;}sv;struct {bool clients=true;}svs;
struct {struct {bool enabled=true;}current;}publication;
auto *publish=&publication;
bool Hosting(){bool localSplitScreen=Directory_IsSplitScreenHost();return ''' + condition + ';}\n'
code += r'''
itemDef_s *find(menuDef_t *m,const char *n){for(int i=0;i<m->itemCount;++i)if(m->items[i]->window.name&&!strcmp(m->items[i]->window.name,n))return m->items[i];assert(false);return nullptr;}
void press(itemDef_s *i,const char *e="action"){assert(Directory_HandleMenuEvent(0,&ui.uiDC,i,e));}
void lifecycle(menuDef_t *m,const char *e){itemDef_s i;i.parent=m;Directory_HandleMenuEvent(0,&ui.uiDC,&i,e);}
int main(){
 auto *dc=&ui.uiDC;
 menuDef_t base;base.window.name="mods_menu";
 Material coop{"menu_background_coop_blurred"};
 itemDef_s frame,text,list;
 frame.window.rect=frame.window.rectClient={-100,65,840,355,4,4};
 frame.window.background=&coop;
 textDef_s tx,lt;focusItemDef_s tf,lf;listBoxDef_s lb;
 text.window.name="template_button";text.type=text.dataType=3;text.typeData.textDef=&tx;tx.text="ORIGINAL";tx.textTypeData.focusItemDef=&tf;
 list.window.name="modlist";list.type=list.dataType=4;list.typeData.textDef=&lt;lt.textTypeData.focusItemDef=&lf;lf.focusTypeData.listBox=&lb;
 lb.special=9;lb.cursorPos[0]=7;lb.elementHeight=19;
 itemDef_s *items[]={&frame,&text,&list};base.items=items;base.itemCount=3;dc->Menus[dc->menuCount++]=&base;
 assert(Directory_ZombiesPrepareMenus(dc));assert(dc->menuCount==4);
 assert(Directory_ZombiesPrepareMenus(dc));assert(dc->menuCount==4); // no duplicated definitions
 auto *browser=Menus_FindByName(dc,browserName),*host=Menus_FindByName(dc,hostName),*popup=Menus_FindByName(dc,passwordName);
 assert(browser&&host&&popup&&browser->ui3dWindowId==-1&&host->fullScreen);
 for (auto *panel : {browser,host}) {
  auto *background=panel->items[0];
  const auto &rect=background->window.rectClient;
  assert(background->window.background==&coop&&background->window.backColor[3]==1);
  assert(rect.x==0&&rect.y==0&&rect.w==640&&rect.h==480&&rect.horzAlign==4&&rect.vertAlign==4);
 }
 assert(frame.window.rect.y==65&&frame.window.rect.h==355);
 assert(!strcmp(UI_GetMapDisplayName("vorkuta"),"Vorkuta"));
 assert(!strcmp(UI_GetMapDisplayName("zombie_custom"),"zombie_custom"));
 auto *browserItem=find(browser,"browser");auto *liveList=browserItem->typeData.textDef->textTypeData.focusItemDef->focusTypeData.listBox;
 assert(liveList!=&lb&&liveList->special==2&&liveList->cursorPos[0]==0&&lb.cursorPos[0]==7&&lb.special==9);
 assert(tx.text==std::string("ORIGINAL")&&text.window.name==std::string("template_button")&&base.itemCount==3&&base.items==items);
 assert(strstr(browser->onEvent->eventScript->action,"PopulateServers"));
 assert(strstr(find(browser,"header_server")->onEvent->eventScript->action,"ServerSort 3"));
 assert(strstr(browserItem->onEvent->next->next->next->eventScript->action,"JoinServer"));
 // A pad can move and join even when the mouse is outside the list.
 browserItem->window.dynamicFlags[0]|=2;
 assert(Directory_HandleControllerKey(0,dc,browser,K_DPAD_DOWN)&&liveList->cursorPos[0]==1);
 assert(Directory_HandleControllerKey(0,dc,browser,K_BUTTON_A)&&selectedRow==1&&joins==1);
 assert(Directory_HandleControllerKey(0,dc,browser,K_UPARROW)&&liveList->cursorPos[0]==0);
 assert(Directory_HandleControllerKey(0,dc,browser,K_APAD_RIGHT)&&focusMoves==1);
 assert(Directory_HandleControllerKey(0,dc,browser,K_BUTTON_X)&&refreshes==1);
 assert(!Directory_HandleControllerKey(0,dc,browser,K_BUTTON_B));
 rows=0;
 assert(Directory_HandleControllerKey(0,dc,browser,K_BUTTON_A)&&joins==1);
 rows=2;
#ifdef KISAK_NX
 pendingPassword=true;
 auto *passwordField=find(popup,"directory_join_password");
 for(int i=0;i<popup->itemCount;++i)if(popup->items[i]==passwordField)popup->cursorItem[0]=i;
 passwordField->window.dynamicFlags[0]|=2;
 assert(Directory_HandleControllerKey(0,dc,popup,K_BUTTON_A)&&keyboardsClosed==1);
 assert(vars["ui_directory_joinPassword"]=="pad password"&&vars["password"].empty());
 keyboardResult=1;keyboardText="cancelled input";
 assert(Directory_HandleControllerKey(0,dc,popup,K_BUTTON_A)&&keyboardsClosed==2);
 assert(vars["ui_directory_joinPassword"]=="pad password"&&pendingPassword);
 pendingPassword=false;
#endif
 joins=refreshes=0;
 press(find(browser,"directory_refresh"));press(find(browser,"directory_join"));assert(refreshes==1&&joins==1);
 Menus_OpenByName(0,dc,browserName);press(find(browser,"directory_back"));assert(cancelJoins==1&&cancelRefreshes==1&&!focused);
 // Real SP online menu has normal/disabled buttons and separate text highlights.
 menuDef_t online;online.window.name="menu_xboxlive";
 itemDef_s entries[8];textDef_s texts[8];itemDef_s *ep[8];
 for(int i=0;i<8;++i){entries[i]=text;entries[i].parent=&online;entries[i].typeData.textDef=&texts[i];entries[i].window.name="stock";entries[i].type=entries[i].dataType=i%2?1:3;texts[i]=tx;texts[i].text=i<4?"@MPUI_FIND_MATCH_CAPS":"@MPUI_CUSTOM_MATCH_CAPS";ep[i]=&entries[i];}
 GenericEventScript stockFocusScript;stockFocusScript.action="setLocalVarInt ui_highlight 1 ; setLocalVarString ui_choicegroup xboxlive ;";
 GenericEventHandler stockFocusEvent;stockFocusEvent.name="onFocus";stockFocusEvent.eventScript=&stockFocusScript;
 for(int i=0;i<8;++i){entries[i].onEvent=&stockFocusEvent;entries[i].visibleExp.numRpn=i+1;entries[i].window.foreColor[0]=i%2?0:1;entries[i].window.foreColor[3]=(i%4<2)?1:.45f;}
 online.items=ep;online.itemCount=8;Directory_MenuOpening(0,dc,&online);
 assert(online.itemCount==8&&online.items!=ep);assert(entries[0].window.name==std::string("stock"));
 for(int i=0;i<8;++i){assert(online.items[i]->onEvent==&stockFocusEvent&&online.items[i]->visibleExp.numRpn==i+1);assert(online.items[i]->window.foreColor[0]==(i%2?0:1)&&online.items[i]->window.foreColor[3]==1);}
 assert(find(&online,"directory_zombie_browser")->typeData.textDef->text==std::string("SERVER BROWSER"));
 assert(find(&online,"directory_zombie_host")->typeData.textDef->text==std::string("HOST MATCH"));
 Directory_MenuOpening(0,dc,&online);assert(online.itemCount==8);
 vars["bo1_directory_publish"]="1";press(find(&online,"directory_zombie_browser"));assert(focused==browser&&vars["bo1_directory_publish"]=="0");
 vars["ui_mapname"]="zombie_theater";press(find(&online,"directory_zombie_host"));assert(focused==host&&configuringHost);
 assert(!strcmp(find(host,"directory_map")->typeData.textDef->text,"MAP: KINO DER TOTEN"));
 press(find(host,"directory_change_map"));assert(choosingMap&&scripts.back().find("levels_zombie")!=std::string::npos);
 assert(!Directory_StartZombieMapNotify(1,"startlevel 0")&&!Directory_StartZombieMapNotify(0,"other"));
 vars["ui_load_level"]="zombie_pentagon";assert(Directory_StartZombieMapNotify(0,"startlevel 0"));
 assert(focused==host&&selectedMap=="zombie_pentagon"&&commands.empty()&&vars["bo1_directory_publish"]=="0");
 assert(!strcmp(find(host,"directory_map")->typeData.textDef->text,"MAP: \"FIVE\""));
 press(find(host,"directory_change_map"));vars["ui_load_level"]="zombietron";
 assert(Directory_StartZombieMapNotify(0,"startlevel 0")&&selectedMap=="zombie_pentagon"&&hostNotice[0]&&commands.empty());
 press(find(host,"directory_change_map"));vars["ui_load_level"]="zombie_theater;quit";
 assert(Directory_StartZombieMapNotify(0,"startlevel 0")&&selectedMap=="zombie_pentagon"&&commands.empty());
 menuDef_t picker;picker.window.name="levels_zombie";press(find(host,"directory_change_map"));lifecycle(&picker,"onESC");assert(!choosingMap&&focused==host);
 vars["bo1_directory_name"]="My Zombies";vars["g_password"]="secret";vars["bo1_zombie_maxplayers"]="3";vars["playlist_enabled"]="1";
 press(find(host,"directory_start"));
 assert(commands.size()==1&&commands[0].find("map zombie_pentagon\n")!=std::string::npos);
 assert(vars["onlinegame"]=="0"&&vars["systemlink"]=="1"&&vars["sv_maxclients"]=="3"&&vars["party_maxplayers"]=="3");
 assert(vars["playlist_enabled"]=="0"&&vars["bo1_directory_publish"]=="1"&&vars["splitscreen"]=="0");
 assert(vars["g_password"]=="secret"&&vars["bo1_directory_name"]=="My Zombies"&&vars["sv_hostname"]=="My Zombies"&&vars["g_gametype"]=="zom");
 assert(!Directory_StartHostMatch());
 vars["mapname"]="zombie_pentagon";Directory_FrontendFrame();assert(!startingHost&&vars["bo1_directory_publish"]=="1");
 vars["mapname"]="frontend";Directory_FrontendFrame();assert(vars["bo1_directory_publish"]=="0"&&runningHostMap.empty());
 for(int n:{1,2,4,18}){vars["bo1_zombie_maxplayers"]=std::to_string(n);Directory_ApplyHostPlayerLimit("zom",true);assert(vars["sv_maxclients"]==std::to_string(std::min(n,4)));}
 pendingPassword=true;vars["ui_directory_joinPassword"]="typed";vars["password"]="";
 auto *field=find(popup,"directory_join_password");assert(!strcmp(field->dvar,"ui_directory_joinPassword"));
 press(field,"onAccept");assert(accepts==1&&joins==1);
 pendingPassword=true;press(find(popup,"directory_password_cancel"));assert(!pendingPassword&&cancels==1&&vars["ui_directory_joinPassword"].empty());
 pendingPassword=true;lifecycle(popup,"onClose");assert(!pendingPassword&&cancels==2);
 pendingPassword=true;lifecycle(popup,"onESC");assert(!pendingPassword&&cancels==3);
 cls.numunrankedservers=2;cls.unrankedServers[0].clients=2;cls.unrankedServers[1].clients=3;
 Directory_FrontendFrame();assert(strstr(browserCounts,"5 players, 2 servers")&&strstr(browserStatus,"Ready to join"));
 vars["mapname"]="zombie_theater";assert(Hosting());
 for(const char *map:{"frontend","vorkuta","zombietron","mp_nuked"}){vars["mapname"]=map;assert(!Hosting());}
 vars["mapname"]="zombie_theater";vars["zombietron"]="1";assert(!Hosting());vars["zombietron"]="0";
 vars["splitscreen"]="1";assert(!Hosting());vars["splitscreen"]="0";
#ifdef KISAK_EXPERIMENTAL_SPLITSCREEN
 splitSelected=true;assert(!Hosting());splitSelected=false;guestConnected=true;assert(!Hosting());guestConnected=false;
#endif
 assert(Hosting());publication.current.enabled=false;assert(!Hosting());publication.current.enabled=true;
 menuDef_t rootMenu;rootMenu.window.name="main_online";
 itemDef_s onlineEntry=text,soloEntry=text;onlineEntry.window.name="online_button1";soloEntry.window.name="online_button2";
 onlineEntry.parent=soloEntry.parent=&rootMenu;itemDef_s *rootItems[]={&onlineEntry,&soloEntry};rootMenu.items=rootItems;rootMenu.itemCount=2;
 Directory_MenuOpening(0,dc,&rootMenu);assert(rootMenu.itemCount==2);
 vars["bo1_directory_publish"]="1";startingHost=true;runningHostMap="zombie_theater";
 press(find(&rootMenu,"online_button1"));assert(vars["bo1_directory_publish"]=="0"&&!startingHost&&runningHostMap.empty());
 assert(scripts.back().find("next_menu_name menu_xboxlive")!=std::string::npos&&scripts.back().find("xsigninlive")==std::string::npos);
 vars["bo1_directory_publish"]="1";assert(!Directory_HandleMenuEvent(0,dc,&soloEntry,"action"));assert(vars["bo1_directory_publish"]=="0");
 // Frontend zone reload reinstalls independent controls and list state.
 dc->menuCount=1;assert(Directory_ZombiesPrepareMenus(dc));assert(dc->menuCount==4&&Menus_FindByName(dc,browserName)!=browser&&lb.special==9);
 puts("PASS: native Zombies browser/host captions, rows, sorting/join/refresh, password events, private UI state, map selection/cancel, validated co-op launch, capacity and publication lifecycle");
}
'''
for name, extra in [('pc-zombies', ['KISAK_EXPERIMENTAL_SPLITSCREEN']),
                    ('nx-zombies', ['KISAK_NX', 'KISAK_EXPERIMENTAL_SPLITSCREEN']),
                    ('normal-zombies', [])]:
    cpp = out / (name + '.cpp')
    exe = cpp.with_suffix('.exe')
    cpp.write_text(code)
    subprocess.run([a.compiler, '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    '-DOPENBLOPS_NO_STEAM_AUTH', '-DKISAK_SP', *['-D' + x for x in extra],
                    '-I' + str(root / 'src'), str(cpp),
                    str(root / 'src/openblops_directory/directory_protocol.cpp'), '-o', str(exe)], check=True, env=env)
    subprocess.run([str(exe)], check=True, env=env)
