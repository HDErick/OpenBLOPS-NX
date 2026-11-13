"""Verify native Switch hang capture resumes the main thread before logging."""
from pathlib import Path
import argparse,subprocess
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--compiler',default='/c/msys64/ucrt64/bin/g++.exe');args=p.parse_args()
root=Path(__file__).resolve().parents[3];out=root/'build-nx/stall-capture-tests';out.mkdir(parents=True,exist_ok=True)
def function(path,sig):
 s=(root/path).read_text();a=s.index(sig);b=s.index('{',a)+1;depth=1
 while depth:depth+=(s[b]=='{')-(s[b]=='}');b+=1
 return s[a:b]
code=r"""
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
using u64=uint64_t;using u32=uint32_t;using Handle=uint32_t;using Result=uint32_t;
#define R_FAILED(x) ((x)!=0)
#define R_SUCCEEDED(x) ((x)==0)
constexpr int ThreadActivity_Paused=1,ThreadActivity_Runnable=0,Perm_R=1;
struct ThreadContext{u64 sp=0,lr=0;struct{u64 x=0;}pc;};
struct MemoryInfo{u64 addr=0,size=0;unsigned perm=0;};
static Handle s_stallMainHandle=88;
static bool hinted=true,paused=false;static Result pauseError=0,captureError=0;static int pauses=0,resumes=0;
static u64 stackWords[4],textBase;static std::string output;
bool envIsSyscallHinted(int){return hinted;}
Result svcSetThreadActivity(Handle h,int activity){assert(h==88);
 if(activity==ThreadActivity_Paused){++pauses;if(pauseError)return pauseError;assert(!paused);paused=true;}
 else {assert(paused);paused=false;++resumes;}return 0;}
Result svcGetThreadContext3(ThreadContext*c,Handle){assert(paused);if(captureError)return captureError;c->sp=reinterpret_cast<u64>(stackWords);c->pc.x=textBase+0x100;c->lr=textBase+0x200;return 0;}
Result svcQueryMemory(MemoryInfo*m,u32*,u64 a){
 if(a==reinterpret_cast<u64>(stackWords)){assert(paused);m->addr=a;m->size=sizeof(stackWords);m->perm=Perm_R;}
 else{assert(!paused);m->addr=textBase;m->size=0x10000;m->perm=Perm_R;}return 0;}
void nxStallWrite(const char*s,int n){assert(!paused);output.append(s,n);}
"""
code+=function('src/nx/nx_main.cpp','static void nxStallCaptureMain()')
code+=r"""
int main(){
 textBase=reinterpret_cast<u64>(&nxStallCaptureMain)&~0xfffull;
 stackWords[0]=textBase+0x300;stackWords[1]=7;
 nxStallCaptureMain();assert(!paused&&pauses==1&&resumes==1);
 assert(output.find("\n[nx-stall]")!=std::string::npos&&output.find("\\n")==std::string::npos);
 assert(output.find("main pc elf+0x100")!=std::string::npos&&output.find("main lr elf+0x200")!=std::string::npos&&output.find("stack candidate sp+0x0 elf+0x300")!=std::string::npos);
 output.clear();captureError=99;nxStallCaptureMain();assert(!paused&&resumes==2&&output.find("capture=0x63")!=std::string::npos);
 output.clear();pauseError=12;nxStallCaptureMain();assert(!paused&&resumes==2&&output.find("pause main failed")!=std::string::npos);
 output.clear();hinted=false;auto attempts=pauses;nxStallCaptureMain();assert(pauses==attempts&&!paused&&output.find("unavailable")!=std::string::npos);
 puts("PASS: actual hang capture snapshots PC/LR/stack, resumes before logging, and handles unavailable syscalls/pause/context errors");
}
"""
cpp=out/'capture.cpp';exe=cpp.with_suffix('.exe');cpp.write_text(code)
subprocess.run([args.compiler,'-std=c++17',str(cpp),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
