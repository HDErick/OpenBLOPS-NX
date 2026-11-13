"""Exercise custom-class UI, host profile transfer and script readers without gameplay."""
from pathlib import Path
import argparse
import os
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler', default='C:/msys64/ucrt64/bin/g++.exe')
parser.add_argument('--baseline', help='Read source from this Git revision to reproduce the bug')
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
out = root / 'build-nx/custom-class-tests'
out.mkdir(parents=True, exist_ok=True)
env = dict(os.environ)
env['PATH'] = str(Path(args.compiler).parent) + os.pathsep + env.get('PATH', '')

def source(path):
    if args.baseline:
        return subprocess.check_output(['git', 'show', f'{args.baseline}:{path}'], cwd=root).decode()
    return (root / path).read_text()

def function(path, name):
    text = source(path)
    start = text.index(name)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

code = r'''
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <cstdlib>
#include <string>
#include <vector>
#include <stdexcept>
#ifndef __cdecl
#define __cdecl
#endif
#define __int64 long long
#define __int8 char
#define iassert(x) do { if (!(x)) throw std::runtime_error("engine assertion: " #x); } while(0)
#define __debugbreak() throw std::runtime_error("engine breakpoint")
#define RETURN_ZERO32() 0
constexpr int STATS_BUFFER_SIZE=256, STATS_LOCATION_NORMAL=0, STATS_LOCATION_GLOBAL=6;
constexpr int ALL_STATS_PACKETS_RECEIVED=3, NA_LOOPBACK=1, NA_IP=2, CS_RECONNECTING=3;
constexpr int SCRIPTINSTANCE_SERVER=0, VAL_INT=0, VAL_STRING=2;
int failures=0;
#define CHECK(x) do { if (!(x)) { std::printf("FAIL line %d: %s\n", __LINE__, #x); ++failures; } } while(0)
struct persistentStats { unsigned char statsBuffer[STATS_BUFFER_SIZE]{}; bool metadata[4]{}; } profile, globalProfile;
bool missingProfile=false;
persistentStats *LiveStorage_GetStatsBuffer(int controller,int location,int){
    CHECK(controller==0); return missingProfile ? nullptr : location==STATS_LOCATION_GLOBAL ? &globalProfile : &profile;
}
struct ddlMember {int type=4,permission=0,arraySize=1,enumIndex=-1;} intMember{0}, stringMember{5}, wideMember{3}, globalMember{0,2}, structMember;
struct ddlDef_t{} schema;
struct ddlState_t { ddlDef_t *ddl=&schema; ddlMember *member=nullptr; int arrayIndex=-1; int offset=0; std::string path; } rootState, cacState;
ddlState_t *LiveStats_GetRootDDLState(){return &rootState;}
ddlState_t *LiveStats_GetCacDDLState(){cacState.path="cacLoadouts";return &cacState;}
ddlDef_t *LiveStats_GetStatsDDL(){return &schema;}
bool DDL_MoveToName(const ddlState_t *from,ddlState_t *to,const char *name){
    ddlState_t next=*from; next.path+=(next.path.empty()?"":".")+std::string(name);
    next.member=&structMember; next.offset=0;
    if(!std::strcmp(name,"primary")){next.member=&intMember;next.offset=4;}
    else if(!std::strcmp(name,"secondary")){next.member=&intMember;next.offset=8;}
    else if(!std::strcmp(name,"killstreak1")){next.member=&intMember;next.offset=12;}
    else if(!std::strcmp(name,"name")){next.member=&stringMember;next.offset=16;}
    else if(!std::strcmp(name,"wide")){next.member=&wideMember;next.offset=64;}
    else if(!std::strcmp(name,"globalValue")){next.member=&globalMember;next.offset=80;}
    else if(std::strcmp(name,"cacLoadouts")&&std::strcmp(name,"customClass1"))return false;
    *to=next;return true;
}
bool DDL_MoveTo(const ddlState_t *from,ddlState_t *to,int count,...){
    ddlState_t state=*from;va_list args;va_start(args,count);bool ok=true;
    for(int i=0;i<count;++i)if(!DDL_MoveToName(&state,&state,va_arg(args,const char *)))ok=false;
    va_end(args);if(ok)*to=state;return ok;
}
bool DDL_MoveToIndex(ddlState_t *,ddlState_t *,int,int){return false;}
bool DDL_AssociateBuffer(char *buffer,int size,ddlDef_t *){return size==STATS_BUFFER_SIZE && (unsigned char)buffer[0]==0x66;}
unsigned int DDL_GetInt(ddlState_t *s,char *b){unsigned int v;std::memcpy(&v,b+s->offset,4);return v;}
unsigned long long DDL_GetInt64(ddlState_t *s,char *b){unsigned long long v;std::memcpy(&v,b+s->offset,8);return v;}
char *DDL_GetString(ddlState_t *s,char *b){return b+s->offset;}
void DDL_PrintError(const char *,...){}
struct client_t {struct {int state=CS_RECONNECTING;struct {struct {int type=NA_LOOPBACK;} remoteAddress;} netchan;} header;
    unsigned char stats[STATS_BUFFER_SIZE]{},globalStats[STATS_BUFFER_SIZE]{};
    bool bIsTestClient=false,bIsDemoClient=false,statsValidated=false;int statPacketsReceived=0;};
struct {client_t clients[4];} svs;
bool SV_CustomClassSyncReady(const client_t*){return false;}
struct dvar_t {struct {int integer=4;bool enabled=false;} current;} maxClients,basicTraining,customClasses;
const dvar_t *com_maxclients=&maxClients,*xblive_basictraining=&basicTraining,*ui_useCustomClassInfo=&customClasses;
int Dvar_GetInt(const dvar_t *d){return d->current.enabled;}
int Assert_MyHandler(const char *,int,int,const char *,...){return 0;}
unsigned char GetCustomClassLoadoutItemForClassName(int,const char *,const char *){return 77;}
char *va(const char *format,...){static char buffers[8][256];static int index;char *b=buffers[(index++)%8];va_list a;va_start(a,format);vsnprintf(b,256,format,a);va_end(a);return b;}
struct scr_entref_t {unsigned int entnum=0;};
struct gentity_s {int *client;struct {unsigned int number;} s;};
int dummy;gentity_s players[]={{&dummy,{0}},{&dummy,{1}},{&dummy,{2}},{nullptr,{3}}};
gentity_s *GetEntity(scr_entref_t ref){return &players[ref.entnum];}
std::vector<std::string> parameters;
int scriptInt;std::string scriptString;
unsigned int Scr_GetNumParam(int){return parameters.size();}
char *Scr_GetString(unsigned int n,int){return parameters.at(n).data();}
int Scr_GetType(int,int){return 2;}
int Scr_GetInt(int n,int){return std::atoi(parameters.at(n).c_str());}
void Scr_Error(const char *s,int){throw std::runtime_error(s);}
void Scr_AddInt(unsigned int n,int){scriptInt=n;}
void Scr_AddString(const char *s,int){scriptString=s;}
union VariableUnion {int intValue;};
struct itemDef_s{};
struct Operand {int dataType=VAL_STRING; union {int intVal;const char *string;} internals{};};
struct OperandList {std::vector<Operand> operands;};
struct OperandStack {std::vector<const char *> path;Operand result;int consumed=0,added=0;};
void GetOperandList(OperandStack *stack,OperandList *list){++stack->consumed;for(auto p:stack->path){Operand op;op.internals.string=p;list->operands.push_back(op);}}
void AddOperandToStack(OperandStack *stack,Operand *value){stack->result=*value;++stack->added;}
int Com_LocalClient_GetControllerIndex(int){return 0;}
bool GetSearchState(ddlState_t *state,OperandList *list,int){for(auto op:list->operands)if(!DDL_MoveToName(state,state,op.internals.string))return false;return true;}
int LiveStats_CanPerformStatOperation(int){return !missingProfile;}
bool LiveStats_GetBasicTrainingState(unsigned char *){return false;}
'''
server = 'src/client_mp/sv_client_mp.cpp'
if 'static void SV_InitLocalClientStats(' in source(server):
    code += function(server, 'static void SV_InitLocalClientStats(')
else:
    # Execute the original no-service-stats gamestate block in baseline mode.
    s = function(server, 'void __cdecl SV_SendClientGameState(')
    start = s.index('    memset(client->stats, 0, sizeof(client->stats));', s.index('Not sending state'))
    end = s.index('    client->statPacketsReceived = ALL_STATS_PACKETS_RECEIVED;', start)
    end += len('    client->statPacketsReceived = ALL_STATS_PACKETS_RECEIVED;')
    code += 'static void SV_InitLocalClientStats(client_t *client){\n'+s[start:end]+'\n}\n'
for name in ['unsigned int __cdecl SV_GetClientDIntStat(', 'char *__cdecl SV_GetClientDStringStat(', 'unsigned __int64 __cdecl SV_GetClientDInt64Stat(']:
    code += function(server, name)
for signature in ['unsigned int __cdecl LiveStats_GetDIntStat(', 'char *__cdecl LiveStats_GetDStringStat(', ('unsigned int' if args.baseline else 'unsigned __int64') + ' __cdecl LiveStats_GetDInt64Stat(']:
    code += function('src/live/live_stats.cpp', signature)
code += function('src/universal/com_expressions_eval.cpp','void __cdecl GetDStat(')
code += function('src/game_mp/g_scr_main_mp.cpp','void __cdecl GScr_GetLoadoutItemFromProfile(')
code += function('src/game_mp/g_scr_main_mp.cpp','void __cdecl GScr_GetDStat(')
code += r'''
void put(unsigned char *b,int offset,unsigned int value){std::memcpy(b+offset,&value,4);}
int main(){
    std::setvbuf(stdout,nullptr,_IONBF,0);
    profile.statsBuffer[0]=globalProfile.statsBuffer[0]=0x66;
    put(profile.statsBuffer,4,42);put(profile.statsBuffer,8,13);put(profile.statsBuffer,12,19);
    std::strcpy((char *)profile.statsBuffer+16,"My custom class");
    unsigned long long wide=0x123456789abcdef0ULL;std::memcpy(profile.statsBuffer+64,&wide,8);
    put(profile.statsBuffer,80,3);put(globalProfile.statsBuffer,80,99);
    // Frontend names, item values, full-width strings, and exactly one consumed list.
    OperandStack ui;ui.path={"cacLoadouts","customClass1","name"};GetDStat(0,nullptr,&ui);
#if defined(KISAK_MP) && defined(OPENBLOPS_NO_STEAM_AUTH)
    CHECK(ui.result.dataType==VAL_STRING);if(ui.result.dataType==VAL_STRING)CHECK(!std::strcmp(ui.result.internals.string,"My custom class"));
    ui={};ui.path={"wide"};GetDStat(0,nullptr,&ui);CHECK(ui.result.dataType==VAL_STRING);
    if(ui.result.dataType==VAL_STRING)CHECK(std::string(ui.result.internals.string)=="1311768467463790320");
    ui={};ui.path={"cacLoadouts","customClass1","primary"};GetDStat(0,nullptr,&ui);CHECK(ui.result.internals.intVal==42);
#else
    CHECK(ui.result.dataType==VAL_INT && ui.result.internals.intVal==0);
#endif
    CHECK(ui.consumed==1 && ui.added==1);
#if defined(KISAK_MP) && defined(OPENBLOPS_NO_STEAM_AUTH)
    // Menus can evaluate before the profile has finished loading.
    missingProfile=true;ui={};ui.path={"cacLoadouts","customClass1","name"};GetDStat(0,nullptr,&ui);
    CHECK(ui.result.dataType==VAL_STRING);if(ui.result.dataType==VAL_STRING)CHECK(ui.result.internals.string && !*ui.result.internals.string);
    missingProfile=false;
#endif
    // Seed stale slots to verify transition/reset does not leak profile data.
    for(int i=0;i<4;++i){std::memset(svs.clients[i].stats,0xaa,STATS_BUFFER_SIZE);std::memset(svs.clients[i].globalStats,0xbb,STATS_BUFFER_SIZE);svs.clients[i].statsValidated=true;}
    svs.clients[1].header.netchan.remoteAddress.type=NA_IP;
    svs.clients[2].bIsTestClient=true;svs.clients[3].bIsDemoClient=true;
    for(auto &client:svs.clients)SV_InitLocalClientStats(&client);
#if defined(KISAK_NX) || (defined(KISAK_MP) && defined(OPENBLOPS_NO_STEAM_AUTH))
    CHECK(!std::memcmp(svs.clients[0].stats,profile.statsBuffer,STATS_BUFFER_SIZE));
    CHECK(!std::memcmp(svs.clients[0].globalStats,globalProfile.statsBuffer,STATS_BUFFER_SIZE));
#else
    CHECK(svs.clients[0].stats[0]==0);
#endif
    for(int i=1;i<4;++i){CHECK(svs.clients[i].stats[0]==0);CHECK(svs.clients[i].globalStats[0]==0);}
    for(auto &client:svs.clients){CHECK(!client.statsValidated);CHECK(client.statPacketsReceived==ALL_STATS_PACKETS_RECEIVED);}
    parameters={"customClass1","primary"};GScr_GetLoadoutItemFromProfile({0});
#if defined(KISAK_NX) || (defined(KISAK_MP) && defined(OPENBLOPS_NO_STEAM_AUTH))
    CHECK(scriptInt==42);
    parameters={"customClass1","secondary"};GScr_GetLoadoutItemFromProfile({0});CHECK(scriptInt==13);
    parameters={"killstreak1"};GScr_GetLoadoutItemFromProfile({0});CHECK(scriptInt==19);
    customClasses.current.enabled=true;parameters={"1","primary"};GScr_GetLoadoutItemFromProfile({0});CHECK(scriptInt==77);customClasses.current.enabled=false;
    parameters={"customClass1","missing"};GScr_GetLoadoutItemFromProfile({0});CHECK(scriptInt==0);
    bool rejected=false;try{GScr_GetLoadoutItemFromProfile({3});}catch(const std::runtime_error &){rejected=true;}CHECK(rejected);
    try {
    parameters={"cacLoadouts","customClass1","name"};GScr_GetDStat({0});CHECK(scriptString=="My custom class");
    parameters={"cacLoadouts","customClass1","primary"};GScr_GetDStat({0});CHECK(scriptInt==42);
    parameters={"wide"};GScr_GetDStat({0});CHECK(scriptString=="1311768467463790320");
    parameters={"globalValue"};GScr_GetDStat({0});CHECK(scriptInt==99);
    basicTraining.current.enabled=true;GScr_GetDStat({0});CHECK(scriptInt==3);basicTraining.current.enabled=false;
    rejected=false;parameters={"cacLoadouts","missing"};try{GScr_GetDStat({0});}catch(const std::runtime_error &){rejected=true;}CHECK(rejected);
#if defined(KISAK_MP) && defined(OPENBLOPS_NO_STEAM_AUTH)
    parameters={"customClass1","primary"};GScr_GetLoadoutItemFromProfile({1});CHECK(scriptInt==0);
    // A profile-less peer returns zero until a valid per-client buffer exists.
    svs.clients[0].statPacketsReceived=0;GScr_GetLoadoutItemFromProfile({0});CHECK(scriptInt==0);
    svs.clients[0].statPacketsReceived=ALL_STATS_PACKETS_RECEIVED;svs.clients[0].header.state=0;GScr_GetLoadoutItemFromProfile({0});CHECK(scriptInt==0);
    svs.clients[0].header.state=CS_RECONNECTING;
#endif
    } catch(const std::runtime_error &error) { std::printf("FAIL script stats: %s\n", error.what());++failures; }
    // Guest shares the local profile, and a new match captures class edits.
    svs.clients[2].bIsTestClient=false;SV_InitLocalClientStats(&svs.clients[2]);
    CHECK(!std::memcmp(svs.clients[2].stats,profile.statsBuffer,STATS_BUFFER_SIZE));
    put(profile.statsBuffer,4,57);SV_InitLocalClientStats(&svs.clients[0]);parameters={"customClass1","primary"};GScr_GetLoadoutItemFromProfile({0});CHECK(scriptInt==57);
    missingProfile=true;SV_InitLocalClientStats(&svs.clients[0]);CHECK(svs.clients[0].stats[0]==0 && svs.clients[0].globalStats[0]==0);
#else
    CHECK(scriptInt==0);
    bool rejected=false;try{GScr_GetDStat({0});}catch(const std::runtime_error &){rejected=true;}CHECK(rejected);
#endif
    if(failures)return 1;
    puts("PASS: class names, weapons, profile transfer, cosmetics stats, global stats, isolation and legacy guards");
}
'''
configs = {'pc-direct-mp':['KISAK_MP','OPENBLOPS_NO_STEAM_AUTH'],
           'nx-direct-mp':['KISAK_MP','KISAK_NX','OPENBLOPS_NO_STEAM_AUTH'],
           'nx-legacy-mp':['KISAK_MP','KISAK_NX'],
           'pc-legacy-mp':['KISAK_MP'], 'pc-direct-sp':['KISAK_SP','OPENBLOPS_NO_STEAM_AUTH']}
# The baseline PC build must reproduce failure; don't stop at compile diagnostics.
if args.baseline:
    configs={'pc-direct-mp':configs['pc-direct-mp']}
for label, defines in configs.items():
    cpp=out/f'{label}.cpp';exe=out/f'{label}.exe'
    cpp.write_text(code)
    subprocess.run([args.compiler,'-std=c++17','-O0',*[f'-D{x}' for x in defines],str(cpp),'-o',str(exe)],check=True,env=env)
    print(label,flush=True)
    subprocess.run([str(exe)],check=True,env=env)
