"""Check the native graphics row with real zone layouts, without launching the game."""
from pathlib import Path
import argparse, ast, json, os, re, struct, subprocess
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--compiler', default='C:/msys64/ucrt64/bin/g++.exe')
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
reader=ast.get_source_segment(s,f).replace('name=string(ptr(item)),type=',
 'dvar=string(ptr(item,192)),text=string(ptr(ptr(item,224),56)) if num(item,176) in (1,3,4,5,7,8,9,10,11,13,15,16,18,19,20,21) else "",rpn=num(item,260),name=string(ptr(item)),type=')
exec(reader)
code=r'''
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#define __cdecl
#define ARRAY_COUNT(x) (sizeof(x)/sizeof(x[0]))
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
'''
code+=function('src/ui/ui_utils.cpp','int __cdecl Item_IsTextDefType(')+'\n'
for sig in ['static itemDef_s *UI_NX_FindItem(', 'static bool UI_NX_ItemNamed(', 'static itemDef_s *UI_NX_FindItemByDvar(', 'static void UI_NX_MoveItem(', 'static itemDef_s *UI_NX_CloneItem(', 'bool UI_NX_IsHiddenAimAssistHighlight(', 'static void UI_AddPerformanceOverlayOption(']:
 code+=function('src/ui/ui_shared.cpp',sig)+'\n'
code+='int main(){\n'
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
 }
'''
code+='puts("PASS: actual SP, MP frontend and MP in-game graphics layouts, native Off/On binding, focus layers, footer clearance, unchanged shadow control and repeated opening");}\n'
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
