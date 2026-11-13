"""Run real offline profile init/save/load in separate processes, without gameplay."""
from pathlib import Path
import argparse
import os
import struct
import subprocess
import time

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--compiler',default='C:/msys64/ucrt64/bin/g++.exe')
a=p.parse_args()
r=Path(__file__).resolve().parents[3]
out=r/'build-nx/offline-profile-tests'
out.mkdir(parents=True,exist_ok=True)

def function(path,name):
 s=(r/path).read_text();start=s.index(name);end=s.index('{',start)+1;depth=1
 while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
 return s[start:end]

storage=(r/'src/live/live_storage.cpp').read_text()
code=r'''
#include <live/offline_profile_store.h>
#include <cassert>
#include <cstdarg>
#include <filesystem>
#include <map>
#include <string>
#ifndef __cdecl
#define __cdecl
#endif
#define __debugbreak() assert(false)
struct persistentStats {unsigned char statsBuffer[STATS_BUFFER_SIZE]{};bool fetched=false,validated=false,checksum=false;};
struct playerNetworkData {persistentStats normal,basic;alignas(persistentStats) unsigned char globalplayerStats[sizeof(persistentStats)]{};} controllerNetworkData[1];
using statsLocation=int;
constexpr int STATS_LOCATION_NORMAL=0,STATS_LOCATION_FORCE_NORMAL=1,STATS_LOCATION_STABLE=3,STATS_LOCATION_BASICTRAINING=5,STATS_LOCATION_GLOBAL=6;
struct dvar_s {const char *name="";struct {bool enabled=false;const char *string="";} current;} modes[6],homepath,newName;
using dvar_t=dvar_s;
const dvar_s *onlinegame=&modes[0],*xblive_privatematch=&modes[1],*xblive_basictraining=&modes[2],*xblive_wagermatch=&modes[3],*xblive_theater=&modes[4],*xblive_clanmatch=&modes[5],*fs_homepath=&homepath,*ui_custom_newname=&newName;
struct ddlDef_t {int version=100,size=321000;} schema;ddlDef_t *g_statsDDL=&schema;
const char *customClassDvars[]={"customclass1","customclass2","customclass3","customclass4","customclass5","prestigeclass1","prestigeclass2","prestigeclass3","prestigeclass4","prestigeclass5"};
std::map<std::string,std::string> names;
unsigned int now=0;int defaultCount=0,serviceCalls=0,errorCount=0;
void Dvar_SetBool(dvar_s *d,bool b){d->current.enabled=b;}
void Com_GameMode_ResetGameModes(){for(auto &d:modes)d.current.enabled=false;}
void Dvar_SetStringByName(const char *n,const char *v){names[n]=v;}
void Dvar_SetString(dvar_s *d,const char *v){names[d->name]=v;}
const char *Dvar_GetString(const char *n){return names[n].c_str();}
void I_strncpyz(char *dst,const char *src,size_t n){std::snprintf(dst,n,"%s",src);}
void Com_Printf(int,const char *,...){}
void Com_PrintError(int,const char *,...){++errorCount;}
unsigned int Sys_Milliseconds(){return now;}
persistentStats *rawGlobal(){return reinterpret_cast<persistentStats *>(controllerNetworkData[0].globalplayerStats);}
persistentStats *LiveStorage_GetStatsBuffer(int,int location,bool){
 if(location==STATS_LOCATION_FORCE_NORMAL)return &controllerNetworkData[0].normal;
 if(location==STATS_LOCATION_BASICTRAINING)return &controllerNetworkData[0].basic;
 if(xblive_basictraining->current.enabled)return &controllerNetworkData[0].basic;
 return location==STATS_LOCATION_GLOBAL?rawGlobal():&controllerNetworkData[0].normal;
}
void put(unsigned char *b,int at,unsigned int value){std::memcpy(b+at,&value,4);}
unsigned int get(const unsigned char *b,int at){unsigned int value;std::memcpy(&value,b+at,4);return value;}
int LiveStats_ChecksumGamerStats(unsigned char *b,int n){unsigned int h=2166136261u;for(int i=0;i<n;++i)h=(h^b[i])*16777619u;return static_cast<int>(h);}
void LiveStats_WriteChecksumToBuffer(unsigned char *b,int n){put(b,0,LiveStats_ChecksumGamerStats(b+4,n-4));}
bool DDL_AssociateBuffer(char *b,int,ddlDef_t *ddl){return get((unsigned char *)b,4)==unsigned(ddl->version);}
bool LiveStats_GetBasicTrainingState(unsigned char *b){return b[12]!=0;}
bool LiveStats_GetOnlineRankedState(unsigned char *b){return b[13]!=0;}
void LiveStats_SetBasicTrainingState(unsigned char *b,bool v){b[12]=v;}
void LiveStats_SetOnlineRankedState(unsigned char *b,bool v){b[13]=v;}
int LiveStorage_ValidateWithDDL(int,int location){auto *s=LiveStorage_GetStatsBuffer(0,location,false);if(!get(s->statsBuffer,4))put(s->statsBuffer,4,schema.version);s->validated=true;return DDL_AssociateBuffer((char *)s->statsBuffer,STATS_BUFFER_SIZE,&schema);}
void LiveStorage_SetStatsFetched(int,int location,bool v){LiveStorage_GetStatsBuffer(0,location,false)->fetched=v;}
void LiveStorage_SetStatsChecksumValid(int,int location,bool v){LiveStorage_GetStatsBuffer(0,location,false)->checksum=v;}
void LiveStats_InitStatsBuffer(int){++defaultCount;auto *b=LiveStorage_GetStatsBuffer(0,STATS_LOCATION_NORMAL,false)->statsBuffer;put(b,128,17);put(b,132,8);}
unsigned int BG_UnlockablesGetEquippedItemInSlot(int,const char *,const char *){return 1;}
bool BG_UnlockablesIsItemValidNotNull(int i){return i==1;}
std::vector<unsigned char> stableNormal,stableGlobal;
void LiveStats_MakeStableStatsBuffer(int){auto *b=controllerNetworkData[0].normal.statsBuffer;stableNormal.assign(b,b+STATS_BUFFER_SIZE);}
void LiveStats_MakeStableGlobalStatsBuffer(int){auto *b=rawGlobal()->statsBuffer;stableGlobal.assign(b,b+STATS_BUFFER_SIZE);}
void FS_BuildOSPath(char *base,char *dir,char *file,char *out){std::snprintf(out,256,"%s/%s/%s",base,dir,file);}
int FS_CreatePath(char *path){std::error_code error;std::filesystem::create_directories(std::filesystem::path(path).parent_path(),error);return error?1:0;}
bool G_ExitAfterToolComplete(){return false;}
void LiveStorage_WriteBasicTrainingStats(int){++serviceCalls;}
void CL_CACValidateRequest_f(){++serviceCalls;}
'''
start=storage.index('enum OfflineStatsState ')
end=storage.index('\ncmd_function_s LiveStorage_FakeComErrorCmd_VAR;',start)
code+=storage[start:end]
code+=function('src/live/live_storage_win.cpp','void __cdecl LiveStorage_UploadStats(')
code+=function('src/client_mp/cl_main_mp.cpp','void __cdecl CL_CmdSetNewCustomName(')
code+=r'''
void checkSaved(){
 assert(get(controllerNetworkData[0].normal.statsBuffer,128)==57);
 assert(get(controllerNetworkData[0].normal.statsBuffer,132)==7);
 assert(get(controllerNetworkData[0].normal.statsBuffer,136)==15);
 assert(get(rawGlobal()->statsBuffer,128)==88);
 assert(get(controllerNetworkData[0].basic.statsBuffer,128)==77);
 assert(names["customclass1"]=="Renamed Rifle");
 assert(get(stableNormal.data(),128)==57 && get(stableGlobal.data(),128)==88);
}
int main(int argc,char **argv){
 assert(argc==3);std::string action=argv[1];homepath.current.string=argv[2];newName.name="ui_custom_newname";
 for(int i=0;i<10;++i)names[customClassDvars[i]]="CUSTOM "+std::to_string(i+1);
 modes[0].current.enabled=false;modes[1].current.enabled=true;modes[2].current.enabled=false;
 assert(!LiveStorage_SaveOfflineStats(0));
#if defined(KISAK_MP) && defined(OPENBLOPS_OFFLINE_MENUS) && !defined(KISAK_LIVE)
 assert(LiveStorage_InitOfflineStats(0));assert(defaultCount==2);
 assert(!modes[0].current.enabled && modes[1].current.enabled && !modes[2].current.enabled);
 assert(LiveStorage_OfflineStatsReady(0));assert(!LiveStorage_SaveOfflineStats(1));
 if(action=="write"){
  put(controllerNetworkData[0].normal.statsBuffer,128,42);
  put(controllerNetworkData[0].normal.statsBuffer,132,7); // attachment
  put(controllerNetworkData[0].normal.statsBuffer,136,15); // gold camo
  put(rawGlobal()->statsBuffer,128,88);put(controllerNetworkData[0].basic.statsBuffer,128,77);
  modes[2].current.enabled=true;LiveStorage_UploadStats();assert(serviceCalls==0);
  names["ui_custom_name"]="customclass1";names["ui_custom_newname"]="Renamed Rifle";CL_CmdSetNewCustomName();assert(names["ui_custom_newname"].empty());
  put(controllerNetworkData[0].normal.statsBuffer,128,57);
  now=999;LiveStorage_OfflineStatsFrame();now=1000;LiveStorage_OfflineStatsFrame();assert(errorCount==0);
 }else if(action=="read"){
  checkSaved();assert(LiveStorage_InitOfflineStats(0));assert(defaultCount==2);
  modes[2].current.enabled=true;assert(LiveStorage_SaveOfflineStats(0));assert(errorCount==0);
 }else if(action=="blocked"){
  assert(get(controllerNetworkData[0].normal.statsBuffer,128)==17);assert(!LiveStorage_SaveOfflineStats(0));assert(errorCount==1);
 }else if(action=="fallback"){
  assert(get(controllerNetworkData[0].normal.statsBuffer,128)==42);assert(names["customclass1"]=="Renamed Rifle");
 }else if(action=="default"){
  assert(get(controllerNetworkData[0].normal.statsBuffer,128)==17);
 }else if(action=="retry"){
  checkSaved();put(controllerNetworkData[0].normal.statsBuffer,128,91);assert(!LiveStorage_SaveOfflineStats(0));assert(errorCount==1);assert(!LiveStorage_SaveOfflineStats(0));assert(errorCount==1);
 }
#else
 assert(!LiveStorage_InitOfflineStats(0));assert(defaultCount==0);LiveStorage_OfflineStatsFrame();assert(!LiveStorage_SaveOfflineStats(0));
#endif
 puts("PASS: offline profile process stage completed");
}
'''
env=dict(os.environ);env['PATH']=str(Path(a.compiler).parent)+os.pathsep+env.get('PATH','')
configs={'pc-mp':['KISAK_MP','OPENBLOPS_OFFLINE_MENUS','OPENBLOPS_NO_STEAM_AUTH'],'nx-mp':['KISAK_MP','OPENBLOPS_OFFLINE_MENUS','OPENBLOPS_NO_STEAM_AUTH','KISAK_NX'],'legacy-mp':['KISAK_MP'],'sp':['KISAK_SP']}
for name,defs in configs.items():
 cpp=out/(name+'.cpp');exe=out/(name+'.exe');cpp.write_text(code)
 subprocess.run([a.compiler,'-std=c++17','-O0','-I',str(r/'src'),*[f'-D{x}' for x in defs],str(cpp),'-o',str(exe)],check=True,env=env)
 folder=out/(name+'-'+str(time.time_ns()));folder.mkdir()
 def run(action,target=folder):subprocess.run([str(exe),action,str(target)],check=True,env=env)
 if 'legacy' in name or name=='sp':
  run('default');assert not list(folder.rglob('*.dat'));print(name+': legacy/SP persistence stays disabled',flush=True);continue
 run('default');run('write')
 slots=[folder/'players'/f'openblops-mp-profile-{i}.dat' for i in range(2)]
 assert all(x.exists() for x in slots)
 before=[(x.read_bytes(),x.stat().st_mtime_ns) for x in slots]
 run('read');assert before==[(x.read_bytes(),x.stat().st_mtime_ns) for x in slots], 'Unchanged profile was rewritten'
 # The latest slot was written by the periodic check. Truncation must fall back
 # to the previous snapshot, including the latest rename at that generation.
 newest=max(slots,key=lambda f:struct.unpack_from('<Q',f.read_bytes(),24)[0]);older=next(f for f in slots if f!=newest)
 newest.write_bytes(newest.read_bytes()[:60]);run('fallback')
 newest.write_bytes(before[slots.index(newest)][0]);older_data=older.read_bytes();older.unlink();older.mkdir();run('retry')
 assert newest.read_bytes()==before[slots.index(newest)][0], 'Failed save damaged last valid snapshot'
 older.rmdir();older.write_bytes(older_data)
 # Both corrupt slots are preserved; defaults can run without overwriting them.
 for file in slots:file.write_bytes(b'bad profile')
 run('blocked');assert all(f.read_bytes()==b'bad profile' for f in slots)
 # Restore valid files, then test wrong schema and bounded/extra-length rejects.
 for file,(data,_) in zip(slots,before):file.write_bytes(data+b'extra')
 run('blocked')
 for file,(data,_) in zip(slots,before):file.write_bytes(data)
 incompatible=bytearray(before[0][0]);struct.pack_into('<I',incompatible,12,101)
 # Even an internally checksummed future-format profile must be preserved.
 def fnv(data):
  h=2166136261
  for value in data:h=((h^value)*16777619)&0xffffffff
  return h
 struct.pack_into('<I',incompatible,len(incompatible)-4,fnv(incompatible[:-4]))
 for file in slots:file.write_bytes(incompatible)
 run('blocked');assert all(f.read_bytes()==incompatible for f in slots)
 print(name+': restart restores weapons/attachments/camo/names, distinct global/basic blobs, no redundant writes, corrupt/write-failure recovery and schema preservation',flush=True)

# The real hooks must execute outside Live/Demonware-only frames and before quit
# tears down the schema/filesystem. The dynamic stages above exercise init/upload/rename.
common=(r/'src/qcommon/common.cpp').read_text()
assert 'LiveStorage_OfflineStatsFrame();' in function('src/qcommon/common.cpp','void __cdecl Com_WriteConfiguration(')
quit=function('src/qcommon/common.cpp','void __cdecl    Com_Quit_f(')
assert quit.index('LiveStorage_SaveOfflineStats(0)')<quit.index('GScr_Shutdown()')<quit.index('FS_Shutdown()')
print('PASS: periodic config and normal-quit save hooks are wired before teardown',flush=True)
