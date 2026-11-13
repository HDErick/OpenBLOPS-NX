"""Run the real streaming query lifecycle for host and shared guest cameras."""
from pathlib import Path
import argparse
import subprocess

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler',default='/c/msys64/ucrt64/bin/g++.exe')
args=parser.parse_args()
root=Path(__file__).resolve().parents[3]
out=root/'build-nx/splitscreen-streaming-tests'
out.mkdir(parents=True,exist_ok=True)
def function(path,signature):
    source=(root/path).read_text(encoding='utf-8',errors='replace')
    start=source.index(signature);end=source.index('{',start)+1;depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]
code=r'''
#include <cassert>
#include <cstring>
#include <cstdio>
#include <vector>
#ifndef __cdecl
#define __cdecl
#endif
#define __int8 char
#define LODWORD(x) (*reinterpret_cast<unsigned int *>(&(x)))
#define PROF_SCOPED(x)
#define iassert(x) assert(x)
#define __debugbreak() assert(false)
struct Dvar {union {bool enabled;float value;int integer;} current;};
Dvar freeze{},check{},maxDist{},hidden{};
auto r_streamFreezeState=&freeze;auto r_streamCheckAabb=&check;
auto r_streamMaxDist=&maxDist;auto r_streamHiddenPush=&hidden;
bool streamIsInitialized=true,streamEnabled=true;
struct StreamFrontendGlob {
    bool queryInProgress=false,diskOrderImagesNeedSorting=false,diskOrder=false,forceDiskOrder=false;
    int queryClient=-1;unsigned frame=0;
    unsigned modelTouchBits[2]{};
    float forcedImageImportance=1,touchedImageImportance=1,initialImageImportance=1;
} streamFrontendGlob;
struct StreamUpdateCmd {StreamFrontendGlob *frontend;float viewPos[3],maxDistSq,distanceScale[2];};
struct StreamSortCmd {StreamFrontendGlob *frontend;bool diskOrder;};
struct StreamCombineCmd {StreamFrontendGlob *frontend;};
struct {float importance=0,origin[3]{};} s_streamHints[4];
struct {void *world=reinterpret_cast<void *>(1);} rgp;
struct Worker {int id;} r_stream_updateWorkerCmd{1},r_stream_combineWorkerCmd{2},r_stream_sortWorkerCmd{3};
std::vector<float> staticOrigins,dynamicOrigins;
int criticalDepth=0,begins=0,combines=0,sorts=0,ends=0,assertions=0;
constexpr int CRITSECT_STREAM_SYNC_COMMAND=1;
void Sys_EnterCriticalSection(int){assert(criticalDepth==0);++criticalDepth;}
void Sys_LeaveCriticalSection(int){assert(criticalDepth==1);--criticalDepth;}
void Sys_AddWorkerCmdInternal(Worker *worker,unsigned char *data,int){
    assert(criticalDepth==1);
    if(worker->id==1){auto cmd=reinterpret_cast<StreamUpdateCmd *>(data);assert(cmd->frontend==&streamFrontendGlob);staticOrigins.push_back(cmd->viewPos[0]);}
    if(worker->id==2){assert(streamFrontendGlob.queryClient>0);++combines;}
    if(worker->id==3)++sorts;
}
void Sys_WaitWorkerCmdInternal(Worker *worker){assert(worker->id==3);assert(sorts>ends);}
void R_StreamAlloc_Lock(){}
void R_StreamAlloc_Unlock(){}
void R_StreamUpdate_EndQuery_Internal(){++ends;}
void R_CheckHighmipAabbs(){}
bool R_StreamUpdate_TryBeginQuery(){if(!streamEnabled||streamFrontendGlob.queryInProgress)return false;++begins;return true;}
void R_StreamUpdatePreventedMaterials(){}
void R_StreamUpdateForcedModels(){}
void R_StreamUpdateTouchedModels(){}
void R_StreamUpdate_AddForcedImages(float,float){}
void R_StreamUpdate_AddInitialImages(float){}
void R_StreamUpdateDynamicModels(const float *origin,float,unsigned,float *){dynamicOrigins.push_back(origin[0]);}
template<class...T> bool Assert_MyHandler(T...){++assertions;return false;}
'''
for sig in ['void __cdecl R_StreamUpdate_EndQuery()',
    'char __cdecl R_StreamUpdate_FindImageAndOptimize(',
    'void __cdecl R_StreamUpdatePerClient(', 'void __cdecl R_StreamUpdate_End()']:
    code+=function('src/gfx_d3d/r_stream.cpp',sig)+'\n'
# The public wrapper's dvar/flush handling is outside this query-state test.
code+='char R_StreamUpdate(const float *origin){return streamEnabled?R_StreamUpdate_FindImageAndOptimize(origin):0;}\n'
code+=function('src/gfx_d3d/r_scene.cpp','static void R_UpdateViewStreaming(')+'\n'
code+=r'''
int main(){
    maxDist.current.value=1000;hidden.current.value=2;
    float host[3]{10,20,30},guest[3]{900,800,700};
    // Each frame begins once, collects both cameras, then combines/sorts once.
    // Repeating catches a queryClient left open at the next frame boundary.
    for(int frame=0;frame<3;++frame){
        R_UpdateViewStreaming(host,0,false,false,false);
        assert(streamFrontendGlob.queryInProgress&&streamFrontendGlob.queryClient==1);
        R_UpdateViewStreaming(guest,0,false,false,true);
        assert(streamFrontendGlob.queryClient==2&&begins==frame+1&&ends==frame);
        assert(staticOrigins[2*frame]==10&&staticOrigins[2*frame+1]==900);
        assert(dynamicOrigins[2*frame]==10&&dynamicOrigins[2*frame+1]==900);
        R_StreamUpdate_End();
        assert(streamFrontendGlob.queryClient==-1&&combines==frame+1&&sorts==frame+1);
        assert(criticalDepth==0&&assertions==0);
    }
    R_StreamUpdate_EndQuery();assert(ends==3&&!streamFrontendGlob.queryInProgress);
    // No primary query: guest, missile and stereo passes must not start one.
    R_UpdateViewStreaming(guest,0,false,false,true);
    R_UpdateViewStreaming(host,0,true,false,false);
    R_UpdateViewStreaming(host,0,false,true,false);
    assert(begins==3&&streamFrontendGlob.queryClient==-1&&staticOrigins.size()==6);
    streamEnabled=false;
    R_UpdateViewStreaming(host,0,false,false,false);R_UpdateViewStreaming(guest,0,false,false,true);
    assert(begins==3&&streamFrontendGlob.queryClient==-1);
    streamEnabled=true;freeze.current.enabled=true;
    R_UpdateViewStreaming(host,0,false,false,false);R_UpdateViewStreaming(guest,0,false,false,true);
    assert(begins==3&&streamFrontendGlob.queryClient==-1);
    freeze.current.enabled=false;
    // Existing normal second-local-client behavior is unchanged.
    R_UpdateViewStreaming(host,0,false,false,false);
    R_UpdateViewStreaming(guest,1,false,false,false);
    assert(begins==4&&streamFrontendGlob.queryClient==2);
    R_StreamUpdate_End();R_StreamUpdate_EndQuery();
    assert(assertions==0&&criticalDepth==0&&combines==4&&sorts==4&&ends==4);
    puts("PASS: one streaming query per frame, both camera origins, end/restart, disabled/frozen and auxiliary views");
}
'''
cpp=out/'streaming.cpp';cpp.write_text(code,encoding='utf-8')
exe=out/'streaming.exe'
subprocess.run([args.compiler,'-std=c++20','-fno-strict-aliasing',str(cpp),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
