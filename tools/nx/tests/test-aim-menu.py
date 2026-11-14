"""Exercise the portable native gamepad aim-assist menu row and shared dvar binding."""
from pathlib import Path
import argparse,subprocess
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--compiler',default='/c/msys64/ucrt64/bin/g++.exe');args=p.parse_args()
root=Path(__file__).resolve().parents[3];out=root/'build-nx/aim-menu-tests';out.mkdir(parents=True,exist_ok=True)
def function(n,sig):
 s=(root/n).read_text();a=s.index(sig);b=s.index('{',a)+1;d=1
 while d:d+=(s[b]=='{')-(s[b]=='}');b+=1
 return s[a:b]
code=r"""
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define __cdecl
#define ARRAY_COUNT(a) (sizeof(a)/sizeof(a[0]))
int I_stricmp(const char*a,const char*b){return strcmp(a,b);}
struct rectDef_s{float x=0,y=0,w=100,h=20;int horzAlign=0,vertAlign=0;};struct textDef_s{const char*text=nullptr;void*textExpData=nullptr;rectDef_s textRect[1];float textalignx=0,textaligny=0,textscale=.3f;};
struct GenericEventHandler{const char*name=nullptr;GenericEventHandler*next=nullptr;};
struct UIAnimState{rectDef_s rectClient;};struct UIAnimInfo{int animStateCount=0;UIAnimState**animStates=nullptr;UIAnimState currentAnimState,nextAnimState;};
struct menuDef_t;
struct itemDef_s{int dataType=0;struct{const char*name=nullptr;rectDef_s rect,rectClient;}window;const char*dvar=nullptr;struct{int numRpn=1;}visibleExp;struct{textDef_s*textDef=nullptr;}typeData;void*rectExpData=nullptr;GenericEventHandler*onEvent=nullptr;UIAnimInfo*animInfo=nullptr;menuDef_t*parent=nullptr;};
struct menuDef_t{struct{const char*name="options_gamepad";}window;int itemCount=0;itemDef_s**items=nullptr;};
bool focused=false;bool Window_HasFocus(int,const decltype(itemDef_s::window)*){return focused;}
"""
code+=function('src/ui/ui_utils.cpp','int __cdecl Item_IsTextDefType(')+'\n'
for sig in ['static itemDef_s *UI_NX_FindItem(', 'static bool UI_NX_ItemNamed(', 'static itemDef_s *UI_NX_FindItemByDvar(', 'static void UI_NX_MoveItem(', 'static itemDef_s *UI_NX_CloneItem(', 'bool UI_NX_IsHiddenAimAssistHighlight(', 'static int UI_NX_CloneInvertRow(', 'static void UI_NX_ScaleRectAbout(', 'static void UI_NX_ScaleItem(', 'static void UI_NX_FitGamepadDiagram(', 'static void UI_NX_AddAimAssistOption(']:
 # The add function has an earlier forward declaration; select definition.
 s=(root/'src/ui/ui_shared.cpp').read_text()
 if sig.startswith('static void UI_NX_AddAimAssistOption'):
  s=s[s.index('static itemDef_s *UI_NX_FindItem('):]
  a=s.index(sig);b=s.index('{',a)+1;d=1
  while d:d+=(s[b]=='{')-(s[b]=='}');b+=1
  code+=s[a:b]+'\n'
 else:code+=function('src/ui/ui_shared.cpp',sig)+'\n'
code+=r"""
int main(){
 menuDef_t menu;itemDef_s label,choice,highlight,buttons,sensitivity;
 textDef_s labelText;labelText.text="LOOK INVERSION";label.dataType=1;label.typeData.textDef=&labelText;label.window.name="invert";
 choice.window.name="invert";choice.dvar="input_invertpitch";GenericEventHandler leave;leave.name="leavefocus";choice.onEvent=&leave;
 highlight.window.name="invert_hl";highlight.visibleExp.numRpn=2;
 buttons.dvar="gpad_buttonsConfig";buttons.window.rect.y=40;sensitivity.dvar="input_viewSensitivity";sensitivity.window.rect.y=70;
 itemDef_s*original[]{&label,&choice,&highlight,&buttons,&sensitivity};menu.items=original;menu.itemCount=5;
 UI_NX_AddAimAssistOption(&menu);assert(menu.itemCount==11);
 auto option=UI_NX_FindItem(&menu,"options_gamepad_nx_aimassist",nullptr);assert(option&&strcmp(option->dvar,"nx_aimassist")==0&&option->window.rect.y==100);
 assert(option->onEvent&&strcmp(option->onEvent->name,"leavefocus")==0);
 auto accel=UI_NX_FindItem(&menu,"options_gamepad_nx_aimaccel",nullptr);assert(accel&&strcmp(accel->dvar,"nx_aimacceleration")==0&&accel->window.rect.y==130);
 assert(accel->onEvent&&strcmp(accel->onEvent->name,"leavefocus")==0);
 assert(strcmp(choice.dvar,"input_invertpitch")==0&&strcmp(labelText.text,"LOOK INVERSION")==0);
 auto hl=UI_NX_FindItem(&menu,"options_gamepad_nx_aimassist_hl",nullptr);assert(hl&&UI_NX_IsHiddenAimAssistHighlight(0,hl));
 auto accelHl=UI_NX_FindItem(&menu,"options_gamepad_nx_aimaccel_hl",nullptr);assert(accelHl&&accelHl->window.rect.y==130&&UI_NX_IsHiddenAimAssistHighlight(0,accelHl));
 focused=true;assert(!UI_NX_IsHiddenAimAssistHighlight(0,hl)&&!UI_NX_IsHiddenAimAssistHighlight(0,accelHl));
 int assistLabels=0,accelLabels=0,assistIndex=-1,accelIndex=-1;
 for(int i=0;i<menu.itemCount;++i){
  auto*item=menu.items[i];if(item==option)assistIndex=i;if(item==accel)accelIndex=i;
  if(!item->typeData.textDef||item==&label||item==&choice)continue;
  if(item->typeData.textDef->text&&!strcmp(item->typeData.textDef->text,"AIM ASSIST"))++assistLabels,assert(item->window.rect.y==100);
  if(item->typeData.textDef->text&&!strcmp(item->typeData.textDef->text,"AIM ACCELERATION"))++accelLabels,assert(item->window.rect.y==130);
 }
 assert(assistLabels==1&&accelLabels==1&&assistIndex>=0&&accelIndex>assistIndex); // D-pad order: sensitivity, aim assist, acceleration
 UI_NX_AddAimAssistOption(&menu);assert(menu.itemCount==11);
 puts("PASS: gamepad Aim Assist and Aim Acceleration rows, dvar bindings, intact source controls, order and highlight/idempotency");
}
"""
cpp=out/'menu.cpp';cpp.write_text(code);exe=cpp.with_suffix('.exe');subprocess.run([args.compiler,'-std=c++17','-w',str(cpp),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
