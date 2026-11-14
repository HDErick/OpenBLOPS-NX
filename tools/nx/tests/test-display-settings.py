"""Check the native graphics row with real zone layouts, without launching the game."""
from pathlib import Path
import argparse, ast, json, os, re, struct, subprocess
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--compiler', default='C:/msys64/ucrt64/bin/g++.exe')
p.add_argument('--zone-dir', type=Path, default=Path('F:/pluto_t5_full_game/kbz'))
a=p.parse_args();root=Path(__file__).resolve().parents[3]
out=root/'build-nx/display-settings-tests';out.mkdir(parents=True,exist_ok=True)
env=os.environ.copy();env['PATH']=str(Path(a.compiler).parent)+os.pathsep+env.get('PATH','')
def function(path,sig):
 s=(root/path).read_text();start=s.index(sig);begin=s.index('{',start)
 # Skip forward declarations.
 if ';' in s[start:begin]:start=s.index(sig,begin);begin=s.index('{',start)
 end=begin+1;depth=1
 while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
 return s[start:end]
# Reuse the zone relocation reader without running the frontend test program.
s=(root/'tools/nx/tests/test-splitscreen-frontend.py').read_text()
f=next(n for n in ast.parse(s).body if isinstance(n,ast.FunctionDef) and n.name=='menu_items')
reader=ast.get_source_segment(s,f).replace("Path('F:/pluto_t5_full_game/kbz')", 'a.zone_dir').replace('name=string(ptr(item)),type=',
 'dvar=string(ptr(item,192)),text=string(ptr(ptr(item,224),56)) if num(item,176) in (1,3,4,5,7,8,9,10,11,13,15,16,18,19,20,21) else "",rpn=num(item,260),name=string(ptr(item)),type=')
exec(reader)
code=r'''
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cstdarg>
#include <initializer_list>
#define __cdecl
#define __debugbreak() assert(false)
#define ARRAY_COUNT(x) (sizeof(x)/sizeof(x[0]))
#define KISAK_NX
int I_stricmp(const char*a,const char*b){return strcmp(a,b);}
struct rectDef_s{float x=0,y=0,w=0,h=0;int horzAlign=0,vertAlign=0;};
struct multiDef_s{const char*dvarList[32];const char*dvarStr[32];float dvarValue[32];int count,actionOnEnterPressOnly,strDef;};
struct focusItemDef_s{struct{multiDef_s*multi;}focusTypeData;};
struct textDef_s{rectDef_s textRect[1];const char*text=nullptr;void*textExpData=nullptr;struct{focusItemDef_s*focusItemDef=nullptr;}textTypeData;};
struct GenericEventHandler{const char*name=nullptr;GenericEventHandler*next=nullptr;};
struct AnimState{rectDef_s rectClient;};struct UIAnimInfo{int animStateCount=0;AnimState**animStates=nullptr;AnimState currentAnimState,nextAnimState;};
struct menuDef_t;
struct itemDef_s{int type=0,dataType=0;struct{const char*name=nullptr;rectDef_s rect,rectClient;int staticFlags=0,dynamicFlags[1]{};}window;
 const char*dvar=nullptr,*dvarTest=nullptr,*enableDvar=nullptr;int dvarFlags=0;struct{int numRpn=0;}visibleExp;
 unsigned long long showBits=0,hideBits=~0ULL;struct{textDef_s*textDef=nullptr;}typeData;
 void*rectExpData=nullptr;GenericEventHandler*onEvent=nullptr;UIAnimInfo*animInfo=nullptr;menuDef_t*parent=nullptr;};
struct menuDef_t{struct{const char*name="options_graphics";}window;int itemCount=0;itemDef_s**items=nullptr;};
const void*focused=nullptr;bool Window_HasFocus(int,const decltype(itemDef_s::window)*w){return focused==w;}
bool registered=false,setting=false;
void _Dvar_RegisterBool(const char*n,bool def,unsigned flags,const char*){
 assert(!strcmp(n,"cg_performanceOverlay")&&!def&&flags==1);if(!registered){registered=true;setting=def;}}
struct dvar_t{struct{int integer;}current;};
dvar_t baseLimit{{60}},savedLimit{{-1}};
const dvar_t *com_maxfps=&baseLimit;
const char *languageName="french";
int SEH_GetCurrentLanguage(){return 0;}
const char *SEH_GetLanguageName(int){return languageName;}
struct UiContext{int contextIndex=0,openMenuCount=1;};
bool Item_ShouldHandleKey(UiContext*,itemDef_s* item,int){return focused==&item->window;}
template<class... T>bool Assert_MyHandler(T...){return false;}
bool vsyncEnabled=false,stagedVsync=false;
int clicks=0,applyScripts=0;
bool Dvar_GetBool(const char*n){assert(!strcmp(n,"r_vsync"));return vsyncEnabled;}
const char *Dvar_GetVariantString(const char*n){
 return (!strcmp(n,"r_vsync")?vsyncEnabled:setting)?"1":"0";}
void Dvar_SetFromStringByName(const char*n,const char*v){
 if(!strcmp(n,"r_vsync")){vsyncEnabled=atoi(v)!=0;return;}
 if(!strcmp(n,"ui_r_vsync")){stagedVsync=atoi(v)!=0;return;}
 assert(!strcmp(n,"nx_fpsLimit"));savedLimit.current.integer=atoi(v);}
void UI_PlaySound(int,const char*n){assert(!strcmp(n,"uin_mouse_click_t"));++clicks;}
void Item_RunEventScript(int,UiContext*,itemDef_s*,const char*){++applyScripts;}
char *va(const char *fmt,...){
 static char value[32];va_list args;va_start(args,fmt);vsnprintf(value,sizeof(value),fmt,args);va_end(args);return value;}
multiDef_s *Item_GetMultiDef(itemDef_s* item){return item->typeData.textDef->textTypeData.focusItemDef->focusTypeData.multi;}
int Item_Multi_FindDvarByValue(itemDef_s*);
int Item_Multi_CountSettings(itemDef_s*);
int Item_List_NextEntryForKey(int,int,int);
'''
code+=function('src/ui/ui_utils.cpp','int __cdecl Item_IsTextDefType(')+'\n'
code+='const dvar_t *nx_fpsLimit=&savedLimit;\n'+function('src/qcommon/common.cpp','int Com_GetFrameRateLimit(')+'\n'
code+=function('src/ui/ui_shared.cpp','struct FrameRateOptionText')+';\n'
code+=function('src/ui/ui_shared.cpp','static const FrameRateOptionText &UI_NX_GetFrameRateOptionText(')+'\n'
for sig in ['static itemDef_s *UI_NX_FindItem(', 'static bool UI_NX_ItemNamed(', 'static itemDef_s *UI_NX_FindItemByDvar(', 'static void UI_NX_MoveItem(', 'static itemDef_s *UI_NX_CloneItem(', 'bool UI_NX_IsHiddenAimAssistHighlight(', 'static bool UI_AddGraphicsChoiceRow(', 'static void UI_AddPerformanceOverlayOption(', 'static void UI_NX_AddFrameRateOption(', 'static float Item_Multi_GetFloatValue(', 'int __cdecl Item_Multi_FindDvarByValue(', 'int __cdecl Item_Multi_CountSettings(', 'int __cdecl Item_List_NextEntryForKey(', 'int __cdecl Item_Multi_HandleKey(', 'const char *__cdecl Item_Multi_Setting(']:
 code+=function('src/ui/ui_shared.cpp',sig)+'\n'
code+=function('src/ui/ui_shared.cpp','void __cdecl Item_Action(')+'\n'
code+='int main(){\n'
source=(root/'src/stringed/stringed_hooks.cpp').read_text()
languages=re.findall(r'\{\s*"(\w+)",\s*"\w+_",\s*0\s*\}',source)
assert languages
for language in languages:
 code+=f' languageName="{language}";assert(!strcmp(UI_NX_GetFrameRateOptionText().language,languageName));\n'
code+=' languageName="unknown";assert(!strcmp(UI_NX_GetFrameRateOptionText().label,"FPS LIMIT"));\n'
for zone in ('code_post_gfx','ui_mp','common_mp'):
 rows=menu_items(zone,'options_graphics')
 if rows is None:raise RuntimeError('Supply local '+zone+'.kbz to check its actual graphics menu')
 code+=' { menuDef_t menu;itemDef_s native[%d];textDef_s texts[%d];itemDef_s*table[%d];GenericEventHandler leave;leave.name="leavefocus";\n'%(len(rows),len(rows),len(rows))
 for i,row in enumerate(rows):
  code+=f' table[{i}]=&native[{i}];native[{i}].parent=&menu;native[{i}].type=native[{i}].dataType={row["type"]};\n'
  for field,value in [('window.name',row['name']),('dvar',row['dvar'])]:
   if value:code+=f' native[{i}].{field}={json.dumps(value)};\n'
  code+=f' native[{i}].window.rect=native[{i}].window.rectClient='+'{'+','.join(map(str,row['rect']))+'};\n'
  code+=f' native[{i}].visibleExp.numRpn={row["rpn"]};native[{i}].window.staticFlags={row["flags"]};\n'
  code+=f' if(Item_IsTextDefType(&native[{i}])){{native[{i}].typeData.textDef=&texts[{i}];texts[{i}].text={json.dumps(row["text"])};}}\n'
  if row['dvar']=='sm_enable' and row['name']:code+=f' native[{i}].onEvent=&leave;\n'
 code+=f' menu.items=table;menu.itemCount={len(rows)};\n'
 language,label,unlimited={
    'code_post_gfx':('french','LIMITE FPS','SANS LIMITE'),
    'ui_mp':('german','FPS-LIMIT','UNBEGRENZT'),
    'common_mp':('japanese',r'FPS \x8F\xE3\x8C\xC0',r'\x96\xB3\x90\xA7\x8C\xC0'),
 }[zone]
 code+=f' languageName="{language}";const char *label="{label}", *unlimited="{unlimited}";\n'
 code+=r'''
 auto*source=UI_NX_FindItemByDvar(&menu,"sm_enable");auto*last=UI_NX_FindItemByDvar(&menu,"cg_fov_default");
 const auto oldRect=source->window.rect;const auto oldText=source->typeData.textDef;
 UI_AddPerformanceOverlayOption(&menu);
 assert(menu.itemCount==int(ARRAY_COUNT(native))+5);
 auto*choice=UI_NX_FindItem(&menu,"options_graphics_performance",nullptr);
 assert(choice&&choice->type==10&&choice->dataType==10&&!strcmp(choice->dvar,"cg_performanceOverlay"));
 assert(choice->window.rect.y==last->window.rect.y+19&&choice->window.rect.x==source->window.rect.x);
 assert(choice->parent==&menu&&choice->typeData.textDef!=oldText&&choice->onEvent&&choice->onEvent->name==leave.name);
 auto*multi=choice->typeData.textDef->textTypeData.focusItemDef->focusTypeData.multi;
 assert(multi&&multi->count==2&&!multi->strDef&&multi->dvarValue[0]==0&&multi->dvarValue[1]==1);
 assert(!strcmp(multi->dvarList[0],"@MENU_OFF")&&!strcmp(multi->dvarList[1],"@MENU_ON"));
 int layers=0,highlights=0;
 for(int i=0;i<menu.itemCount;++i){auto*item=menu.items[i];
  if(item->window.name&&!strncmp(item->window.name,"options_graphics_performance",28)){
   ++layers;assert(item->visibleExp.numRpn==0&&!item->dvarFlags&&!item->showBits&&item->hideBits==~0ULL);
   if(item!=choice&&Item_IsTextDefType(item))assert(!strcmp(item->typeData.textDef->text,"PERFORMANCE OVERLAY"));
   if(!strcmp(item->window.name,"options_graphics_performance_hl")){
    ++highlights;assert(UI_NX_IsHiddenAimAssistHighlight(0,item));focused=&choice->window;
    assert(!UI_NX_IsHiddenAimAssistHighlight(0,item));focused=nullptr;
   }
  }
  if(Item_IsTextDefType(item)&&item->typeData.textDef){const char*t=item->typeData.textDef->text;
   if(t&&(!strcmp(t,"@MENU_APPLY")||!strcmp(t,"@PLATFORM_BACK")))
    assert(item->window.rect.y>=choice->window.rect.y+choice->window.rect.h+5&&item->window.rect.y+item->window.rect.h<=240);
  }
 }
 assert(layers==5&&highlights==2);
 assert(source->typeData.textDef==oldText&&!memcmp(&source->window.rect,&oldRect,sizeof(oldRect))&&!strcmp(source->dvar,"sm_enable"));
 int count=menu.itemCount;auto*items=menu.items;setting=true;UI_AddPerformanceOverlayOption(&menu);
 assert(setting&&count==menu.itemCount&&items==menu.items);

 auto*vsync=UI_NX_FindItemByDvar(&menu,"ui_r_vsync");
 auto*filter=UI_NX_FindItemByDvar(&menu,"r_texFilterMipMode");
 auto*texture=UI_NX_FindItemByDvar(&menu,"ui_r_picmip");
 const auto vsyncRect=vsync->window.rect, filterRect=filter->window.rect, textureRect=texture->window.rect;
 UI_NX_AddFrameRateOption(&menu);
 assert(menu.itemCount==count+5);
 assert(UI_NX_FindItemByDvar(&menu,"r_vsync")==vsync);
 multiDef_s vsyncValues={{"@MENU_NO_CAPS","@MENU_YES_CAPS"},{},{0,1},2,0,0};
 focusItemDef_s vsyncFocus{{&vsyncValues}};
 vsync->typeData.textDef->textTypeData.focusItemDef=&vsyncFocus;
 UiContext dc;
 focused=&vsync->window;
 for(bool enabled:{true,false}){
  const int previousClicks=clicks;
  assert(Item_Multi_HandleKey(&dc,vsync,23)&&vsyncEnabled==enabled);
  Item_Action(0,&dc,vsync);
  assert(stagedVsync==enabled&&clicks==previousClicks+1&&!applyScripts);
 }
 focused=nullptr;
 auto*fps=UI_NX_FindItem(&menu,"options_graphics_frame_limit",nullptr);
 assert(fps&&fps->type==10&&fps->onEvent&&fps->onEvent->name==leave.name);
 assert(fps->window.rect.y==vsyncRect.y+19&&filter->window.rect.y==fps->window.rect.y+19);
 assert(texture->window.rect.y-filter->window.rect.y==textureRect.y-filterRect.y);
 assert(!memcmp(&vsync->window.rect,&vsyncRect,sizeof(vsyncRect))&&texture->window.rect.y+texture->window.rect.h<source->window.rect.y);
 for(auto**it=menu.items;it!=menu.items+menu.itemCount;++it)
  if(*it==vsync)assert(it[5]==fps);
 auto*hl=UI_NX_FindItem(&menu,"options_graphics_frame_limit_hl",nullptr);
 assert(hl&&UI_NX_IsHiddenAimAssistHighlight(0,hl));
 int localizedLabels=0;
 for(int i=0;i<menu.itemCount;++i){
  const auto* item=menu.items[i];
  if((UI_NX_ItemNamed(item,"options_graphics_frame_limit_row")||UI_NX_ItemNamed(item,"options_graphics_frame_limit_hl"))&&Item_IsTextDefType(item)){
   assert(!strcmp(item->typeData.textDef->text,label));++localizedLabels;
  }
 }
 assert(localizedLabels==2);
 baseLimit.current.integer=60;savedLimit.current.integer=-1;
 assert(!strcmp(Item_Multi_Setting(fps),"60 FPS"));
 assert(!Item_Multi_HandleKey(&dc,fps,23));
 focused=&fps->window;
 assert(!UI_NX_IsHiddenAimAssistHighlight(0,hl));
 assert(Item_Multi_HandleKey(&dc,fps,23)&&Com_GetFrameRateLimit()==75);
 assert(Item_Multi_HandleKey(&dc,fps,22)&&Com_GetFrameRateLimit()==60);
 for(int i=0;i<5;++i) assert(Item_Multi_HandleKey(&dc,fps,23));
 assert(Com_GetFrameRateLimit()==0&&!strcmp(Item_Multi_Setting(fps),unlimited));
 assert(baseLimit.current.integer==60);
 assert(Item_Multi_HandleKey(&dc,fps,23)&&Com_GetFrameRateLimit()==30);
 savedLimit.current.integer=-1;baseLimit.current.integer=72;
 assert(!strcmp(Item_Multi_Setting(fps),"72 FPS"));
 focused=nullptr;items=menu.items;UI_NX_AddFrameRateOption(&menu);
 assert(menu.itemCount==count+5&&menu.items==items);
 }
'''
code+='puts("PASS: all FPS limit locales, SP/MP graphics layouts, live FPS presets, native controls and repeated opening");}\n'
cpp=out/'menu.cpp';exe=cpp.with_suffix('.exe');cpp.write_text(code)
subprocess.run([a.compiler,'-std=c++17','-w',str(cpp),'-o',str(exe)],check=True,env=env)
subprocess.run([str(exe)],check=True,env=env)
# Compile the actual drawing gate: old archived cg_drawFPS=1 must stay hidden;
# the new switch must work even when cg_drawFPS was previously zero.
code=r'''
#include <cassert>
#define __cdecl
struct meminfo_t{};struct ScreenPlacement{}scrPlaceFull;
struct Dvar{struct{bool enabled=false;int integer=0;float vector[2]{};}current;};
Dvar corner,fps,stat,anim,snap,time;
Dvar *cg_debugInfoCornerOffset=&corner,*cg_drawFPS=&fps,*com_statmon=&stat,*cg_drawAnimAttachTags=&anim,*cg_drawSnapshot=&snap,*cg_drawSnapshotTime=&time,*g_listEntityCounts=nullptr;
int g_listEntityCountsValue=0,draws=0;bool overlay=false;
void track_getbasicinfo(meminfo_t*){}void R_TrackStatistics(int){}
int Dvar_GetInt(const char*){return 0;}bool Dvar_GetBool(const char*){return overlay;}
float CG_DrawViewpos(ScreenPlacement*,float y,int){return y;}
float CG_DrawFPS(int,ScreenPlacement*,float y,meminfo_t*){++draws;return y;}
float CG_DrawStatmon(ScreenPlacement*,float y){return y;}float CG_DrawAnimTagInfo(ScreenPlacement*,float y){return y;}
float CG_DrawSnapshot(int,float y){return y;}float DrawSnapshotInfo(ScreenPlacement*,float y){return y;}
void DrawEntityCounts(ScreenPlacement*,float){}
'''+function('src/cgame/cg_draw_debug.cpp','void __cdecl CG_DrawUpperRightDebugInfo(')+r'''
int main(){fps.current.integer=1;CG_DrawUpperRightDebugInfo(0);assert(!draws);
overlay=true;CG_DrawUpperRightDebugInfo(0);assert(draws==1);fps.current.integer=0;CG_DrawUpperRightDebugInfo(0);assert(draws==2);
overlay=false;CG_DrawUpperRightDebugInfo(0);assert(draws==2);}
'''
cpp=out/'draw.cpp';exe=cpp.with_suffix('.exe');cpp.write_text(code)
subprocess.run([a.compiler,'-std=c++17','-w',str(cpp),'-o',str(exe)],check=True,env=env);subprocess.run([str(exe)],check=True,env=env)
assert 'CG_DrawVersion(' not in function('src/client_mp/cl_scrn_mp.cpp','void __cdecl CL_DrawScreen(')
assert 'return false;' in function('src/ui/ui_main.cpp','bool __cdecl UI_ShouldDrawBuildNumber(')
print('PASS: FPS/stats master toggle ignores legacy FPS setting; menu and gameplay build labels removed')
