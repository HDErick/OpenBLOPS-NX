"""Exercise the real gamepad developer-chord gate without a signed-in service."""
from pathlib import Path
import argparse, subprocess
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--compiler',default='/c/msys64/ucrt64/bin/g++.exe')
args=p.parse_args()
root=Path(__file__).resolve().parents[3]
s=(root/'src/client/cl_gamepad.cpp').read_text()
a=s.index('if (',s.index('Demo_HandleInput(localClientNum, controllerIndex, key, pressed);'))+len('if (')
b=s.index('\n                    {',a)
condition=s[a:b].rstrip()
assert condition.endswith(')')
condition=condition[:-1]
out=root/'build-nx/gamepad-developer-chord-tests'
out.mkdir(parents=True,exist_ok=True)
source=out/'chord.cpp'
source.write_text(r"""
#include <cassert>
#include <cstdio>
struct Key{bool down=false;} keys[32];
static bool menu, signedIn;
static int tier, tierCalls, signInCalls;
bool Menu_IsMenuOpenAndVisible(int,const char*){return menu;}
bool Live_IsSignedIn(int){++signInCalls;return signedIn;}
int Live_GetTier(int){assert(signedIn);++tierCalls;return tier;}
bool chord(int localClientNum,int controllerIndex){return (
"""+condition+r"""
);}
int main(){
 for(auto &key:keys)key.down=true;
 menu=false;signedIn=false;tier=1;
 assert(!chord(0,0)&&tierCalls==0&&signInCalls==0);
 menu=true;
 assert(!chord(0,0)&&tierCalls==0);
 signedIn=true;
#ifdef KISAK_NX
 assert(!chord(0,0)&&tierCalls==0&&signInCalls==0);
#else
 assert(chord(0,0)&&tierCalls==1);
 tier=0;assert(!chord(0,0)&&tierCalls==2);
#endif
 const int calls=tierCalls;
 assert(!chord(0,-1)&&tierCalls==calls);
 keys[16].down=false;
 assert(!chord(0,0)&&tierCalls==calls);
 puts("PASS: gamepad developer shortcut skips signed-out and gameplay tier queries");
}
""")
for platform,flags in [('pc',[]),('nx',['-DKISAK_NX'])]:
 exe=out/('chord-'+platform+'.exe')
 subprocess.run([args.compiler,'-std=c++17','-O2',*flags,str(source),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
