"""Check custom bot setup/script adjustment and community browser totals without gameplay."""
from pathlib import Path
import argparse,os,subprocess,json,struct,zlib
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--compiler',default='C:/msys64/ucrt64/bin/g++.exe');a=p.parse_args()
r=Path(__file__).resolve().parents[3];out=r/'build-nx/custom-bot-tests';out.mkdir(parents=True,exist_ok=True)
def function(path,sig):
 s=(r/path).read_text();start=s.index(sig);end=s.index('{',start)+1;depth=1
 while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
 return s[start:end]
# Read the actual shipped bot source through KBZ's relocation table.
def bot_source():
 b=Path('F:/pluto_t5_full_game/kbz/common_mp.kbz').read_bytes();nb=struct.unpack_from('<I',b,8)[0];sizes=struct.unpack_from('<'+'I'*nb,b,12);nr,na=struct.unpack_from('<II',b,12+4*nb);pos=20+4*nb;blocks=[]
 for i,n in enumerate(sizes):blocks.append(b[pos:pos+n] if i!=1 else b'');pos+=n if i!=1 else 0
 rel={}
 for _ in range(nr):sb,so,tb,to=struct.unpack_from('<BIBI',b,pos);pos+=10;rel[(sb,so)]=(tb,to)
 def ptr(p,o=0):return rel.get((p[0],p[1]+o)) if p else None
 def string(p):
  data=blocks[p[0]];return data[p[1]:data.find(b'\0',p[1])].decode()
 for _ in range(na):
  t,ab,ao=struct.unpack_from('<IBI',b,pos);pos+=9;m=(ab,ao)
  if t==36 and string(ptr(m))=='maps/mp/gametypes/_bot.gsc':
   n=struct.unpack_from('<i',blocks[ab],ao+8)[0];buf=ptr(m,16);data=blocks[buf[0]][buf[1]:buf[1]+n];uncompressed,compressed=struct.unpack_from('<II',data);decoded=zlib.decompress(data[8:8+compressed]);assert len(decoded)==uncompressed;return decoded.decode().rstrip('\0')
 raise AssertionError('Missing shipped _bot.gsc')
script=bot_source();(out/'shipped_bot.gsc').write_text(script)
assert 'scr_bots_managed_spawn' in script and 'differenceAxis = axis_num - countAxis;' in script
code=r'''
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <map>
#include <string>
#include <vector>
#define KISAK_MP 1
#ifndef __cdecl
#define __cdecl
#endif
std::map<std::string,std::string>vars;
bool Dvar_GetBool(const char*n){return vars[n]=="1";}
int Dvar_GetInt(const char*n){return std::atoi(vars[n].c_str());}
const char*Dvar_GetString(const char*n){return vars[n].c_str();}
void Dvar_SetIntByName(const char*n,int v){vars[n]=std::to_string(v);}
void Dvar_SetStringByName(const char*n,const char*v){vars[n]=v;}
int I_stricmp(const char*a,const char*b){return strcmp(a,b);}
void Com_PrintWarning(int,const char*,...){}
using scriptInstance_t=int;constexpr int SCRIPTINSTANCE_SERVER=0;
void*Hunk_AllocateTempMemoryHigh(int n,const char*){return malloc(n);}
'''
for path,sig in [('src/server_mp/sv_bot_mp.cpp','bool SV_IsCustomMatchBotSession('),('src/server_mp/sv_bot_mp.cpp','void SV_ApplyCustomMatchBots('),('src/clientscript/cscr_parser.cpp','static char *Scr_AdjustCustomMatchBots(')]:code+=function(path,sig)+'\n'
code+='const char* shipped = '+json.dumps(script)+';\n'
code+=r'''
int main(){
 vars["scr_num_bots"]="5";vars["xblive_privatematch"]="1";
 assert(SV_IsCustomMatchBotSession());
 int initialLength=int(strlen(shipped));char*initial=strdup(shipped);
 char*initialAdjusted=Scr_AdjustCustomMatchBots(0,"maps/mp/gametypes/_bot.gsc",initial,&initialLength);
 assert(initialAdjusted!=initial&&strstr(initialAdjusted,"if ( !GetDvarInt( #\"xblive_basictraining\" ) )"));free(initial);free(initialAdjusted);
 SV_ApplyCustomMatchBots();
 assert(vars["scr_bots_managed_all"]=="5"&&vars["scr_bots_managed_spawn"]=="1"&&vars["scr_bot_difficulty"]=="normal");
 // The real direct Host Match flags: xblive_privatematch is deliberately false.
 vars["xblive_privatematch"]="0";vars["onlinegame"]="0";vars["systemlink"]="1";
#ifdef OPENBLOPS_NO_STEAM_AUTH
 assert(SV_IsCustomMatchBotSession());
#else
 assert(!SV_IsCustomMatchBotSession());
#endif
 SV_ApplyCustomMatchBots();
#ifdef OPENBLOPS_NO_STEAM_AUTH
 assert(vars["scr_bots_managed_spawn"]=="1"&&vars["scr_bots_managed_all"]=="5");
 // Both the native count setup and the script rewriter must select this mode.
 for(bool privateFlag:{false,true}){
  vars["xblive_privatematch"]=privateFlag?"1":"0";
  int n=int(strlen(shipped));char*input=strdup(shipped);char*adjusted=Scr_AdjustCustomMatchBots(0,"maps/mp/gametypes/_bot.gsc",input,&n);
  assert(adjusted!=input&&strstr(adjusted,"friendlyBots = GetDvarInt")&&strstr(adjusted,"desiredBots = GetDvarInt"));
  assert(!strstr(adjusted,"if ( level.onlineGame && !GetDvarInt( #\"xblive_basictraining\" ) )"));
  assert(strstr(adjusted,"#\"scr_bot_difficulty\"")&&!strstr(adjusted,"#\"bot_difficulty\""));free(input);free(adjusted);
 }
 vars["xblive_privatematch"]="0";vars["scr_num_bots"]="0";vars["scr_num_bots_friendly"]="3";vars["scr_num_bots_enemy"]="4";
 SV_ApplyCustomMatchBots();assert(vars["scr_bots_managed_spawn"]=="1");
 vars["scr_num_bots_friendly"]="0";vars["scr_num_bots_enemy"]="0";SV_ApplyCustomMatchBots();assert(vars["scr_bots_managed_spawn"]=="0");
 vars["scr_num_bots"]="999";SV_ApplyCustomMatchBots();assert(vars["scr_bots_managed_all"]=="17");
 vars["scr_num_bots"]="-5";SV_ApplyCustomMatchBots();assert(vars["scr_bots_managed_all"]=="0");
 for(const char*flag:{"xblive_rankedmatch","xblive_wagermatch","xblive_basictraining","xblive_theater"}){
  vars[flag]="1";assert(!SV_IsCustomMatchBotSession());SV_ApplyCustomMatchBots();assert(vars["scr_bots_managed_spawn"]=="0");vars[flag]="0";
 }
 vars["dedicated"]="2";assert(!SV_IsCustomMatchBotSession());vars["scr_bots_managed_all"]="9";SV_ApplyCustomMatchBots();assert(vars["scr_bots_managed_all"]=="9");
 vars["dedicated"]="0";vars["onlinegame"]="1";assert(!SV_IsCustomMatchBotSession());
#else
 assert(vars["scr_bots_managed_spawn"]=="0");
#endif
 puts("PASS: private/custom host bot counts, team counts, native/script gates, shipped bot source adjustment, exclusions and legacy behavior");
}
'''
env=os.environ.copy();env['PATH']=str(Path(a.compiler).parent)+os.pathsep+env.get('PATH','')
for direct in (False,True):
 cpp=out/('direct.cpp' if direct else 'legacy.cpp');exe=cpp.with_suffix('.exe');cpp.write_text(code)
 subprocess.run([a.compiler,'-std=c++17','-Wall','-Wextra','-Werror',*(['-DOPENBLOPS_NO_STEAM_AUTH'] if direct else []),str(cpp),'-o',str(exe)],check=True,env=env);subprocess.run([str(exe)],check=True,env=env)
code=r'''
#include <cassert>
#include <map>
#include <string>
#include <vector>
struct Row{int players=0;};struct Client{std::vector<Row> rows;const std::vector<Row>&Rows(){return rows;}}store;
auto*client=&store;struct{bool uiStarted=true;}cls;
int source=0;bool enabled=true;std::map<std::string,std::string>labels;
bool Directory_Source(int n){return enabled&&n==0;}int Dvar_GetInt(const char*){return source;}
const char*UI_SafeTranslateString(const char*n){return n;}
char*UI_ReplaceConversionInt(const char*n,int v){static std::string result;result=std::string(n)+":"+std::to_string(v);return result.data();}
void Dvar_SetStringByName(const char*n,const char*v){labels[n]=v;}
'''+function('src/openblops_directory/directory_engine.cpp','bool Directory_UpdateBrowserCounts(')+r'''
int main(){
 store.rows={{3},{7},{0}};assert(Directory_UpdateBrowserCounts());
 assert(labels["ui_browserPlayerCount"]=="PLATFORM_PLAYERS_ONLINE:10"&&labels["ui_browserDedicatedServerCount"]=="PLATFORM_ALL_SERVERS:3");
 store.rows={{2}};Directory_UpdateBrowserCounts();assert(labels["ui_browserPlayerCount"]=="PLATFORM_PLAYERS_ONLINE:2"&&labels["ui_browserDedicatedServerCount"]=="PLATFORM_ALL_SERVERS:1");
 store.rows.clear();Directory_UpdateBrowserCounts();assert(labels["ui_browserPlayerCount"]=="PLATFORM_PLAYERS_ONLINE:0"&&labels["ui_browserDedicatedServerCount"]=="PLATFORM_ALL_SERVERS:0");
 source=1;labels.clear();assert(!Directory_UpdateBrowserCounts()&&labels.empty());source=0;cls.uiStarted=false;assert(!Directory_UpdateBrowserCounts()&&labels.empty());
}
'''
cpp=out/'counts.cpp';exe=cpp.with_suffix('.exe');cpp.write_text(code)
subprocess.run([a.compiler,'-std=c++17','-Wall','-Wextra','-Werror',str(cpp),'-o',str(exe)],check=True,env=env);subprocess.run([str(exe)],check=True,env=env)
assert 'if (!Directory_UpdateBrowserCounts())' in (r/'src/ui/ui_main.cpp').read_text()
assert 'UpdateRows();\n    Directory_UpdateBrowserCounts();' in (r/'src/openblops_directory/directory_engine.cpp').read_text()
print('PASS: localized browser totals follow actual directory rows, including asynchronous refreshes, player changes and empty results')
