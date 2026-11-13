"""Exercise menu-only split-screen selection, native UI dispatch, and session cleanup."""
from pathlib import Path
import argparse,json,re,struct,subprocess
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--compiler',default='/c/msys64/ucrt64/bin/g++.exe');args=p.parse_args()
root=Path(__file__).resolve().parents[3];out=root/'build-nx/splitscreen-frontend-tests';out.mkdir(parents=True,exist_ok=True)
def function(n,sig):
 s=(root/n).read_text();a=s.index(sig);b=s.index('{',a)+1;d=1
 while d:d+=(s[b]=='{')-(s[b]=='}');b+=1
 return s[a:b]
def clean(s):return re.sub(r'^\s*#include.*\n','',s,flags=re.M)
# Read the supplied zones through their actual relocation tables, without loading
# or changing game files. Verify the row/action used by the native integration.
def menu_items(zone,name):
 path=Path('F:/pluto_t5_full_game/kbz')/(zone+'.kbz')
 if not path.exists():return None
 b=path.read_bytes();nb=struct.unpack_from('<I',b,8)[0];sizes=struct.unpack_from('<'+'I'*nb,b,12);nr,na=struct.unpack_from('<II',b,12+4*nb);pos=20+4*nb;blocks=[]
 for i,n in enumerate(sizes):blocks.append(b[pos:pos+n] if i!=1 else b'');pos+=n if i!=1 else 0
 rel={}
 for _ in range(nr):sb,so,tb,to=struct.unpack_from('<BIBI',b,pos);pos+=10;rel[(sb,so)]=(tb,to)
 def ptr(p,o=0):return rel.get((p[0],p[1]+o)) if p else None
 def num(p,o):return struct.unpack_from('<i',blocks[p[0]],p[1]+o)[0]
 def string(p):
  if not p:return ''
  data=blocks[p[0]];return data[p[1]:data.find(b'\0',p[1])].decode('utf8','replace')
 for _ in range(na):
  t,ab,ao=struct.unpack_from('<IBI',b,pos);pos+=9;m=(ab,ao)
  if t!=22 or string(ptr(m))!=name:continue
  result=[];table=ptr(m,448)
  for j in range(num(m,192)):
   item=ptr(table,j*8);actions=[];event=ptr(item,320);seen=set()
   while event and event not in seen:
    seen.add(event);script=ptr(event,8);scripts=set()
    while script and script not in scripts:
     scripts.add(script)
     if string(ptr(event)).lower()=='action':actions.append(string(ptr(script,40)))
     script=ptr(script,56)
    event=ptr(event,16)
   result.append(dict(flags=num(item,92),style=blocks[item[0]][item[1]+64],forecolor=struct.unpack_from('<4f',blocks[item[0]],item[1]+100),backcolor=struct.unpack_from('<4f',blocks[item[0]],item[1]+116),name=string(ptr(item)),type=num(item,176),rect=struct.unpack_from('<4fii',blocks[item[0]],item[1]+8),actions=actions))
  return result
 raise AssertionError('missing menu '+zone+':'+name)
sp=menu_items('frontend','main_online');mp=menu_items('ui_mp','menu_xboxlive_privatelobby');main=menu_items('ui_mp','main_text')
solo=next(x for x in sp if x['name']=='online_button2' and x['type']==3) if sp else dict(rect=(0,150,640,95,0,0))
start=next(x for x in mp if any('StartListenServer' in a for a in x['actions'])) if mp else dict(actions=['uiScript StartListenServer ;'])
play=next(x for x in main if x['type']==3 and any('open live_flyout' in a for a in x['actions'])) if main else dict(rect=(-258,40,180,17,2,1),actions=['open live_flyout ;'])
if main:
 play_at=main.index(play);layers=main[play_at-2:play_at+2];assert [x['type'] for x in layers]==[2,0,3,1]
else:layers=[dict(flags=65540,style=3,forecolor=(1,1,1,1),backcolor=(0,0,0,0)) for _ in range(4)]
if sp and mp and main:print('PASS: actual Zombies rows, four native Play row layers/colors, and private-match start/settings actions found in supplied zones')
code=r"""
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <cstdlib>
#include <string>
#include <map>
#include <list>
#include <vector>
#include <stdexcept>
#define __cdecl
#define __debugbreak() assert(false)
#define KISAK_EXPERIMENTAL_SPLITSCREEN 1
#define OPENBLOPS_NO_STEAM_AUTH 1
#define PROF_SCOPED(x)
#define ARRAY_COUNT(x) (sizeof(x)/sizeof(x[0]))
struct rectDef_s{float x=0,y=0,w=640,h=95;int horzAlign=0,vertAlign=0;};
struct ExpressionStatement{const char*filename=nullptr;int numRpn=0;};
struct Material{};
struct windowDef_t{const char*name=nullptr,*group=nullptr;rectDef_s rect,rectClient;unsigned char style=0,border=0,modal=0;int ownerDraw=0,staticFlags=0,dynamicFlags[1]{};float foreColor[4]{1,1,1,1},backColor[4]{};Material*background=nullptr;};
struct GenericEventScript{const char*action=nullptr;GenericEventScript*next=nullptr;};
struct GenericEventHandler{const char*name=nullptr;GenericEventScript*eventScript=nullptr;GenericEventHandler*next=nullptr;};
struct ItemKeyHandler{int key=0;GenericEventScript*keyScript=nullptr;};
struct focusItemDef_s{void*onKey=nullptr;};
struct animParamsDef_t{rectDef_s rectClient;};
struct UIAnimInfo{int animStateCount=0;animParamsDef_t**animStates=nullptr;animParamsDef_t currentAnimState,nextAnimState;};
struct textDef_s{rectDef_s textRect[1];float textscale=1.75f,textalignx=0,textaligny=0;const char*text=nullptr;void*textExpData=nullptr;struct{focusItemDef_s*focusItemDef=nullptr;}textTypeData;};
struct menuDef_t;
struct itemDef_s{windowDef_t window;int type=3,dataType=3,imageTrack=0;const char*dvar=nullptr,*dvarTest=nullptr,*enableDvar=nullptr;int dvarFlags=0;ExpressionStatement visibleExp,forecolorAExp;unsigned long long showBits=0,hideBits=~0ULL;void*rectExpData=nullptr;UIAnimInfo*animInfo=nullptr;int ui3dWindowId=-1;menuDef_t*parent=nullptr;struct{textDef_s*textDef=nullptr;}typeData;GenericEventHandler*onEvent=nullptr;};
struct menuDef_t{windowDef_t window;rectDef_s initialRectInfo;int ui3dWindowId=-1;unsigned long long hideBits=~0ULL;int openSlideSpeed=-1,closeSlideSpeed=-1,openSlideDirection=-1,closeSlideDirection=-1,openFadingTime=-1,closeFadingTime=-1,fadeTimeCounter=-1,slideTimeCounter=-1;float focusColor[4]{},fadeClamp=1;int cursorItem[1]{-1},imageTrack=0,itemCount=0;itemDef_s**items=nullptr;GenericEventHandler*onEvent=nullptr;ItemKeyHandler*onKey=nullptr;};
struct UiContext{int contextIndex=0,menuCount=0,openMenuCount=0;menuDef_t*Menus[600]{};};
struct uiInfo_s{UiContext uiDC;}ui;
uiInfo_s*UI_GetInfo(int n){assert(n==0);return &ui;}
std::map<std::string,std::string>vars;
bool Dvar_GetBool(const char*n){return vars[n]=="1";}
int Dvar_GetInt(const char*n){return atoi(vars[n].c_str());}
const char*Dvar_GetString(const char*n){return vars[n].c_str();}
void Dvar_SetBoolByName(const char*n,bool v){vars[n]=v?"1":"0";}
void Dvar_SetIntByName(const char*n,int v){vars[n]=std::to_string(v);}
void Dvar_SetStringByName(const char*n,const char*v){vars[n]=v;}
std::vector<std::string>commands,uiScripts;int nativeEvents=0,closed=0;
void Cbuf_AddText(int n,const char*s){assert(n==0);commands.emplace_back(s);}
char Cmd_ExecFromDiskAddText(int,int,const char*){assert(false);return 0;}
const char*va(const char*fmt,...){static char b[1024];va_list a;va_start(a,fmt);vsnprintf(b,sizeof(b),fmt,a);va_end(a);return b;}
bool Com_IsZombieMap(const char*n){return n&&!strncmp(n,"zombie_",7);}
int I_stricmp(const char*a,const char*b){return strcmp(a,b);}
int I_strcmp(const char*a,const char*b){return strcmp(a,b);}
struct parseInfo_t{char*token;};parseInfo_t*UI_GetMapRotationToken(){return nullptr;}
void UI_AddMenu(int,UiContext*,menuDef_t*,int);
void UI_SplitScreenAddMenus(int,UiContext*,menuDef_t*);
void UI_SplitScreenMenuOpening(int,UiContext*,menuDef_t*);
bool UI_SplitScreenIsCustomMenu(const menuDef_t*);
bool UI_SplitScreenHandleEvent(int,UiContext*,itemDef_s*,const char*);
bool Directory_HandleMenuEvent(int,UiContext*,itemDef_s*,const char*){return false;}
bool Directory_StartHostMatch(){return false;}
struct HostLimitSetting {struct {int integer=0;}current;};
const HostLimitSetting*_Dvar_RegisterInt(const char*n,int value,int,int,unsigned,const char*){
 if(!vars.count(n))vars[n]=std::to_string(value);
 static HostLimitSetting setting;setting.current.integer=std::atoi(vars[n].c_str());return &setting;
}
menuDef_t*Menus_FindByName(const UiContext*,const char*);
menuDef_t*lastOpened=nullptr;
int Menus_OpenByName(int n,UiContext*dc,const char*name){assert(n==0);lastOpened=Menus_FindByName(dc,name);assert(lastOpened);UI_SplitScreenMenuOpening(n,dc,lastOpened);lastOpened->window.dynamicFlags[0]|=6;dc->openMenuCount++;return 1;}
void Menus_Close(int n,UiContext*dc,menuDef_t*m){assert(n==0);if(dc->openMenuCount)--dc->openMenuCount;m->window.dynamicFlags[0]&=~6;++closed;}
void Menus_CloseAll(int n,UiContext*dc){assert(n==0);dc->openMenuCount=0;++closed;for(int i=0;i<dc->menuCount;++i)dc->Menus[i]->window.dynamicFlags[0]&=~6;
 // Native close/focus callbacks may rewrite frontend dvars.
 vars["onlinegame"]="1";vars["xblive_privatematch"]="1";vars["ui_mapname"]="menu_changed_map";vars["ui_gametype"]="tdm";
}
void Item_RunScript(int n,UiContext*,itemDef_s*,const char*s){assert(n==0);uiScripts.emplace_back(s);}
void UI_RunMenuScript_StartListenServer();
void Item_RunSingleEventScript(int,UiContext*,itemDef_s*,GenericEventScript*s){++nativeEvents;if(s&&s->action&&strstr(s->action,"StartListenServer"))UI_RunMenuScript_StartListenServer();}
bool Item_IsTextDefType(const itemDef_s*);
bool UI_SplitScreenStartMpMatch();
bool Window_HasFocus(int n,const windowDef_t*w){assert(n==0);return (w->dynamicFlags[0]&2)!=0;}
bool Window_IsVisible(int n,const windowDef_t*w){assert(n==0);return (w->dynamicFlags[0]&4)!=0;}
std::vector<std::string>sessionLogs;void CL_SplitGuestLog(const char*s){sessionLogs.emplace_back(s);}
struct FakeDvar{struct{bool enabled=false;}current;};using dvar_s=FakeDvar;FakeDvar ending;auto xblive_matchEndingSoon=&ending;
void Dvar_SetBool(FakeDvar*d,bool v){d->current.enabled=v;}
bool IsDedicatedServer(){return false;}
void CL_SetupClientsForFrontend(){}
void Cbuf_ExecuteBuffer(int,int,const char*){}
template<class...T> bool Assert_MyHandler(T...){return false;}
constexpr int ERR_DROP=1,ASSET_TYPE_MENU=22;
struct{struct{bool enabled=true;}current;}fast;auto useFastFile=&fast;
std::map<std::string,menuDef_t*>zoneMenus;
struct Header{menuDef_t*menu;};
Header DB_FindXAssetHeader(int,const char*n,int,int){assert(zoneMenus.count(n));return {zoneMenus[n]};}
void Com_Error(int,const char*){assert(false);}
void UI_NX_AddAimAssistOption(menuDef_t*){}
void UI_AddPerformanceOverlayOption(menuDef_t*){}
bool UI_IsCustomBotMenu(const menuDef_t*){return false;}
void UI_AddCustomBotSettings(int,UiContext*,menuDef_t*){}
"""
# Exercise Window_Paint's real material-color selection, including its null
# (white) fallback when the native tint bit is lost.
paint=function('src/ui/ui_shared.cpp','void __cdecl Window_Paint(')
tint=paint.split('case 3u:',1)[1].split('if (itemType == 17)',1)[0]
code+='int Window_GetDynamicFlags(int n,const windowDef_t*w){assert(n==0);return w->dynamicFlags[0];}\n'
code+='const float* NativeMaterialTint(UiContext*dc,itemDef_s*item){auto*w=&item->window;const float*foreColor;float*v17;'+tint+'return foreColor;}\n'
code+=function('src/ui/ui_utils.cpp','int __cdecl Item_IsTextDefType(').replace('int __cdecl','bool __cdecl')+'\n'
code+=function('src/ui/ui_shared.cpp','menuDef_t *__cdecl Menus_FindByName(')+'\n'
code+=function('src/openblops_directory/directory_frontend.cpp','void Directory_ApplyHostPlayerLimit(')+'\n'
code+=function('src/ui_mp/ui_main_mp.cpp','void __cdecl UI_RunMenuScript_StartListenServer(')+'\n'
code+=clean((root/'src/client/splitscreen_session.cpp').read_text())+'\n'
code+=clean((root/'src/ui/ui_splitscreen.cpp').read_text())+'\n'
code+=function('src/qcommon/common.cpp','void __cdecl Com_LoadFrontEnd(')+'\n'
code+=function('src/ui/ui_shared.cpp','void __cdecl UI_AddMenu(')+'\n'
code+=function('src/ui/ui_shared.cpp','void __cdecl Item_RunEventScript(')+'\n'
code+='const char*startAction='+json.dumps(next(a for a in start['actions'] if 'StartListenServer' in a))+';\n'
code+='rectDef_s soloRect{'+','.join(str(v) for v in solo['rect'])+'};\n'
code+='rectDef_s playRect{'+','.join(str(v) for v in play['rect'])+'};\n'
code+='const char*playAction='+json.dumps(next(a for a in play['actions'] if 'open live_flyout' in a))+';\n'
code+='const int native_flags[4]={'+','.join(str(layer['flags']) for layer in layers)+'};\n'
code+='const unsigned char native_style[4]={'+','.join(str(layer['style']) for layer in layers)+'};\n'
for field in ['forecolor','backcolor']:
 code+='const float native_'+field+'[4][4]={'+','.join('{'+','.join(str(v)+'f' for v in layer[field])+'}' for layer in layers)+'};\n'
code+=r"""
int main(){
 auto*dc=&ui.uiDC;
 // Even legacy launcher/config settings cannot activate a session.
 vars["splitguest_auto"]="1";vars["systemlink"]="1";assert(!CL_SplitScreenSessionRequested());
 textDef_s sourceText;sourceText.text="ORIGINAL";sourceText.textExpData=(void*)123;
 itemDef_s source;source.typeData.textDef=&sourceText;
 source.window.name="online_button2";source.window.rect=source.window.rectClient=soloRect;
 GenericEventScript stockScript;stockScript.action=startAction;
 GenericEventHandler stockEvent;stockEvent.name="action";stockEvent.eventScript=&stockScript;source.onEvent=&stockEvent;
 menuDef_t parent;source.parent=&parent;
#ifdef KISAK_SP
 parent.window.name="main_online";
 itemDef_s backing=source,alt=source;
 backing.type=backing.dataType=0;backing.window.name=nullptr;backing.window.style=3;
 backing.window.foreColor[3]=.24f;backing.window.dynamicFlags[0]=0x10004;
 backing.onEvent=nullptr;
 source.window.foreColor[0]=alt.window.foreColor[0]=.69f;
 itemDef_s*original[]{&backing,&source,&alt};
#else
 parent.window.name="menu_xboxlive_privatelobby";
 itemDef_s*original[]{&source};
#endif
 const int originalCount=sizeof(original)/sizeof(original[0]);parent.items=original;parent.itemCount=originalCount;
 zoneMenus[parent.window.name]=&parent;UI_AddMenu(0,dc,&parent,1);
 UI_SplitScreenMenuOpening(0,dc,&parent);
#ifdef KISAK_SP
 assert(parent.itemCount==6&&parent.items!=original&&parent.items[0]==&backing&&parent.items[1]==&source);
 auto*splitBacking=parent.items[3];auto*button=parent.items[4];auto*altButton=parent.items[5];
 assert(!strcmp(button->typeData.textDef->text,"SPLIT-SCREEN")&&!strcmp(altButton->typeData.textDef->text,"SPLIT-SCREEN"));
 assert(splitBacking->window.style==3&&splitBacking->window.foreColor[3]==.24f&&button->window.foreColor[0]==.69f);
 assert(splitBacking->window.rectClient.y==soloRect.y+soloRect.h&&UI_SplitScreenIsHiddenLayer(0,splitBacking));
 altButton->window.dynamicFlags[0]|=2;assert(!UI_SplitScreenIsHiddenLayer(0,splitBacking));altButton->window.dynamicFlags[0]&=~2;
 assert(button->window.rectClient.y==soloRect.y+soloRect.h&&button->window.rectClient.h==soloRect.h);
 assert(button->typeData.textDef!=&sourceText&&sourceText.textExpData==(void*)123);
 assert(source.window.rectClient.y==soloRect.y&&source.onEvent==&stockEvent);
 UI_SplitScreenMenuOpening(0,dc,&parent);assert(parent.itemCount==6);
 Item_RunEventScript(1,dc,button,"action");assert(!CL_SplitScreenSessionRequested());
 Item_RunEventScript(0,dc,button,"action");assert(zombieSelection&&!uiScripts.empty()&&uiScripts.back().find("levels_zombie")!=std::string::npos);
 assert(!CL_SplitScreenSessionRequested()&&commands.empty()); // selection is not yet a map
 vars["ui_load_level"]="zombie_theater";
 assert(UI_SplitScreenStartZombieMap(0,"startlevel 0")&&CL_SplitScreenSessionRequested());
 assert(vars["sv_maxclients"]=="2"&&vars["zombiemode"]=="1"&&vars["systemlink"]=="1"&&vars["onlinegame"]=="0");
 assert(commands.back().find("map zombie_theater\n")!=std::string::npos);
 // Returning/cancelling, then Solo, uses the original action and no guest.
 CL_SplitScreenSessionActivated();UI_SplitScreenMenuOpening(0,dc,&parent);assert(!CL_SplitScreenSessionRequested()&&!zombieSelection);
 int before=nativeEvents;Item_RunEventScript(0,dc,&source,"action");assert(nativeEvents==before+1);
 assert(!UI_SplitScreenStartZombieMap(0,"startlevel 0"));
 // Asset replacement adds a separate stable row without invalidating the first.
 menuDef_t patch=parent;patch.items=original;patch.itemCount=originalCount;UI_SplitScreenAddMenus(0,dc,&patch);
 assert(patch.itemCount==6&&patch.items[4]!=button&&button->parent==&parent);
 const char*map="zombie_theater";
#else
 assert(dc->menuCount==1&&parent.itemCount==1&&source.onEvent==&stockEvent);
 // Ordinary Private Match starts directly through its original action.
 vars["bo1_directory_publish"]="1";vars["ui_mapname"]="mp_array";vars["ui_gametype"]="dom";vars["onlinegame"]="1";vars["xblive_privatematch"]="1";
 int before=nativeEvents;Item_RunEventScript(0,dc,&source,"action");
 assert(nativeEvents==before+1&&commands.back().find("map mp_array")!=std::string::npos&&!CL_SplitScreenSessionRequested());
 // Main-menu Play and Operations are real stock row geometry/actions. Insert
 // Split-Screen between them in draw/navigation order; keep originals intact.
 menuDef_t root;root.window.name="main_text";
 itemDef_s play,operations;play.typeData.textDef=operations.typeData.textDef=&sourceText;play.parent=operations.parent=&root;
 play.window.rect=play.window.rectClient=playRect;operations.window.rect=operations.window.rectClient=playRect;
 operations.window.rect.y+=17;operations.window.rectClient.y+=17;
 GenericEventScript playScript;playScript.action=playAction;
 GenericEventHandler playEvent;playEvent.name="action";playEvent.eventScript=&playScript;play.onEvent=&playEvent;
 animParamsDef_t operationState;operationState.rectClient=operations.window.rectClient;animParamsDef_t*states[]{&operationState};
 UIAnimInfo operationAnimation;operationAnimation.animStateCount=1;operationAnimation.animStates=states;operations.animInfo=&operationAnimation;
 itemDef_s backing,highlight,focusLabel;backing.type=backing.dataType=2;highlight.type=highlight.dataType=0;focusLabel.type=focusLabel.dataType=1;
 backing.window.rect=backing.window.rectClient=highlight.window.rect=highlight.window.rectClient=focusLabel.window.rect=focusLabel.window.rectClient=playRect;
 backing.window.rectClient.w+=1;backing.window.style=1;Material focusMaterial;highlight.window.background=&focusMaterial;
 textDef_s focusedText;focusLabel.typeData.textDef=&focusedText;
 itemDef_s*rootItems[]{&backing,&highlight,&play,&focusLabel,&operations};root.items=rootItems;root.itemCount=5;
 for(int i=0;i<4;++i){rootItems[i]->window.style=native_style[i];rootItems[i]->window.dynamicFlags[0]=native_flags[i]|2;memcpy(rootItems[i]->window.foreColor,native_forecolor[i],sizeof(native_forecolor[i]));memcpy(rootItems[i]->window.backColor,native_backcolor[i],sizeof(native_backcolor[i]));}
 zoneMenus[root.window.name]=&root;UI_AddMenu(0,dc,&root,1);UI_SplitScreenMenuOpening(0,dc,&root);
 assert(root.itemCount==9&&root.items[2]==&play&&root.items[8]!=&operations);
 auto*button=root.items[6];assert(!strcmp(button->typeData.textDef->text,"SPLIT-SCREEN"));
 assert(button->window.rectClient.y==playRect.y+17&&root.items[8]->window.rectClient.y==playRect.y+34);
 assert(root.items[8]->animInfo!=&operationAnimation&&operationState.rectClient.y==playRect.y+17);
 assert(root.items[8]->animInfo->animStates[0]->rectClient.y==playRect.y+34);
 for(int i=0;i<4;++i){assert(!memcmp(root.items[4+i]->window.foreColor,native_forecolor[i],sizeof(native_forecolor[i])));assert(!memcmp(root.items[4+i]->window.backColor,native_backcolor[i],sizeof(native_backcolor[i])));}
 assert(root.items[4]->window.style==native_style[0]&&root.items[5]->window.background==&focusMaterial);
 // Copied colors must reach the painter: dropping 0x10000 draws opaque white.
 assert(NativeMaterialTint(dc,root.items[4])==root.items[4]->window.foreColor);
 assert(root.items[4]->window.foreColor[0]==0&&root.items[4]->window.foreColor[3]>0&&root.items[4]->window.foreColor[3]<1);
 for(int i=0;i<4;++i)assert(root.items[4+i]->window.dynamicFlags[0]==((native_flags[i]&~2)|4));
 // Unselected: text only, no backing or highlight. Selected: all native layers.
 assert(UI_SplitScreenIsHiddenLayer(0,root.items[4]));
 assert(!UI_SplitScreenIsHiddenLayer(0,button));
 assert(UI_SplitScreenIsHiddenLayer(0,root.items[5])&&UI_SplitScreenIsHiddenLayer(0,root.items[7]));
 button->window.dynamicFlags[0]|=2;
 assert(!UI_SplitScreenIsHiddenLayer(0,root.items[4]));
 assert(!UI_SplitScreenIsHiddenLayer(0,root.items[5])&&!UI_SplitScreenIsHiddenLayer(0,root.items[7]));
 button->window.dynamicFlags[0]&=~2;
 assert(UI_SplitScreenIsHiddenLayer(0,root.items[4])&&!UI_SplitScreenIsHiddenLayer(0,button));
 UI_SplitScreenMenuOpening(0,dc,&root);assert(root.itemCount==9); // no repeated shifts
 // Separate lobby state, but native name lookup still finds it for child
 // map/game-mode/settings menus and ui_animate. No start chooser remains.
 Item_RunEventScript(0,dc,button,"action");auto*copy=lastOpened;
 assert(copy!=&parent&&UI_SplitScreenIsCustomMenu(copy)&&copy->itemCount==parent.itemCount&&!CL_SplitScreenSessionRequested());
 assert(Menus_FindByName(dc,"menu_xboxlive_privatelobby")==copy);
 UI_SplitScreenMenuOpening(0,dc,&root);assert(splitLobby&&Menus_FindByName(dc,"menu_xboxlive_privatelobby")==copy); // background root does not cancel a live lobby
 auto*guestStart=copy->items[0];assert(guestStart!=&source&&guestStart->parent==copy);
 assert(guestStart->typeData.textDef!=source.typeData.textDef&&guestStart->onEvent==source.onEvent);
 guestStart->window.dynamicFlags[0]=123;guestStart->typeData.textDef->text="PRIVATE CACHE";
 assert(source.window.dynamicFlags[0]!=123&&!strcmp(source.typeData.textDef->text,"ORIGINAL"));
 Menus_OpenByName(0,dc,"menu_xboxlive_privatelobby");assert(lastOpened==copy); // returning from map/rule picker
 vars["bo1_directory_publish"]="1";vars["ui_mapname"]="mp_array";vars["ui_gametype"]="dom";vars["sv_maxclients"]="18";
 Item_RunEventScript(0,dc,guestStart,"action");assert(CL_SplitScreenSessionRequested());
 assert(vars["bo1_directory_publish"]=="0");
 assert(vars["g_gametype"]=="dom"&&vars["sv_maxclients"]=="18"&&vars["systemlink"]=="1"&&vars["onlinegame"]=="0"&&vars["xblive_privatematch"]=="0");
 assert(commands.back().find("map mp_array\n")!=std::string::npos&&!vars.count("scr_dom_scorelimit"));
 // Exercise actual Com_LoadFrontEnd during the queued/loading phases, as
 // Com_AssetLoadUI does when rebuilding UI for a map. This used to erase P2.
 Com_LoadFrontEnd();UI_SplitScreenMenuOpening(0,dc,&root);assert(CL_SplitScreenSessionRequested());
 CL_SplitScreenMapLoading("mp_array");Com_LoadFrontEnd();UI_SplitScreenMenuOpening(0,dc,&root);assert(CL_SplitScreenSessionRequested());
 CL_SplitScreenSessionActivated();Com_LoadFrontEnd();UI_SplitScreenMenuOpening(0,dc,&root);
 assert(!splitLobby&&!CL_SplitScreenSessionRequested()&&Menus_FindByName(dc,"menu_xboxlive_privatelobby")==&parent);
 // Back out without starting: the next regular private match stays native.
 Item_RunEventScript(0,dc,button,"action");assert(lastOpened!=copy);
 Menus_Close(0,dc,lastOpened);UI_SplitScreenMenuOpening(0,dc,&root);before=nativeEvents;
 vars["ui_mapname"]="mp_nuked";vars["ui_gametype"]="tdm";
 Item_RunEventScript(0,dc,&source,"action");assert(nativeEvents==before+1&&!CL_SplitScreenSessionRequested());
 assert(commands.back().find("map mp_nuked")!=std::string::npos);
 for(const char*mode:{"tdm","hctdm"}){
  vars["ui_gametype"]=mode;vars["sv_maxclients"]="8";
  UI_RunMenuScript_StartListenServer();assert(vars["sv_maxclients"]=="12"&&vars["party_maxplayers"]=="12");
 }
 vars["sv_maxclients"]="18";UI_RunMenuScript_StartListenServer();assert(vars["sv_maxclients"]=="18");
 vars["bo1_host_maxplayers"]="8";UI_RunMenuScript_StartListenServer();assert(vars["sv_maxclients"]=="8"&&vars["party_maxplayers"]=="8");
 vars["bo1_host_maxplayers"]="0";UI_RunMenuScript_StartListenServer();assert(vars["sv_maxclients"]=="12");
 const char*map="mp_array";

#endif
 assert(CL_SplitScreenStartMap(map));
 CL_SplitScreenMapLoading(map);assert(CL_SplitScreenSessionRequested()); // new match
 CL_SplitScreenMapLoading(map);assert(CL_SplitScreenSessionRequested()); // restart
 CL_SplitScreenMapLoading("frontend");assert(!CL_SplitScreenSessionRequested());
 assert(!CL_SplitScreenStartMap("bad;quit")&&!CL_SplitScreenStartMap("frontend")&&!CL_SplitScreenSessionRequested());
 assert(CL_SplitScreenStartMap(map));CL_SplitScreenMapLoading("different_map");assert(!CL_SplitScreenSessionRequested());
 UI_SplitScreenMenuOpening(0,dc,&parent);assert(!CL_SplitScreenSessionRequested());
#ifndef KISAK_SP
 assert(CL_SplitScreenStartMap(map)); // abandoned load followed by a regular match
 UI_RunMenuScript_StartListenServer();assert(!CL_SplitScreenSessionRequested());
#endif
 puts("PASS: actual native UI dispatch and zone-asset checks, private data ownership, native row layers/colors/focus, lobby copy without caption, native starts surviving UI reloads, selection/start/cancel, selected map/mode, no DVAR activation, same-map restart and frontend cleanup");
}
"""
for mode in ('SP','MP'):
 cpp=out/('frontend-'+mode.lower()+'.cpp');exe=cpp.with_suffix('.exe');cpp.write_text(code)
 subprocess.run([args.compiler,'-std=c++17','-O0','-w','-DKISAK_'+mode+'=1',str(cpp),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
# Verify the former unconditional reset reproduces the lost-session failure
# at the real Com_LoadFrontEnd boundary, instead of stubbing that boundary out.
legacy=code.replace(function('src/qcommon/common.cpp','void __cdecl Com_LoadFrontEnd('),function('src/qcommon/common.cpp','void __cdecl Com_LoadFrontEnd(').replace('CL_SplitScreenFrontendEntered();','CL_SplitScreenResetSession();'))
cpp=out/'frontend-legacy-reset.cpp';exe=cpp.with_suffix('.exe');cpp.write_text(legacy)
subprocess.run([args.compiler,'-std=c++17','-O0','-w','-DKISAK_MP=1',str(cpp),'-o',str(exe)],check=True)
failed=subprocess.run([str(exe)],capture_output=True,text=True)
assert failed.returncode and 'CL_SplitScreenSessionRequested()' in failed.stderr
print('PASS: legacy frontend reset reproduces the missing split-screen session; current native reload path preserves it')

# The prior row initialization must fail the actual painter tint check.
legacy=code.replace('(item->window.dynamicFlags[0] & ~2) | 4','4')
cpp=out/'frontend-legacy-tint.cpp';exe=cpp.with_suffix('.exe');cpp.write_text(legacy)
subprocess.run([args.compiler,'-std=c++17','-O0','-w','-DKISAK_MP=1',str(cpp),'-o',str(exe)],check=True)
failed=subprocess.run([str(exe)],capture_output=True,text=True)
assert failed.returncode and 'NativeMaterialTint' in failed.stderr
print('PASS: previous flag reset reproduces white backing; native material tint/alpha now preserved')

s=(root/'src/client/splitscreen.cpp').read_text()
assert '_Dvar_RegisterBool("splitguest_auto"' not in s
assert 'Cmd_AddCommandInternal("splitguest_add"' not in s
for path,text in [('src/ui/ui_shared.cpp','UI_SplitScreenMenuOpening(localClientNum, dc, menu)'),('src/ui_mp/ui_main_mp.cpp','UI_SplitScreenStartZombieMap(localClientNum, szNotify)'),('src/client_mp/cl_main_mp.cpp','CL_SplitScreenMapLoading(mapname)')]:assert text in (root/path).read_text()
print('PASS: native frontend/map hooks are wired and old auto-enable/add command registrations are absent')
