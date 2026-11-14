"""Check the gamepad AIM ASSIST / AIM ACCELERATION rows and the controller diagram fit with real zone layouts."""
from pathlib import Path
import argparse, ast, json, os, struct, subprocess
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--compiler', default='C:/msys64/ucrt64/bin/g++.exe')
p.add_argument('--zone-dir', type=Path, default=Path('F:/pluto_t5_full_game/kbz'))
a=p.parse_args();root=Path(__file__).resolve().parents[3]
out=root/'build-nx/gamepad-menu-tests';out.mkdir(parents=True,exist_ok=True)
env=os.environ.copy();env['PATH']=str(Path(a.compiler).parent)+os.pathsep+env.get('PATH','')
def function(path,sig):
 s=(root/path).read_text();start=s.index(sig);begin=s.index('{',start)
 if ';' in s[start:begin]:start=s.index(sig,begin);begin=s.index('{',start)   # skip forward declarations
 end=begin+1;depth=1
 while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
 return s[start:end]
# Reuse the zone relocation reader, adding each text item's alignment offsets and scale (textDef_s +40).
s=(root/'tools/nx/tests/test-splitscreen-frontend.py').read_text()
f=next(n for n in ast.parse(s).body if isinstance(n,ast.FunctionDef) and n.name=='menu_items')
TEXT='(1,3,4,5,7,8,9,10,11,13,15,16,18,19,20,21)'
reader=ast.get_source_segment(s,f).replace("Path('F:/pluto_t5_full_game/kbz')",'a.zone_dir').replace('name=string(ptr(item)),type=',
 f'dvar=string(ptr(item,192)),text=string(ptr(ptr(item,224),56)) if num(item,176) in {TEXT} else "",'
 f'tdef=struct.unpack_from("<3f",blocks[ptr(item,224)[0]],ptr(item,224)[1]+40) if num(item,176) in {TEXT} and ptr(item,224) else (0.0,0.0,0.0),'
 'rpn=num(item,260),name=string(ptr(item)),type=')
exec(reader)
def extent(r):
 x,y,w,h=r['rect'][:4];return min(x,x+w),max(x,x+w),min(y,y+h),max(y,y+h)
code=r'''
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#define __cdecl
#define ARRAY_COUNT(x) (sizeof(x)/sizeof(x[0]))
int I_stricmp(const char*a,const char*b){return strcmp(a,b);}
struct rectDef_s{float x=0,y=0,w=0,h=0;int horzAlign=0,vertAlign=0;};
struct textDef_s{rectDef_s textRect[1];float textalignx=0,textaligny=0,textscale=0;const char*text=nullptr;void*textExpData=nullptr;};
struct GenericEventHandler{const char*name=nullptr;GenericEventHandler*next=nullptr;};
struct AnimState{rectDef_s rectClient;};struct UIAnimInfo{int animStateCount=0;AnimState**animStates=nullptr;AnimState currentAnimState,nextAnimState;};
struct menuDef_t;
struct itemDef_s{int type=0,dataType=0;struct{const char*name=nullptr;rectDef_s rect,rectClient;}window;const char*dvar=nullptr;
 struct{int numRpn=0;}visibleExp;struct{textDef_s*textDef=nullptr;}typeData;void*rectExpData=nullptr;
 GenericEventHandler*onEvent=nullptr;UIAnimInfo*animInfo=nullptr;menuDef_t*parent=nullptr;};
struct menuDef_t{struct{const char*name="options_gamepad";}window;int itemCount=0;itemDef_s**items=nullptr;};
bool Window_HasFocus(int,const decltype(itemDef_s::window)*){return false;}
struct Box{float x0=1e30f,x1=-1e30f,y0=1e30f,y1=-1e30f;int count=0;};
Box diagram(const menuDef_t&menu){Box b;for(int i=0;i<menu.itemCount;++i){const auto&r=menu.items[i]->window.rect;if(r.vertAlign!=1)continue;
 b.x0=fminf(b.x0,fminf(r.x,r.x+r.w));b.x1=fmaxf(b.x1,fmaxf(r.x,r.x+r.w));b.y0=fminf(b.y0,fminf(r.y,r.y+r.h));b.y1=fmaxf(b.y1,fmaxf(r.y,r.y+r.h));++b.count;}return b;}
bool near(float a,float b,float e=.01f){return fabsf(a-b)<=e;}
'''
code+=function('src/ui/ui_utils.cpp','int __cdecl Item_IsTextDefType(')+'\n'
for sig in ['static itemDef_s *UI_NX_FindItem(', 'static bool UI_NX_ItemNamed(', 'static itemDef_s *UI_NX_FindItemByDvar(',
            'static void UI_NX_MoveItem(', 'static itemDef_s *UI_NX_CloneItem(', 'bool UI_NX_IsHiddenAimAssistHighlight(',
            'static int UI_NX_CloneInvertRow(', 'static void UI_NX_ScaleRectAbout(', 'static void UI_NX_ScaleItem(',
            'static void UI_NX_FitGamepadDiagram(', 'static void UI_NX_AddAimAssistOption(']:
 code+=function('src/ui/ui_shared.cpp',sig)+'\n'
code+='int main(){\n'
for zone in ('code_post_gfx','ui_mp','common_mp'):
 rows=menu_items(zone,'options_gamepad')
 if rows is None:raise RuntimeError('Supply local '+zone+'.kbz (--zone-dir) to check its actual gamepad menu')
 n=len(rows)
 by={r['dvar']:r for r in rows if r['dvar']}
 sens,buttons,invert=by['input_viewSensitivity'],by['gpad_buttonsConfig'],by['input_invertpitch']
 step=sens['rect'][1]-buttons['rect'][1]
 boxes=[extent(r) for r in rows if r['rect'][5]==1]
 x0,x1,y0,y1=min(b[0] for b in boxes),max(b[1] for b in boxes),min(b[2] for b in boxes),max(b[3] for b in boxes)
 # The inner backing panel: the smallest centre-aligned background around the diagram (top-aligned, so y-240 here).
 panel=min((r for r in rows if r['type']==0 and r['rect'][5]==2 and r['rect'][2]>0 and r['rect'][0]<=x0
            and r['rect'][0]+r['rect'][2]>=x1 and r['rect'][1]<=y0-240 and r['rect'][1]+r['rect'][3]>=y1-240),
           key=lambda r:r['rect'][2]*r['rect'][3])
 labels=[i for i,r in enumerate(rows) if r['rect'][5]==1 and r['text']]
 code+=f' {{ menuDef_t menu;itemDef_s native[{n}];textDef_s texts[{n}];itemDef_s*table[{n}];rectDef_s before[{n}];GenericEventHandler leave;leave.name="leavefocus";\n'
 for i,row in enumerate(rows):
  code+=f' table[{i}]=&native[{i}];native[{i}].parent=&menu;native[{i}].type=native[{i}].dataType={row["type"]};native[{i}].visibleExp.numRpn={row["rpn"]};\n'
  for field,value in [('window.name',row['name']),('dvar',row['dvar'])]:
   if value:code+=f' native[{i}].{field}={json.dumps(value)};\n'
  code+=f' native[{i}].window.rect=native[{i}].window.rectClient=before[{i}]='+'{'+','.join(map(str,row['rect']))+'};\n'
  code+=(f' if(Item_IsTextDefType(&native[{i}])){{native[{i}].typeData.textDef=&texts[{i}];texts[{i}].text={json.dumps(row["text"])};'
         f'texts[{i}].textalignx={row["tdef"][0]}f;texts[{i}].textaligny={row["tdef"][1]}f;texts[{i}].textscale={row["tdef"][2]}f;}}\n')
  if row['dvar']=='input_invertpitch' and row['name']:code+=f' native[{i}].onEvent=&leave;\n'
 code+=f' menu.items=table;menu.itemCount={n};const Box old=diagram(menu);assert(old.count==113);\n'
 code+=f' const float labelScale={rows[labels[0]]["tdef"][2]}f,labelWidth={rows[labels[0]]["rect"][2]}f;\n'
 code+=r'''
 UI_NX_AddAimAssistOption(&menu);
 auto*assist=UI_NX_FindItem(&menu,"options_gamepad_nx_aimassist",nullptr);auto*accel=UI_NX_FindItem(&menu,"options_gamepad_nx_aimaccel",nullptr);
 assert(assist&&accel&&!strcmp(assist->dvar,"nx_aimassist")&&!strcmp(accel->dvar,"nx_aimacceleration"));
'''
 code+=f' assert(near(assist->window.rect.y,{sens["rect"][1]+step}f)&&near(accel->window.rect.y,{sens["rect"][1]+2*step}f));\n'
 code+=f' const float rowsBottom=240+{sens["rect"][1]+2*step+invert["rect"][3]}f;   // rows are centre-aligned; the diagram is top-aligned\n'
 code+=f' const float panelBottom=240+{panel["rect"][1]+panel["rect"][3]}f,panelLeft={panel["rect"][0]}f,panelRight={panel["rect"][0]+panel["rect"][2]}f;\n'
 code+=r'''
 const Box fit=diagram(menu);const float scale=(old.y1-fit.y0)/(old.y1-old.y0);
 assert(fit.count==113&&old.y0<rowsBottom);                          // the stock diagram did start under the new rows
 assert(fit.y0>=rowsBottom+4-.01f&&fit.y0<=rowsBottom+4.5f);          // now it starts just below them
 assert(near(fit.y1,old.y1)&&fit.y1<=panelBottom&&fit.x0>=panelLeft&&fit.x1<=panelRight);
 assert(scale>.7f&&scale<.8f&&near((fit.x0+fit.x1)/2,(old.x0+old.x1)/2));
 int scaledLabels=0;
 for(int i=0;i<int(ARRAY_COUNT(native));++i){
  const auto&r=native[i].window.rect;
  if(before[i].vertAlign!=1){assert(!memcmp(&r,&before[i],sizeof(r)));continue;}   // rows, panel and footer untouched
  assert(near(r.w,before[i].w*scale)&&near(r.h,before[i].h*scale)&&!memcmp(&r,&native[i].window.rectClient,sizeof(r)));
  if(native[i].typeData.textDef&&native[i].typeData.textDef->text&&*native[i].typeData.textDef->text){
   assert(near(native[i].typeData.textDef->textscale,labelScale*scale,1e-4f)&&near(r.w,labelWidth*scale));++scaledLabels;}
 }
 assert(scaledLabels==89);
 const int count=menu.itemCount;UI_NX_AddAimAssistOption(&menu);const Box again=diagram(menu);
 assert(menu.itemCount==count&&near(again.y0,fit.y0)&&near(again.y1,fit.y1));   // reopening the menu changes nothing
 printf("  %s: diagram y %.1f..%.1f -> %.1f..%.1f (scale %.3f), rows end at %.1f\n",
'''
 code+=f'  "{zone}",old.y0,old.y1,fit.y0,fit.y1,scale,rowsBottom);\n }}\n'
code+=' puts("PASS: real SP, MP frontend and MP in-game gamepad menus: both rows placed, controller diagram scaled clear of them inside the panel, labels scaled, other items untouched, reopening");}\n'
cpp=out/'gamepad.cpp';exe=cpp.with_suffix('.exe');cpp.write_text(code)
subprocess.run([a.compiler,'-std=c++17','-w',str(cpp),'-o',str(exe)],check=True,env=env)
subprocess.run([str(exe)],check=True,env=env)
