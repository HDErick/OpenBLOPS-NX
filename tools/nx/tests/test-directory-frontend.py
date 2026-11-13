"""Test the actual Host Match/menu patch and publication lifecycle, without running the game."""
from pathlib import Path
import argparse,os,re,subprocess,struct
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--compiler',default='C:/msys64/ucrt64/bin/g++.exe');a=p.parse_args()
r=Path(__file__).resolve().parents[3];out=r/'build-nx/directory-frontend-tests';out.mkdir(parents=True,exist_ok=True)
# Verify the private/ranked actions against the supplied prelinked assets.
src=(r/'tools/nx/tests/test-splitscreen-frontend.py').read_text();start=src.index('def menu_items(');end=src.index('\nsp=menu_items',start);namespace={'Path':Path,'struct':struct}
asset_parser=src[start:end].replace('result.append(dict(', """td=ptr(item,224);exp=ptr(td,64);rp=ptr(exp,16);keys=[]
   if exp and rp and num(item,176) in (1,3):
    keys=[string(ptr(rp,k*24+16)) for k in range(num(exp,12)) if num(rp,k*24)==0 and num(rp,k*24+8)==2]
   result.append(dict(dvar=string(ptr(item,192)),expression_keys=keys,""")
exec(asset_parser,namespace)
rows=namespace['menu_items']('ui_mp','live_flyout')
if rows:
 ranked=next(x for x in rows if x['name']=='SecondFlyout');private=next(x for x in rows if x['name']=='FifthFlyout')
 assert any('quickmatch' in x for x in ranked['actions'])
 assert any('open menu_xboxlive_privatelobby' in x for x in private['actions'])
 assert ranked['rect'][1] < private['rect'][1]
 assert '@MENU_RANKED_MATCH_CAPS' in rows[8]['expression_keys']
 assert '@MENU_RANKED_MATCH_CAPS' in rows[9]['expression_keys']
 main_rows=namespace['menu_items']('ui_mp','main_text')
 assert all('@MENU_OPERATIONS_CAPS' in main_rows[i]['expression_keys'] for i in (22,34,35,36))
 settings_rows=namespace['menu_items']('ui_mp','server_settings')
 assert next(x for x in settings_rows if x['dvar']=='sv_hostname')['type']==5
 assert next(x for x in settings_rows if x['dvar']=='g_password')['type']==5
 name_rect=next(x for x in settings_rows if x['dvar']=='sv_hostname')['rect']
 assert any(x['dvar']=='sv_vac' for x in settings_rows)
 for item in settings_rows:
  if item['dvar'] and item['dvar'] not in ('sv_hostname','g_password'):
   assert item['rect'][0]==name_rect[0] and item['rect'][3]==name_rect[3] and item['rect'][1] not in (-131,-55)

 assert any('open server_settings' in action for item in namespace['menu_items']('ui_mp','menu_xboxlive_privatelobby') for action in item['actions'])
 print('PASS: shipped expression labels in Ranked normal/highlight and all four Operations text layers verified',flush=True)
code=r"""
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <map>
#include <string>
#include <vector>
#include <list>
#include <openblops_directory/directory_protocol.h>
struct Rect {float x=0,y=0,w=180,h=17;};
struct Window {const char *name=nullptr;Rect rect,rectClient;int style=3,background=123;float foreColor[4]{.2f,.3f,.4f,1};};
struct GenericEventScript {const char *action=nullptr;GenericEventScript *next=nullptr;};
struct GenericEventHandler {const char *name=nullptr;GenericEventScript *eventScript=nullptr;GenericEventHandler *next=nullptr;};
struct editFieldDef_s {int cursorPos[1]{},paintOffset=0,maxChars=20;};
struct multiDef_s {const char*dvarList[32]{};float dvarValue[32]{};int count=0;};
struct focusItemDef_s {int marker=42;struct {editFieldDef_s *editField=nullptr;multiDef_s *multi=nullptr;} focusTypeData;};
constexpr int VAL_STRING=2;
struct expressionRpn {int type=0;struct {struct {int dataType=VAL_STRING;struct {const char *string=nullptr;} internals;} constant;} data;};
struct ExpressionStatement {expressionRpn *rpn=nullptr;int numRpn=0;};
struct textExp_s {ExpressionStatement textExp;};
struct textDef_s {const char *text=nullptr;textExp_s *textExpData=nullptr;Rect textRect[1];struct {focusItemDef_s *focusItemDef=nullptr;} textTypeData;};
struct menuDef_t;
struct itemDef_s {Window window;int type=3,dataType=3;struct {textDef_s *textDef=nullptr;} typeData;menuDef_t *parent=nullptr;GenericEventHandler *onEvent=nullptr;const char *dvar=nullptr,*dvarTest=nullptr,*enableDvar=nullptr;void *rectExpData=nullptr,*animInfo=nullptr;int dvarFlags=0;ExpressionStatement visibleExp;unsigned long long showBits=0,hideBits=~0ull;};
struct menuDef_t {Window window;int itemCount=0;itemDef_s **items=nullptr;};
struct UiContext {};
struct UIInfo {UiContext uiDC;} ui;
struct {struct {bool enabled=false;} current;} running;
auto com_sv_running=&running;
std::map<std::string,std::string> strings{{"mapname",""},{"ui_mapname","mp_mountain"},{"ui_gametype","tdm"},{"sv_mapRotation","gametype dm map mp_nuked"},{"g_password","keep-password"}};
std::map<std::string,bool> flags;
std::map<std::string,int> integers{{"sv_maxclients",24}};
std::vector<std::string> commands;
int privateActions=0,quickMatches=0,focusEvents=0,guestResets=0;
bool loaded=false;
menuDef_t *mainMenu=nullptr,*privateLobby=nullptr,*splitTestLobby=nullptr;
#ifdef KISAK_EXPERIMENTAL_SPLITSCREEN
bool UI_SplitScreenIsCustomMenu(const menuDef_t*m){return m==splitTestLobby;}
#endif
GenericEventScript *nativePrivate=nullptr,*nativeRanked=nullptr;
#include <openblops_directory/directory_engine.h>
int I_stricmp(const char *a,const char *b){return std::strcmp(a,b);}
int Item_IsTextDefType(const itemDef_s *i){return i->typeData.textDef!=nullptr;}
void Dvar_SetBoolByName(const char *name,bool value){flags[name]=value;}
void _Dvar_RegisterString(const char*name,const char*value,int,const char*){if(!strings.count(name))strings[name]=value;}
struct LimitSetting {struct {int integer=0;}current;};
const LimitSetting*_Dvar_RegisterInt(const char*n,int value,int min,int max,unsigned flags,const char*){
 assert(std::string(n)=="bo1_host_maxplayers"&&min==0&&max==18&&flags==1);
 if(!integers.count(n))integers[n]=value;
 static LimitSetting setting;setting.current.integer=integers[n];return &setting;
}
#if defined(OPENBLOPS_NO_STEAM_AUTH) && defined(KISAK_MP)
bool pendingPassword=false;int passwordAccepts=0,passwordCancels=0;
bool Directory_HasPasswordPrompt(){return pendingPassword;}
bool Directory_AcceptJoinPassword(){if(!pendingPassword)return false;pendingPassword=false;++passwordAccepts;return true;}
void Directory_CancelJoinPassword(){if(pendingPassword)++passwordCancels;pendingPassword=false;}
#endif
int Dvar_GetInt(const char *name){return integers[name];}
void Dvar_SetIntByName(const char *name,int value){integers[name]=value;}
void Dvar_SetStringByName(const char *name,char *value){strings[name]=value;}
const char *Dvar_GetString(const char *name){return strings[name].c_str();}
void Com_Printf(int,const char *,...){}
void Com_PrintWarning(int,const char *,...){}
void CL_SplitScreenResetSession(){++guestResets;}
UIInfo *UI_GetInfo(int){return &ui;}
bool SV_Loaded(){return loaded;}
bool Com_IsMenuLevel(const char *){return strings["mapname"]=="ui";}
void Menus_CloseAll(int,UiContext *){
 // Asset/frontend rebuild during a queued map must not cancel publication.
 if(mainMenu)Directory_MenuOpening(0,&ui.uiDC,mainMenu);
 flags["onlinegame"]=true;
}
void Item_RunSingleEventScript(int,UiContext *dc,itemDef_s *,GenericEventScript *script){
 if(script==nativePrivate){++privateActions;strings["ui_mapname"]="mp_mountain";strings["ui_gametype"]="tdm";
  if(mainMenu)Directory_MenuOpening(0,dc,mainMenu);
  if(privateLobby)Directory_MenuOpening(0,dc,privateLobby);
 }else if(script==nativeRanked)++quickMatches;else ++focusEvents;
}
void Cbuf_AddText(int,const char *s){commands.emplace_back(s);}
char *va(const char *format,...){static char b[1024];va_list a;va_start(a,format);vsnprintf(b,sizeof(b),format,a);va_end(a);return b;}
"""
source=(r/'src/openblops_directory/directory_frontend.cpp').read_text();source=re.sub(r'^#include[^\n]*\n','',source,flags=re.M);code+=source
code+=r"""
int main(){
 focusItemDef_s focus;
 textDef_s rankedText,rankedDecoration,opsText,privateText,otherText,wagerText,wagerHighlightText;
 expressionRpn rankParts[2],opsParts[2];
 rankParts[0].data.constant.internals.string="@MENU_RANKED_MATCH_CAPS";rankParts[1].type=3;
 opsParts[0].data.constant.internals.string="@MENU_OPERATIONS_CAPS";opsParts[1].type=3;
 textExp_s rankExpr{{rankParts,2}},opsExpr{{opsParts,2}};
 // The shipped buttons and highlight overlays have no literal text at all.
 rankedText.textExpData=&rankExpr;rankedText.textTypeData.focusItemDef=&focus;
 rankedDecoration.textExpData=&rankExpr;opsText.textExpData=&opsExpr;opsText.textTypeData.focusItemDef=&focus;
 privateText.text="@MPUI_CUSTOM_MATCH_CAPS";otherText.text="OTHER";
 GenericEventScript rankedAction{"exec quickmatch ;"},privateAction{"open menu_xboxlive_privatelobby ;"},onFocus{"set ui_highlight 2 ;"};
 nativePrivate=&privateAction;nativeRanked=&rankedAction;
 GenericEventHandler rankFocus{"onFocus",&onFocus},rankEvent{"action",&rankedAction,&rankFocus},privateEvent{"action",&privateAction};
 itemDef_s rank,decor,ops,priv,other,wager,wagerHighlight;
 rank.window.name="SecondFlyout";rank.window.rect.y=rank.window.rectClient.y=57;rank.typeData.textDef=&rankedText;rank.onEvent=&rankEvent;
 decor.typeData.textDef=&rankedDecoration;decor.dataType=1;
 priv.window.name="FifthFlyout";priv.typeData.textDef=&privateText;priv.onEvent=&privateEvent;
 ops.typeData.textDef=&opsText;other.typeData.textDef=&otherText;
 wagerText.text="@MPUI_WAGER_MATCH_CAPS";wagerHighlightText.text="WAGER MATCH";wager.typeData.textDef=&wagerText;wagerHighlight.typeData.textDef=&wagerHighlightText;wagerHighlight.dataType=1;
 GenericEventScript wagerNative{"exec wagermatch ;"};GenericEventHandler wagerEvent{"action",&wagerNative};wager.onEvent=&wagerEvent;
 itemDef_s *flyItems[]{&rank,&decor,&other,&priv,&wager,&wagerHighlight};menuDef_t fly;fly.window.name="live_flyout";fly.items=flyItems;fly.itemCount=6;
 for(auto *i:flyItems)i->parent=&fly;
 itemDef_s opsHeader=ops,opsHighlight=ops,opsOther=ops;
 opsHeader.dataType=opsHighlight.dataType=opsOther.dataType=1;
 itemDef_s *mainItems[]{&ops,&opsHeader,&opsHighlight,&opsOther};menuDef_t main;main.window.name="main_text";main.items=mainItems;main.itemCount=4;
 for(auto *i:mainItems)i->parent=&main;mainMenu=&main;
 menuDef_t lobby;lobby.window.name="menu_xboxlive_privatelobby";privateLobby=&lobby;
 expressionRpn lobbyPart;lobbyPart.data.constant.internals.string="@MPUI_CUSTOM_MATCH_LOBBY_CAPS";
 textExp_s lobbyExpr{{&lobbyPart,1}};textDef_s lobbyText;lobbyText.textExpData=&lobbyExpr;
 itemDef_s lobbyTitle;lobbyTitle.dataType=1;lobbyTitle.typeData.textDef=&lobbyText;lobbyTitle.parent=&lobby;
 itemDef_s *lobbyItems[]{&lobbyTitle};lobby.items=lobbyItems;lobby.itemCount=1;
 Directory_MenuOpening(0,&ui.uiDC,&main);Directory_MenuOpening(0,&ui.uiDC,&fly);
#if defined(OPENBLOPS_NO_STEAM_AUTH) && defined(KISAK_MP)
 // The stock popup keeps its native field and buttons with a private draft binding.
 menuDef_t popup;popup.window.name="join_password_popmenu";
 editFieldDef_s popupEdit;focusItemDef_s popupFocus;popupFocus.focusTypeData.editField=&popupEdit;
 textDef_s popupText;popupText.textTypeData.focusItemDef=&popupFocus;
 itemDef_s input,joinButton;input.dataType=5;input.dvar="password";input.typeData.textDef=&popupText;input.parent=&popup;
 GenericEventScript submitAction{"uiScript JoinServer ;"};GenericEventHandler submitEvent{"action",&submitAction};
 joinButton.parent=&popup;joinButton.onEvent=&submitEvent;
 itemDef_s *popupItems[]{&input,&joinButton};popup.items=popupItems;popup.itemCount=2;
 pendingPassword=true;Directory_MenuOpening(0,&ui.uiDC,&popup);
 auto*popupField=popup.items[0];assert(popupField!=&input&&std::string(popupField->dvar)=="ui_directory_joinPassword");
 assert(popupField->typeData.textDef!=&popupText&&std::string(input.dvar)=="password");
 assert(Directory_HandleMenuEvent(0,&ui.uiDC,&joinButton,"action")&&passwordAccepts==1);
 pendingPassword=true;assert(Directory_HandleMenuEvent(0,&ui.uiDC,popupField,"onAccept")&&passwordAccepts==2);
 pendingPassword=true;assert(!Directory_HandleMenuEvent(0,&ui.uiDC,&joinButton,"onESC")&&passwordCancels==1);
 pendingPassword=true;assert(!Directory_HandleMenuEvent(0,&ui.uiDC,&joinButton,"onClose")&&passwordCancels==2);
 Directory_MenuOpening(0,&ui.uiDC,&popup);assert(std::string(popup.items[0]->dvar)=="password"); // Native LAN popup compatibility.
 for(int i=0;i<main.itemCount;++i){
  assert(std::string(main.items[i]->typeData.textDef->text)=="CREATE A CLASS");
  assert(main.items[i]->typeData.textDef->textExpData==nullptr);
 }
 assert(opsText.text==nullptr&&opsText.textExpData==&opsExpr&&std::string(opsParts[0].data.constant.internals.string)=="@MENU_OPERATIONS_CAPS");
 assert(std::string(fly.items[0]->typeData.textDef->text)=="HOST MATCH");
 assert(std::string(fly.items[1]->typeData.textDef->text)=="HOST MATCH");
 assert(rankedText.text==nullptr&&rankedText.textExpData==&rankExpr&&std::string(rankParts[0].data.constant.internals.string)=="@MENU_RANKED_MATCH_CAPS");
 assert(fly.itemCount==6&&fly.items[2]==&other&&fly.items[3]!=&priv);
 assert(std::string(fly.items[3]->typeData.textDef->text)=="OFFLINE MATCH"&&fly.items[3]->onEvent==priv.onEvent);
 assert(std::string(fly.items[4]->typeData.textDef->text).empty()&&std::string(fly.items[5]->typeData.textDef->text).empty());
 assert(fly.items[4]->onEvent==&wagerEvent&&std::string(wagerText.text)=="@MPUI_WAGER_MATCH_CAPS");
 auto *host=fly.items[0];auto **patched=fly.items;
 assert(host->window.rect.y==57&&host->window.background==123&&host->window.style==3);
 assert(host->window.foreColor[0]==rank.window.foreColor[0]&&host->typeData.textDef->textExpData==nullptr);
 Directory_MenuOpening(0,&ui.uiDC,&fly);assert(fly.items==patched);
 assert(Directory_HandleMenuEvent(0,&ui.uiDC,host,"onFocus")&&focusEvents==1);
 assert(strings["ui_lobby_desc"].find("advertise")!=std::string::npos);
 assert(!Directory_HandleMenuEvent(1,&ui.uiDC,host,"action"));
 assert(Directory_HandleMenuEvent(0,&ui.uiDC,host,"action"));
 assert(privateActions==1&&quickMatches==0&&!flags["bo1_directory_publish"]&&commands.empty());
 assert(std::string(lobby.items[0]->typeData.textDef->text)=="HOST MATCH"&&lobbyText.textExpData==&lobbyExpr);
 auto*hostCaption=lobby.items[0];Directory_MenuOpening(0,&ui.uiDC,&lobby);assert(lobby.items[0]==hostCaption);
 // Use the native settings panel while Host Match is configuring.
 menuDef_t settings;settings.window.name="server_settings";
 editFieldDef_s nameEdit,passwordEdit,maxEdit;passwordEdit.maxChars=12;
 focusItemDef_s nameFocus,passwordFocus,maxFocus;maxFocus.focusTypeData.editField=&maxEdit;nameFocus.focusTypeData.editField=&nameEdit;passwordFocus.focusTypeData.editField=&passwordEdit;
 textDef_s nameText,passwordText,maxText;maxText.textTypeData.focusItemDef=&maxFocus;nameText.textTypeData.focusItemDef=&nameFocus;passwordText.textTypeData.focusItemDef=&passwordFocus;
 itemDef_s nameField,passwordField,nameLabel,passwordLabel,maxField,maxLabel,maxHighlight,unused,back;
 nameField.dataType=passwordField.dataType=5;
 nameField.dvar="sv_hostname";nameField.window.rect=nameField.window.rectClient={-200,-131,375,17};nameField.typeData.textDef=&nameText;
 passwordField.dvar="g_password";passwordField.window.rect=passwordField.window.rectClient={-200,-55,375,17};passwordField.typeData.textDef=&passwordText;
 nameLabel.window.rect=nameLabel.window.rectClient=nameField.window.rect;nameLabel.dataType=1;
 passwordLabel.window.rect=passwordLabel.window.rectClient=passwordField.window.rect;passwordLabel.dataType=1;
 maxField.type=maxField.dataType=7;maxField.dvar="sv_maxclients";
 maxField.window.rect=maxField.window.rectClient={-200,59,375,17};maxField.typeData.textDef=&maxText;maxField.visibleExp.numRpn=2;
 maxLabel.window.rect=maxLabel.window.rectClient=maxField.window.rect;maxLabel.dataType=1;maxLabel.visibleExp.numRpn=2;
 maxHighlight=maxLabel;maxHighlight.visibleExp.numRpn=14;
 unused.dvar="sv_vac";unused.window.rect=unused.window.rectClient={-200,78,375,17};
 back.window.rect=back.window.rectClient={1,193.5,10,20};
 GenericEventScript closeNative{"close self ;"};GenericEventHandler closeAction{"action",&closeNative};back.onEvent=&closeAction;
 itemDef_s *settingsItems[]{&nameLabel,&nameField,&unused,&passwordLabel,&passwordField,&maxLabel,&maxHighlight,&maxField,&back};settings.items=settingsItems;settings.itemCount=9;
 Directory_MenuOpening(0,&ui.uiDC,&settings);assert(settings.itemCount==8);
 assert(settings.items[0]!=&nameLabel&&settings.items[1]!=&nameField&&settings.items[7]==&back&&settings.items[7]->onEvent==&closeAction);
 auto*editName=settings.items[1];auto*editPassword=settings.items[3];
 auto*limit=settings.items[6];assert(limit->type==10&&limit->dataType==10&&std::string(limit->dvar)=="bo1_host_maxplayers"&&limit->window.rect.y==-55);
 auto*choices=limit->typeData.textDef->textTypeData.focusItemDef->focusTypeData.multi;
 assert(choices&&choices->count==19&&std::string(choices->dvarList[0])=="DEFAULT"&&choices->dvarValue[0]==0&&choices->dvarValue[18]==18);
 assert(limit->visibleExp.numRpn==0&&maxField.type==7&&maxField.visibleExp.numRpn==2);
 assert(std::string(settings.items[5]->window.name)=="openblops_host_maxplayers_hl");
 Dvar_SetIntByName(limit->dvar,8);Directory_MenuOpening(0,&ui.uiDC,&settings);assert(integers["bo1_host_maxplayers"]==8);
 Dvar_SetIntByName(limit->dvar,0);
 assert(std::string(editName->dvar)=="bo1_directory_name"&&std::string(editPassword->dvar)=="g_password");
 assert(editName->typeData.textDef->textTypeData.focusItemDef->focusTypeData.editField!=&nameEdit);
 assert(editName->typeData.textDef->textTypeData.focusItemDef->focusTypeData.editField->maxChars==48&&nameEdit.maxChars==20);
 assert(editPassword->window.rect.y==-93&&passwordField.window.rect.y==-55&&editPassword->typeData.textDef!=&passwordText);
 auto**settingsPatched=settings.items;Directory_MenuOpening(0,&ui.uiDC,&settings);assert(settings.items==settingsPatched&&settings.itemCount==8);
 Dvar_SetStringByName(editName->dvar,const_cast<char*>("Erick's game server"));
 Dvar_SetStringByName(editPassword->dvar,const_cast<char*>("secret123"));
 Directory_MenuOpening(0,&ui.uiDC,&lobby); // Returning from Settings must keep public setup armed.
 strings["ui_mapname"]="mp_nuked;quit";assert(Directory_StartHostMatch());assert(commands.empty());
 strings["ui_mapname"]="mp_nuked";strings["ui_gametype"]="dom";
 assert(Directory_StartHostMatch()&&commands.size()==1);
 assert(commands.back().find("map mp_nuked")!=std::string::npos&&strings["g_gametype"]=="dom");
 assert(flags["bo1_directory_publish"]&&flags["bo1_directory_enabled"]&&flags["systemlink"]&&!flags["onlinegame"]&&!flags["playlist_enabled"]);
 assert(strings["g_password"]=="secret123"&&strings["bo1_directory_name"]=="Erick's game server"&&strings["sv_hostname"]=="Erick's game server");
 assert(integers["sv_maxclients"]==18&&integers["party_maxplayers"]==18);
 Directory_MenuOpening(0,&ui.uiDC,&main);assert(flags["bo1_directory_publish"]);
 running.current.enabled=true;loaded=true;strings["mapname"]="mp_nuked";Directory_FrontendFrame();
 running.current.enabled=false;strings["mapname"]="";Directory_FrontendFrame();assert(!flags["bo1_directory_publish"]&&!Directory_StartHostMatch());
 Directory_HandleMenuEvent(0,&ui.uiDC,host,"action");
 assert(!Directory_HandleMenuEvent(0,&ui.uiDC,&priv,"action"));assert(!Directory_StartHostMatch()&&!flags["bo1_directory_publish"]);
 Directory_MenuOpening(0,&ui.uiDC,&lobby);assert(std::string(lobby.items[0]->typeData.textDef->text)=="OFFLINE MATCH");
 Directory_HandleMenuEvent(0,&ui.uiDC,host,"action");Directory_MenuOpening(0,&ui.uiDC,&main);assert(!Directory_StartHostMatch());
 Directory_HandleMenuEvent(0,&ui.uiDC,host,"action");strings["g_password"]="";strings["ui_gametype"]="dm";integers["sv_maxclients"]=4;assert(Directory_StartHostMatch());assert(integers["sv_maxclients"]==4&&strings["g_password"].empty());
 // Explicit settings override all automatic floors and apply to private starts too.
 for(int selected:{1,4,8,12,18}){
  integers["bo1_host_maxplayers"]=selected;integers["sv_maxclients"]=24;
  Directory_ApplyHostPlayerLimit("tdm",true);assert(integers["sv_maxclients"]==selected&&integers["party_maxplayers"]==selected);
  integers["sv_maxclients"]=24;Directory_ApplyHostPlayerLimit("hctdm",false);assert(integers["sv_maxclients"]==selected&&integers["party_maxplayers"]==selected);
 }
 integers["bo1_host_maxplayers"]=0;
 // TDM must not inherit the old 8-slot host cap; larger selections remain.
 for(const char*tdm:{"tdm","hctdm"}){
  Directory_HandleMenuEvent(0,&ui.uiDC,host,"action");strings["ui_gametype"]=tdm;integers["sv_maxclients"]=8;
  assert(Directory_StartHostMatch()&&integers["sv_maxclients"]==12&&integers["party_maxplayers"]==12);
 }
 Directory_HandleMenuEvent(0,&ui.uiDC,host,"action");strings["ui_gametype"]="tdm";integers["sv_maxclients"]=18;
 assert(Directory_StartHostMatch()&&integers["sv_maxclients"]==18&&integers["party_maxplayers"]==18);
#ifdef KISAK_EXPERIMENTAL_SPLITSCREEN
 menuDef_t split=lobby;itemDef_s splitTitle=*lobby.items[0];textDef_s splitText=*splitTitle.typeData.textDef;
 splitTitle.typeData.textDef=&splitText;splitTitle.parent=&split;itemDef_s*splitItems[]{&splitTitle};split.items=splitItems;splitTestLobby=&split;
 Directory_MenuOpening(0,&ui.uiDC,&split);assert(std::string(split.items[0]->typeData.textDef->text)=="SPLIT-SCREEN");
 assert(std::string(lobby.items[0]->typeData.textDef->text)=="HOST MATCH"&&split.items[0]!=lobby.items[0]);
#endif
 puts("PASS: Host Match/Offline Match captions, native row styles/ordering, Operations rename, private-setup reuse, map/mode choice, server-name/password binding, private edit state, publication lifecycle and cancellation");
#else
 assert(fly.items==flyItems&&main.items==mainItems);
 assert(rankedText.text==nullptr&&rankedText.textExpData==&rankExpr&&std::string(rankParts[0].data.constant.internals.string)=="@MENU_RANKED_MATCH_CAPS"&&opsText.text==nullptr&&opsText.textExpData==&opsExpr&&std::string(opsParts[0].data.constant.internals.string)=="@MENU_OPERATIONS_CAPS");
 assert(!Directory_HandleMenuEvent(0,&ui.uiDC,&rank,"action")&&!Directory_StartHostMatch());
 puts("PASS: non-direct menu behavior unchanged; direct SP covered by test-zombies-directory.py");
#endif
}
"""
cpp=out/'frontend.cpp';cpp.write_text(code)
env=os.environ.copy();env['PATH']=str(Path(a.compiler).parent)+os.pathsep+env.get('PATH','')
for name,defines in [('pc-direct-mp',['KISAK_MP','OPENBLOPS_NO_STEAM_AUTH','KISAK_EXPERIMENTAL_SPLITSCREEN']),('nx-direct-mp',['KISAK_MP','KISAK_NX','OPENBLOPS_NO_STEAM_AUTH']),('legacy-mp',['KISAK_MP']),('legacy-sp',['KISAK_SP'])]:
 exe=out/(name+'.exe')
 subprocess.run([a.compiler,'-std=c++20','-I'+str(r/'src'),*[('-D'+d) for d in defines],str(cpp),str(r/'src/openblops_directory/directory_protocol.cpp'),'-o',str(exe)],check=True,env=env)
 subprocess.run([str(exe)],check=True,env=env)
 print(name+': passed',flush=True)
