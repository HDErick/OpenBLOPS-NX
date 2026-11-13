"""Build/run native directory regressions; optional public UDP smoke test, never the game."""
from pathlib import Path
import argparse, os, subprocess
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--compiler',default='C:/msys64/ucrt64/bin/g++.exe')
p.add_argument('--server',help='Optional authorized public directory IPv4:port')
p.add_argument('--mode', choices=['mp','zm'], default='mp', help='Mode for the optional live smoke test')
p.add_argument('--fallback', action='store_true', help='Leave direct connection incomplete to verify automatic relay fallback')
a=p.parse_args()
r=Path(__file__).resolve().parents[3]
out=r/'build-nx/directory-tests'
out.mkdir(parents=True,exist_ok=True)
env=os.environ.copy();env['PATH']=str(Path(a.compiler).parent)+os.pathsep+env.get('PATH','')
common=[a.compiler,'-std=c++20','-Wall','-Wextra','-Werror','-static-libstdc++','-static-libgcc','-I'+str(r/'src'),str(r/'src/openblops_directory/directory_protocol.cpp'),str(r/'src/openblops_directory/directory_client.cpp')]
for name,arguments in [('directory_client_test',[])]+([('directory_wire_test',[a.server,a.mode]+(['fallback'] if a.fallback else []))] if a.server else []):
    exe=out/(name+'.exe')
    subprocess.run(common+[str(r/'tools/nx/tests'/f'{name}.cpp'),'-o',str(exe)]+(['-lws2_32'] if name.endswith('wire_test') else []),check=True,env=env)
    subprocess.run([str(exe),*arguments],check=True,env=env,timeout=65)
    if name == 'directory_client_test':
        subprocess.run([str(exe),'zm'],check=True,env=env,timeout=65)

# Compile the actual engine publication predicate and split-screen lifecycle.
# The client test above checks authenticated withdrawal when SetHost becomes false.
def engine_function(path, signature):
    source=(r/path).read_text();start=source.index(signature);end=source.index('{',start)+1;depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]

import re
engine=(r/'src/openblops_directory/directory_engine.cpp').read_text()
condition=re.search(r'bool hosting = (.*?);',engine,re.S).group(1)
policy=r'''
#include <cassert>
#include <cstring>
#include <string>
#include <map>
#include <cstdarg>
#include <cstdio>
#define KISAK_MP 1
std::map<std::string,std::string> vars;
bool Dvar_GetBool(const char*n){return vars[n]=="1";}
int Dvar_GetInt(const char*n){return std::atoi(vars[n].c_str());}
const char*Dvar_GetString(const char*n){return vars[n].c_str();}
void Dvar_SetBoolByName(const char*n,bool v){vars[n]=v?"1":"0";}
void Dvar_SetIntByName(const char*n,int v){vars[n]=std::to_string(v);}
void Dvar_SetStringByName(const char*n,const char*v){vars[n]=v;}
void CL_SplitGuestLog(const char*){}
void Cbuf_AddText(int,const char*){}
const char*va(const char*fmt,...){static char b[256];va_list a;va_start(a,fmt);vsnprintf(b,sizeof(b),fmt,a);va_end(a);return b;}
bool guest=false,menu=false;
bool CL_IsSplitGuest(int n){assert(n==1);return guest;}
bool Com_IsMenuLevel(const char*){return menu;}
struct Dvar{struct{bool enabled=true;}current;};Dvar pub,running;
Dvar*publish=&pub,*com_sv_running=&running;
enum{SS_GAME=2};struct{int state=SS_GAME;}sv;struct{bool clients=true;}svs;
'''
policy+=re.sub(r'^#include[^\n]*\n','',(r/'src/client/splitscreen_session.cpp').read_text(),flags=re.M)
policy+=engine_function('src/openblops_directory/directory_engine.cpp','static bool Directory_IsSplitScreenHost(')
policy+=engine_function('src/openblops_directory/directory_engine.cpp','static bool Directory_IsCompatibleHostMap(')
policy+='\nbool Hosting(){bool localSplitScreen=Directory_IsSplitScreenHost();return '+condition+';}\n'
policy+=r'''
int main(){
 assert(Hosting());menu=true;assert(!Hosting());menu=false;
 running.current.enabled=false;assert(!Hosting());running.current.enabled=true;
 svs.clients=false;assert(!Hosting());svs.clients=true;
 vars["bo1_directory_publish"]="1";
 assert(!CL_SplitScreenStartMap("bad;quit")&&vars["bo1_directory_publish"]=="1"&&Hosting());
#ifdef KISAK_EXPERIMENTAL_SPLITSCREEN
 assert(CL_SplitScreenStartMap("mp_array"));
 assert(vars["bo1_directory_publish"]=="0"&&!Hosting());
 // Re-enabling public advertising cannot bypass queued/loading/active policy.
 vars["bo1_directory_publish"]="1";publish->current.enabled=true;assert(!Hosting());
 CL_SplitScreenMapLoading("mp_array");assert(!Hosting());
 CL_SplitScreenSessionActivated();assert(!Hosting());
 CL_SplitScreenResetSession();guest=true;assert(!Hosting());guest=false;assert(Hosting());
#endif
 vars["splitscreen"]="1";assert(!Hosting());vars["splitscreen"]="0";assert(Hosting());
 publish->current.enabled=false;assert(!Hosting());
 puts("PASS: actual directory publication gate blocks local split-screen before/during gameplay, including publish re-enabling; ordinary hosts preserved");
}
'''
for split in (False,True):
    cpp=out/('publication-split.cpp' if split else 'publication-normal.cpp');exe=cpp.with_suffix('.exe');cpp.write_text(policy)
    subprocess.run([a.compiler,'-std=c++17','-Wall','-Wextra','-Werror',*(['-DKISAK_EXPERIMENTAL_SPLITSCREEN=1'] if split else []),str(cpp),'-o',str(exe)],check=True,env=env)
    subprocess.run([str(exe)],check=True,env=env)
