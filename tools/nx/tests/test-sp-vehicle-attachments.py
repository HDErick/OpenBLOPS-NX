"""Compile real SP vehicle model builders; verify server/client tag parity."""
from pathlib import Path
import argparse
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler', default='/c/msys64/ucrt64/bin/g++.exe')
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]

def function(path, signature):
    source = (root / path).read_text()
    start = source.index(signature)
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

helper = function('src/game_mp/g_utils_mp.cpp',
                  'static unsigned int G_AddVehicleDefAttachments_SP(')
client = function('src/cgame_mp/cg_ents_mp.cpp',
                  'unsigned int __cdecl CG_AddVehicleAttachedModel(')
init = function('src/game/g_scr_vehicle.cpp',
                'void __cdecl VEH_InitModelAndValidateTags(')
out = root / 'build-nx/sp-vehicle-attachments-tests'
out.mkdir(parents=True, exist_ok=True)
source = out / 'vehicle.cpp'
source.write_text(r"""
#include <cassert>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#ifndef __cdecl
#define __cdecl
#endif
const int ET_VEHICLE=14, ET_VEHICLE_COLLMAP=16, SCRIPTINSTANCE_SERVER=0;
const int CS_TAGS=0, ERR_DROP=1;
struct XModel { std::string name; std::set<std::string> bones; };
struct DObjModel_s { XModel* model=nullptr; unsigned boneName=0; bool ignoreCollision=true; };
struct vehicle_info_t {
 char attachmentModels[4][64]{}, attachmentTags[4][64]{};
 char deathAttachmentModels[4][64]{}, deathAttachmentTags[4][64]{};
} defs[3];
struct State {
 int eType=0,number=132;
 struct { int vehicleInfoIndex=0,attachModelIndex[2]{},attachTagIndex[2]{}; } vehicleState;
 struct { unsigned eFlags2=0; } lerp;
};
struct gentity_s { State s; int model=1; struct { float currentOrigin[3]{}; } r; };
struct centity_s { State nextState; };
struct cgs_t { XModel* gameModels[16]{}; } cgs;
std::map<std::string,XModel> modelRegistry;
std::map<std::string,unsigned> strings;
std::vector<DObjModel_s> built;
std::vector<int> builtIndices;
bool forceInvalid=false;
int warnings=0;
const vehicle_info_t* BG_GetVehicleInfo(int index) {
 assert(index>=0 && index<3); return &defs[index];
}
XModel* SV_XModelGet(char* name) { return &modelRegistry.at(name); }
XModel* R_RegisterModel(char* name) { return SV_XModelGet(name); }
unsigned SL_FindString(const char* name,int) { return strings.at(name); }
const cgs_t* CG_GetLocalClientStaticGlobals(int) { return &cgs; }
const char* CL_GetConfigString(int) { return "body_animate_jnt"; }
void Com_PrintWarning(int,const char*,...) { ++warnings; }
""" + helper + r"""
bool hasTag(const std::vector<DObjModel_s>& models,const char* tag) {
 for (const auto& m:models) if (m.model->bones.count(tag)) return true;
 return false;
}
void G_DObjUpdate(gentity_s* ent) {
 builtIndices.push_back(ent->s.vehicleState.vehicleInfoIndex);
 DObjModel_s models[32];
 models[0].model=&modelRegistry.at(ent->model==1?"base":"defaultvehicle");
 unsigned count=G_AddVehicleDefAttachments_SP(ent,models,1);
 built.assign(models,models+count);
}
const char* s_defaultVehicleInfoName="default";
int VEH_GetVehicleInfoFromName(char*) { return 0; }
void G_SetModel(gentity_s* ent,char*) { ent->model=2; }
bool G_XModelBad(int model) { return model==3; }
void G_OverrideModel(int,char*) { }
const char* va(const char*,...) { return "unexpected model failure"; }
bool Assert_MyHandler(const char*,int,int,const char*,...) { throw std::runtime_error("assert"); }
void __debugbreak() { throw std::runtime_error("debugbreak"); }
unsigned G_ModelName(int) { return 1; }
char* SL_ConvertToString(unsigned,int) { static char name[]="base"; return name; }
void Com_Error(int,const char*) { throw std::runtime_error("missing required tag"); }
char VEH_DObjHasRequiredTags(gentity_s*,int index) {
 if (forceInvalid && index==1) return false;
 return index==0 || (hasTag(built,"tag_flash_gunner1") && hasTag(built,"tag_flash_gunner3"));
}
""" + init + '\n' + client + r"""
int main() {
 // Rebirth helicopter metadata: the base has gunner2, while guns and spotlight
 // own gunner1/3. The latter tag creates the mission's moving scan target.
 modelRegistry["base"]={"base",{"body_animate_jnt","tag_flash_gunner2"}};
 modelRegistry["guns"]={"guns",{"tag_flash_gunner1"}};
 modelRegistry["spotlight"]={"spotlight",{"tag_flash_gunner3"}};
 modelRegistry["wreck"]={"wreck",{"wreck_tag"}};
 modelRegistry["defaultvehicle"]={"defaultvehicle",{}};
 strings["body_animate_jnt"]=1; strings["wreck_mount"]=2;
 strcpy(defs[1].attachmentModels[0],"guns");
 strcpy(defs[1].attachmentModels[1],"spotlight");
 strcpy(defs[1].attachmentTags[0],"body_animate_jnt");
 strcpy(defs[1].attachmentTags[1],"body_animate_jnt");
 strcpy(defs[1].deathAttachmentModels[0],"wreck");
 strcpy(defs[1].deathAttachmentTags[0],"wreck_mount");
 gentity_s ent; int index=1;
 // Exercise real initial setup: state starts at default index, no scr_vehicle.
 VEH_InitModelAndValidateTags(&ent,&index);
 assert(index==1 && ent.model==1 && built.size()==3);
 assert((builtIndices==std::vector<int>{1}));
 assert(hasTag(built,"tag_flash_gunner1") && hasTag(built,"tag_flash_gunner3"));
 centity_s cent; cent.nextState=ent.s;
 DObjModel_s clientModels[32]; clientModels[0]=built[0];
 unsigned clientCount=CG_AddVehicleAttachedModel(&cent,clientModels,1,0);
 assert(clientCount==built.size());
 for(unsigned i=1;i<clientCount;++i) {
  assert(clientModels[i].model==built[i].model);
  assert(clientModels[i].boneName==built[i].boneName);
  assert(!built[i].ignoreCollision);
 }
 // Explicit attachments stay before definition submodels on both sides.
 cgs.gameModels[1]=&modelRegistry.at("wreck");
 cent.nextState.vehicleState.attachModelIndex[0]=1;
 assert(CG_AddVehicleAttachedModel(&cent,clientModels,1,0)==4);
 DObjModel_s serverModels[33]; serverModels[0]=built[0];
 serverModels[1]=clientModels[1];
 assert(G_AddVehicleDefAttachments_SP(&ent,serverModels,2)==4);
 for(unsigned i=1;i<4;++i) assert(serverModels[i].model==clientModels[i].model);
 // Wrecks, collmap vehicles and model-count limit.
 ent.s.lerp.eFlags2=0x02000000u;
 ent.s.eType=ET_VEHICLE_COLLMAP;
 assert(G_AddVehicleDefAttachments_SP(&ent,serverModels,1)==2);
 assert(serverModels[1].model==&modelRegistry.at("wreck"));
 assert(serverModels[1].boneName==2);
 ent.s.lerp.eFlags2=0;
 serverModels[32].model=&modelRegistry.at("wreck");
 assert(G_AddVehicleDefAttachments_SP(&ent,serverModels,31)==32);
 assert(serverModels[32].model==&modelRegistry.at("wreck") && warnings==1);
 assert(G_AddVehicleDefAttachments_SP(&ent,serverModels,32)==32);
 ent.s.eType=0;
 assert(G_AddVehicleDefAttachments_SP(&ent,serverModels,1)==1);
 ent.s.eType=ET_VEHICLE; ent.s.vehicleState.vehicleInfoIndex=0;
 assert(G_AddVehicleDefAttachments_SP(&ent,serverModels,1)==1);
 // Fallback must select the default definition before rebuilding, even if the
 // entity previously had helicopter attachments. Keep strict validation.
 forceInvalid=true; builtIndices.clear(); ent.model=1; index=1;
 VEH_InitModelAndValidateTags(&ent,&index);
 assert(index==0 && ent.model==2 && built.size()==1);
 assert((builtIndices==std::vector<int>{1,0}));
 forceInvalid=false; builtIndices.clear(); index=0;
 VEH_InitModelAndValidateTags(&ent,&index);
 assert((builtIndices==std::vector<int>{0}));
 puts("PASS: SP vehicle server/client tags, initial build, wrecks, bounds and fallback");
}
""")
for platform, flags in [('pc', []), ('nx', ['-DKISAK_NX'])]:
    exe = out / ('vehicle-' + platform + '.exe')
    subprocess.run([args.compiler, '-std=c++17', '-DKISAK_SP', '-O2', *flags,
                    str(source), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
