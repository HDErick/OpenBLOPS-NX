"""Exercise real SP death scheduling and archived mission progress writes."""
from pathlib import Path
import argparse
import re
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler', default='/c/msys64/ucrt64/bin/g++.exe')
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]

def function(path, name):
    source = (root / 'src' / path).read_text()
    match = re.search(r'^\w[^\n]*\b' + name + r'\([^;]*?\)\s*\{', source, re.M)
    assert match, name
    end = source.index('{', match.start()) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end]

script = (root / 'src/game_mp/g_scr_main_mp.cpp').read_text()
state = script[script.index('struct SPChangeLevelState'):script.index('\nstatic void __cdecl GScr_SetThreatBias_SP()')]
functions = [function(path, name) for path, name in [
    ('qcommon/dvar_cmds.cpp', 'Dvar_WriteSingleVariable'),
    ('qcommon/common.cpp', 'Com_WriteConfigToFile'),
    ('qcommon/common.cpp', 'Com_WriteConfiguration'),
    ('qcommon/common.cpp', 'Com_WriteConfig_f'),
    ('game_mp/g_scr_main_mp.cpp', 'GScr_SetMissionDvar_SP'),
    ('game_mp/g_scr_main_mp.cpp', 'GScr_UpdateGamerProfile_SP'),
    ('cgame/cg_weapons.cpp', 'CG_CalculateWeaponMovement_Landing'),
]]
out = root / 'build-nx/campaign-restart-progress-tests'
out.mkdir(parents=True, exist_ok=True)
source = out / 'campaign.cpp'
source.write_text(r"""
#include <cassert>
#include <bit>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#include <climits>
#ifndef __cdecl
#define __cdecl
#endif
const int SCRIPTINSTANCE_SERVER=0, G_FIND_CLASSNAME=0, SV_CMD_RELIABLE=0;
const int DVAR_TYPE_INT=0, DVAR_TYPE_STRING=1;
struct dvar_s {
 const char* name; int type=0,flags=1;
 struct { int integer=0; float value=0; bool enabled=false; const char* string=""; } current;
 std::string storage;
};
using dvar_t=dvar_s;
std::map<std::string,dvar_s> dvars;
dvar_s reload, delay, changeDelay, home;
dvar_s *g_reloading=&reload,*g_deathDelay=&delay,*g_changelevel_time=&changeDelay,*fs_homepath=&home;
struct cg_s { int time; int landTime; float landChange; };
struct gentity_s { int health=100; } player;
struct { int player=1; } scr_const;
struct { int time=0,savepersist=0,missionSuccess=0; char missionSuccessCinematic[64]{}; } level;
unsigned now=0;
unsigned Sys_Milliseconds() { return now; }
bool havePlayer=true,cheats=false,menuLevel=false;
bool checkpointAvailable=false;
bool G_CheckpointTryLoad_SP() { if(!checkpointAvailable)return false;return true; }
dvar_s zombie;
dvar_s* zombiemode=&zombie;
bool Com_IsMenuLevel(const char*) { return menuLevel; }
const char* Dvar_GetString(const char*) { return "vorkuta"; }
gentity_s* G_Find(void*,int,int) { return havePlayer?&player:nullptr; }
std::vector<std::string> commands,serverCommands,parameters;
unsigned dvar_modifiedFlags=0;
bool com_fullyInitialized=true;
bool mainThread=true;
void LiveStorage_OfflineStatsFrame() {}
int writes=0;
FILE* config=nullptr;
std::string configPath;
const char* va(const char* format,...) {
 static char buffer[2048]; va_list args;va_start(args,format);
 vsnprintf(buffer,sizeof(buffer),format,args);va_end(args);return buffer;
}
void Cbuf_AddText(int,const char* text) { commands.emplace_back(text); }
void SV_GameSendServerCommand(int,int,const char* text) { serverCommands.emplace_back(text); }
const dvar_s* Dvar_FindVar(const char* name) {
 auto it=dvars.find(name);return it==dvars.end()?nullptr:&it->second;
}
void Dvar_SetInt(dvar_s* dvar,int value) {
 if(dvar->current.integer!=value) dvar_modifiedFlags|=dvar->flags;
 dvar->current.integer=value;
}
void Dvar_SetIntByName(const char* name,int value) { Dvar_SetInt(&dvars.at(name),value); }
void Dvar_SetStringByName(const char* name,const char* value) {
 auto& d=dvars.at(name);
 if(d.storage!=value) dvar_modifiedFlags|=d.flags;
 d.storage=value;d.current.string=d.storage.c_str();
}
bool Dvar_GetBool(const char*) { return cheats; }
const char* Dvar_GetVariantString(const char* name) {
 const auto& d=dvars.at(name);
 return d.type==DVAR_TYPE_STRING?d.current.string:va("%d",d.current.integer);
}
const char* Dvar_DisplayableLatchedValue(const dvar_s* d) { return Dvar_GetVariantString(d->name); }
int I_strcmp(const char* a,const char* b) { return strcmp(a,b); }
int I_stricmp(const char* a,const char* b) { return strcmp(a,b); }
void I_strncpyz(char* out,const char* in,size_t size) { snprintf(out,size,"%s",in); }
void Com_Printf(int,const char*,...) { }
int Scr_GetNumParam(int) { return (int)parameters.size(); }
const char* Scr_GetString(unsigned i,int) { return parameters.at(i).c_str(); }
int Scr_GetInt(unsigned i,int) { return std::stoi(parameters.at(i)); }
float Scr_GetFloat(unsigned i,int) { return std::stof(parameters.at(i)); }
void Scr_Error(const char* message,int) { throw std::runtime_error(message); }
void Scr_ParamError(int,const char* message,int) { throw std::runtime_error(message); }
int Cmd_Argc() { return (int)parameters.size(); }
const char* Cmd_Argv(int i) { return parameters.at(i).c_str(); }
void Com_DefaultExtension(char*,unsigned,const char*) { }
int FS_FOpenFileWriteToDir(char* filename,char* directory,const char*) {
 assert(mainThread && "config writes must not run on the script/server thread");
 assert(!strcmp(filename,"config.cfg") && !strcmp(directory,"players"));
 config=fopen(configPath.c_str(),"wb");assert(config);++writes;return 1;
}
void FS_Printf(int,char* format,...) {
 va_list args;va_start(args,format);vfprintf(config,format,args);va_end(args);
}
void FS_FCloseFile(int) { fclose(config);config=nullptr; }
bool IsDedicatedServer() { return false; }
void Key_WriteBindings(int,int) { }
void Gamepad_WriteBindings(int,int) { }
void Con_WriteFilterConfigString(int) { }
void Dvar_WriteSingleVariable(const dvar_s*,int*);
void Dvar_WriteVariables(int f) {
 for(const auto& [name,dvar]:dvars) Dvar_WriteSingleVariable(&dvar,&f);
}
""" + state + '\n' + '\n'.join(functions) + r"""
void reset(unsigned time=0) {
 now=time; commands.clear();serverCommands.clear();level={};
 havePlayer=true;player.health=100;delay.current.integer=200;
 GScr_ResetChangeLevel_SP();
}
void setMission(const char* name,const char* value) {
 parameters={name,value};GScr_SetMissionDvar_SP();
}
int main(int argc,char** argv) {
 assert(argc==2);configPath=argv[1];
 reload.flags=0; delay.flags=0;
 dvars["mis_01"].name="mis_01";
 dvars["mis_difficulty"].name="mis_difficulty";
 dvars["mis_difficulty"].type=DVAR_TYPE_STRING;
 dvars["unrelated"].name="unrelated";
 // Timer, death fade, duplicate requests, and one complete restart on expiry.
 reset(100);GScr_ReloadPlayer_SP();
 assert(reload.current.integer==1 && s_spChangeLevel.deathReloadTime==1300);
 size_t sends=serverCommands.size();GScr_ReloadPlayer_SP();
 assert(serverCommands.size()==sends);
 now=1300;GScr_UpdateChangeLevel_SP();assert(commands.empty());
 now=1301;GScr_UpdateChangeLevel_SP();
 assert((commands==std::vector<std::string>{"fast_restart\n"}));
 GScr_UpdateChangeLevel_SP();assert(commands.size()==1);
 // A valid checkpoint prevents the full-map fallback.
 reset(100);checkpointAvailable=true;GScr_ReloadPlayer_SP();now=1301;GScr_UpdateChangeLevel_SP();assert(commands.empty());checkpointAvailable=false;
 // Mission failure uses the same timer; missing players don't schedule reloads.
 reset(200);GScr_MissionFailed_SP();
 assert(s_spChangeLevel.missionFailed==1 && s_spChangeLevel.deathReloadTime==1400);
 now=1401;GScr_UpdateChangeLevel_SP();assert(commands[0]=="fast_restart\n");
 reset();havePlayer=false;GScr_MissionFailed_SP();
 now=5000;GScr_UpdateChangeLevel_SP();assert(commands.empty());
 // Wrap-safe wall-clock timing, and reset cancels pending reloads on a new map.
 reset(UINT_MAX-100);delay.current.integer=0;GScr_ReloadPlayer_SP();
 now=900;GScr_UpdateChangeLevel_SP();assert(commands.size()==1);
 reset();GScr_ReloadPlayer_SP();GScr_ResetChangeLevel_SP();
 now=5000;GScr_UpdateChangeLevel_SP();assert(commands.empty());
 // Ordinary successful transitions remain separate from death restart.
 reset();parameters={"vorkuta"};GScr_ChangeLevel_SP();GScr_UpdateChangeLevel_SP();
 level.time=1001;GScr_UpdateChangeLevel_SP();assert(commands[0]=="spmap vorkuta\n");
 reset();cheats=true;parameters={"rebirth"};GScr_ChangeLevel_SP();GScr_UpdateChangeLevel_SP();
 level.time=1001;GScr_UpdateChangeLevel_SP();assert(commands[0]=="spdevmap rebirth\n");
 // Ordinary solo death must arm recovery through the actual per-frame
 // observer, without the missing script missionfailed() call.
 reset(100);GScr_UpdateChangeLevel_SP();player.health=0;GScr_UpdateChangeLevel_SP();
 assert(s_spChangeLevel.deathReloadTime==1300);
 now=1301;GScr_UpdateChangeLevel_SP();assert(commands.size()==1 && commands[0]=="fast_restart\n");
 GScr_UpdateChangeLevel_SP();assert(commands.size()==1);
 reset();player.health=0;GScr_UpdateChangeLevel_SP();assert(s_spChangeLevel.deathReloadTime==0);
 reset();zombie.current.enabled=true;GScr_UpdateChangeLevel_SP();player.health=0;GScr_UpdateChangeLevel_SP();assert(s_spChangeLevel.deathReloadTime==0);zombie.current.enabled=false;
 reset();menuLevel=true;GScr_UpdateChangeLevel_SP();player.health=0;GScr_UpdateChangeLevel_SP();assert(s_spChangeLevel.deathReloadTime==0);menuLevel=false;
 reset();
 // The script's legitimate mission keys serialize via the real archived-dvar
 // writer and config flush. These are unlock/difficulty data, not checkpoints.
 setMission("mis_01","2");setMission("mis_difficulty","1000000000000000000000000");
 commands.clear();mainThread=false;
 GScr_UpdateGamerProfile_SP();assert(writes==0 && (dvar_modifiedFlags&1));
 assert((commands==std::vector<std::string>{"writeconfig config.cfg\n"}));
 // Dispatch the queued console command on the main thread using its actual
 // handler. A direct script-side config call would trip the assertion above.
 mainThread=true;parameters={"writeconfig","config.cfg"};Com_WriteConfig_f();
 assert(writes==1);
 Com_WriteConfiguration(0);assert(writes==2 && !(dvar_modifiedFlags&1));
 Com_WriteConfiguration(0);assert(writes==2);
 std::ifstream file(configPath);std::string saved((std::istreambuf_iterator<char>(file)),{});
 assert(saved.find("seta mis_01 \"2\"")!=std::string::npos);
 assert(saved.find("seta mis_difficulty \"1000000000000000000000000\"")!=std::string::npos);
 mainThread=false;GScr_UpdateGamerProfile_SP();assert(writes==2);
 mainThread=true;
 // Whitelist rejects otherwise valid unrelated dvars without modifying them.
 bool rejected=false;
 try { setMission("unrelated","99"); } catch(const std::runtime_error&) { rejected=true; }
 assert(rejected && dvars.at("unrelated").current.integer==0);
 // Missing/invalid types remain script errors.
 dvars.at("mis_01").type=99;rejected=false;
 try { setMission("mis_01","3"); } catch(const std::runtime_error&) { rejected=true; }
 assert(rejected);dvars.erase("mis_01");rejected=false;
 try { setMission("mis_01","3"); } catch(const std::runtime_error&) { rejected=true; }
 assert(rejected);
 // Rewinding before a landing must not extrapolate the weapon upward.
 cg_s view{7000, 10000, -24.0f};
 for (const int elapsed : {-3000, 0, 75, 150, 300, 450}) {
     view.time = view.landTime + elapsed;
     float origin[3] = {1.0f, 2.0f, 64.0f};
     CG_CalculateWeaponMovement_Landing(&view, origin);
     const float expected = elapsed == 150 ? 58.0f : (elapsed == 75 || elapsed == 300 ? 61.0f : 64.0f);
     assert(origin[0] == 1.0f && origin[1] == 2.0f && origin[2] == expected);
 }
 puts("PASS: campaign death/failure restart, timers, map transitions and main-thread progress writes");
 puts("PASS: viewmodel landing dip and recovery ignore future landing timestamps");
}
""")
for platform, flags in [('pc', []), ('nx', ['-DKISAK_NX'])]:
    exe = out / ('campaign-' + platform + '.exe')
    subprocess.run([args.compiler, '-std=c++20', '-DKISAK_SP', '-O2', *flags,
                    str(source), '-o', str(exe)], check=True)
    subprocess.run([str(exe), str(out / ('config-' + platform + '.cfg'))], check=True)
