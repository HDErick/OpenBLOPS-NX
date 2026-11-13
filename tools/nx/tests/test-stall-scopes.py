"""Verify Switch hang reports retain stages with telemetry disabled."""
from pathlib import Path
import argparse,re,subprocess
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--compiler',default='/c/msys64/ucrt64/bin/g++.exe');args=p.parse_args()
root=Path(__file__).resolve().parents[3];out=root/'build-nx/stall-scope-tests';out.mkdir(parents=True,exist_ok=True)
def clean(s):return re.sub(r'^\s*#(?:include|pragma).*\n','',s,flags=re.M)
def function(path,sig):
 s=(root/path).read_text();a=s.index(sig);b=s.index('{',a)+1;depth=1
 while depth:depth+=(s[b]=='{')-(s[b]=='}');b+=1
 return s[a:b]
code=r"""
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <atomic>
#define KISAK_NX 1
static unsigned reads=0,gpuBegins=0,gpuEnds=0;
"""
profile=clean((root/'src/universal/profile.h').read_text())
profile=profile.replace(function('src/universal/profile.h','static inline uint64_t NxProf_Now()'),'static inline uint64_t NxProf_Now(){return ++reads;}')
code+=profile+'\n'+clean((root/'src/nx/nx_stall_scope.h').read_text())
code+=r"""
NxProfSite::NxProfSite(const char*n):name(n),next(nullptr),ticks{},calls{},gpuScope(-1){}
NxProfSite *volatile g_nxProfStack[NX_PROF_THREADS][NX_PROF_STACK]{};
volatile int g_nxProfDepth[NX_PROF_THREADS]{};
std::atomic<int> s_nextSlot{1};char s_threadNames[NX_PROF_THREADS][32]{"main"};
bool enabled=false;int selectedSlot=0;
bool NxProf_Enabled(){return enabled;}
int NxProf_ThreadSlot(){return selectedSlot;}
void NxProf_GpuBegin(int){++gpuBegins;}
void NxProf_GpuEnd(int){++gpuEnds;}
"""
code+=function('src/nx/nx_prof.cpp','int NxProf_DescribeOpenScopes(')
code+=r"""
int main(){
 NxProfSite frame("Main frame"),present("Graphics present"),timed("timed");timed.gpuScope=0;
 char report[1024];
 {
  NxStallScope main(&frame);assert(g_nxProfDepth[0]==1);
  {NxProfZone disabled(&timed);assert(g_nxProfDepth[0]==1&&reads==0&&gpuBegins==0);}
  {NxStallScope child(&present);NxProf_DescribeOpenScopes(report,sizeof(report));
   assert(strstr(report,"main")&&strstr(report,"Main frame > Graphics present")&&g_nxProfDepth[0]==2);}
  assert(g_nxProfDepth[0]==1&&reads==0&&gpuBegins==0);
  enabled=true;
  {NxProfZone active(&timed);assert(g_nxProfDepth[0]==2&&gpuBegins==1);
   {NxStallScope child(&present);assert(g_nxProfDepth[0]==3);}
   enabled=false;}
  assert(g_nxProfDepth[0]==1&&timed.calls[0]==1&&gpuEnds==1);
 }
 assert(g_nxProfDepth[0]==0);
 // Bounded reporting and correct unwinding beyond the stored stack depth.
 g_nxProfDepth[0]=NX_PROF_STACK;
 {NxStallScope overflow(&present);assert(g_nxProfDepth[0]==NX_PROF_STACK+1);NxProf_DescribeOpenScopes(report,80);}
 assert(g_nxProfDepth[0]==NX_PROF_STACK);g_nxProfDepth[0]=0;
 selectedSlot=-1;{NxStallScope ignored(&frame);}assert(g_nxProfDepth[0]==0);
 puts("PASS: real stall report retains nested stages with telemetry off, no timer/GPU work, mixed timed scopes, bounded reporting and cleanup");
}
"""
cpp=out/'scopes.cpp';exe=cpp.with_suffix('.exe');cpp.write_text(code)
subprocess.run([args.compiler,'-std=c++17','-w',str(cpp),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
