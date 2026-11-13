"""Reproduce guest cursor-hint binding lookups against the native one-profile guard."""
from pathlib import Path
import argparse,subprocess
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--compiler',default='/c/msys64/ucrt64/bin/g++.exe');args=p.parse_args()
root=Path(__file__).resolve().parents[3];out=root/'build-nx/splitscreen-profile-tests';out.mkdir(parents=True,exist_ok=True)
def function(path,sig):
 s=(root/path).read_text();a=s.index(sig);b=s.index('{',a)+1;depth=1
 while depth:depth+=(s[b]=='{')-(s[b]=='}');b+=1
 return s[a:b]
code=r"""
#include <cassert>
#include <cstring>
#include <cstdio>
#include <cstdarg>
#define __cdecl
#define KISAK_EXPERIMENTAL_SPLITSCREEN 1
#define __debugbreak() assert(false)
template<class... T>bool Assert_MyHandler(T...){return false;}
struct GamerSettingState{const char *gpadButtonsConfig="buttons_default";}gamerSettings[1];
struct Dvar{struct{bool enabled=false;}current;}pad,zombie;auto gpad_enabled=&pad;auto zombietron=&zombie;
bool CL_IsSplitGuest(int n){return n==1;}
const char *Dvar_GetString(const char *){return "buttons_default";}
int I_strcmp(const char*a,const char*b){return strcmp(a,b);}
void I_strncpyz(char*out,const char*in,int n){snprintf(out,n,"%s",in);}
const char *SEH_StringEd_GetString(const char*s){return s;}
char *UI_SafeTranslateString(const char*s){return const_cast<char*>(s);}
void Com_sprintf(char*out,int n,const char*fmt,...){va_list a;va_start(a,fmt);vsnprintf(out,n,fmt,a);va_end(a);}
int CL_GetGamePadBinding(int n,const char*command,char binds[2][128],bool){assert(n==0);strcpy(binds[0],!strcmp(command,"+frag")?"RB":"X");return 1;}
int CL_GetKeyBinding(int n,const char*,char binds[2][128],bool){assert(n==0);strcpy(binds[0],"F");return 1;}
"""
code+=function('src/win32/win_gamerprofile.cpp','GamerSettingState *__cdecl GamerProfile_GetProfileSettings(')+'\n'
code+=function('src/ui/ui_shared.cpp','void __cdecl GetPlatformCommand(')+'\n'
code+=function('src/ui/ui_shared.cpp','int __cdecl UI_GetKeyBindingLocalizedString(')+'\n'
code+=r"""
int main(){char bind[256];pad.current.enabled=true;
 // This grenade/throwback hint was the recorded crash, unlike +activate which
 // takes an early path that never reads the saved profile.
 assert(UI_GetKeyBindingLocalizedString(1,"+frag",bind,0)==1&&!strcmp(bind,"RB"));
 assert(UI_GetKeyBindingLocalizedString(1,"+activate",bind,0)==1&&!strcmp(bind,"X"));
 const char *command;GetPlatformCommand(1,"+speed_throw",&command);assert(!strcmp(command,"+speed_throw"));
 pad.current.enabled=false;assert(UI_GetKeyBindingLocalizedString(1,"+frag",bind,0)==1&&!strcmp(bind,"F"));
 puts("PASS: native grenade/use hint binding path uses profile zero, including direct platform translation and keyboard fallback");}
"""
cpp=out/'bindings.cpp';exe=out/'bindings.exe';cpp.write_text(code)
subprocess.run([args.compiler,'-std=c++17',str(cpp),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
