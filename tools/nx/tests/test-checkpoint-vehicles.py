"""Offline regressions for vehicle and pose capture and retained campaign checkpoints."""
from pathlib import Path
import argparse
import os
import re
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--compiler', default='C:/msys64/ucrt64/bin/g++.exe')
a = p.parse_args()
root = Path(__file__).resolve().parents[3]

def block(text, start):
    begin = text.index('{', start)
    end, depth = begin + 1, 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

def function(path, name):
    text = (root / 'src' / path).read_text()
    match = re.search(r'^[^\n]*\b' + re.escape(name) + r'\([^;]*?\)(?:\s*:[^{]+)?\s*\{', text, re.M)
    assert match, name
    return block(text, match.start())

def declaration(path, name):
    text = (root / 'src' / path).read_text()
    match = re.search(r'^(?:struct|enum)[^\n{;]*\b' + name + r'\b[^\n{;]*', text, re.M)
    assert match, name
    return block(text, match.start()) + ';'

vehicle = (root / 'src/vehicle/nitrous_vehicle.h').read_text()
members = vehicle[vehicle.index('    WheelState m_wheel_state[4];'):vehicle.index('\n};', vehicle.index('    WheelState m_wheel_state[4];'))]
types = '\n'.join([
    declaration('qcommon/msg_mp.h', 'usercmd_s'),
    declaration('vehicle/nitrous_vehicle.h', 'WheelEffectState'),
    declaration('vehicle/nitrous_vehicle.h', 'WheelState'),
    declaration('vehicle/nitrous_vehicle_controller.h', 'VehicleStun'),
    declaration('vehicle/nitrous_vehicle_controller.h', 'NitrousVehicleController'),
    'struct alignas(16) NitrousVehicle { NitrousVehicle();\n' + members + '\n};',
])
types = types.replace('__declspec(align(2))', '').replace('unsigned __int16', 'std::uint16_t').replace('unsigned __int8', 'std::uint8_t').replace('__int32', 'int')
harness = r"""
#include <game_mp/checkpoint_fields.h>
#include <cassert>
#include <cstdio>
#include <cstdarg>
#include <new>
#include <string>
#include <vector>
#ifndef __thiscall
#define __thiscall
#endif
#define KISAK_SP
struct alignas(16) phys_vec3 {
 float x,y,z,w;
 phys_vec3() {}
 phys_vec3(float value):x(value),y(value),z(value),w(value) {}
};
struct phys_mat44 { phys_vec3 x,y,z,w; };
template<unsigned N>struct bitarray { unsigned array[(N+31)/32]; };
struct NitrousVehicle;struct VehicleParameter;struct gentity_s;struct vehicle_info_t;struct PhysObjUserData;
struct XModel;struct rigid_body_constraint_custom_orientation;
struct rigid_body_constraint_custom_path;struct rigid_body_constraint_wheel;
"""
harness += types
harness += '\n' + function('vehicle/nitrous_vehicle.cpp', 'NitrousVehicle::NitrousVehicle')
harness += '\n' + function('vehicle/nitrous_vehicle.cpp', 'NitrousVehicleController::Init')
harness += '\n#include <game_mp/checkpoint_nitrous_fields.inc>\n'
harness += r"""
using namespace checkpoint;
struct dvar_s { struct { bool enabled=false; } current; };
dvar_s on,off;
const dvar_s *enabled=&on,*automatic=&off,*probe=&off,*restoreTest=&off,*saveCycle=&off,*zombiemode=&off;
struct gentity_s { void* client=(void*)1;int health=100; } g_entities[1];
struct { int time=1000; } level;
Store store;Resources preparedResources,committedResources;
std::vector<std::uint8_t> preparedImage,committedImage,probeImage;
int nextSaveId=1,requestedId=0,preparedId=0,committedId=0;
bool requestPending=false,commitRequested=false,lastSaveSuccessful=false,restoreQueued=false;
unsigned mapGeneration=19;
std::string requestedName;
float preparedOrigin[3]{},committedOrigin[3]{};
std::vector<std::string> commands,logs;
constexpr int SV_CMD_RELIABLE=0;
const char* va(const char* format,...) {
 static char buffer[2048];va_list a;va_start(a,format);vsnprintf(buffer,sizeof(buffer),format,a);va_end(a);return buffer;
}
void Com_Printf(int,const char* format,...) {
 char buffer[2048];va_list a;va_start(a,format);vsnprintf(buffer,sizeof(buffer),format,a);va_end(a);logs.emplace_back(buffer);
}
void SV_GameSendServerCommand(int,int,const char*) {}
void Cbuf_AddText(int,const char* text) { commands.emplace_back(text); }
bool Com_IsMenuLevel(const char*) { return false; }
int Dvar_GetInt(const char*) { return 0; }
const char* Dvar_GetString(const char*) { return "cuba"; }
"""
for path, name in [
    ('game_mp/g_checkpoint_sp.cpp', 'ReportCheckpointSaveFailure_SP'),
    ('game_mp/g_checkpoint_sp.cpp', 'G_CheckpointRequest_SP'),
    ('game_mp/g_checkpoint_sp.cpp', 'G_CheckpointCommit_SP'),
    ('game_mp/g_checkpoint_sp.cpp', 'G_CheckpointSaveSuccessful_SP'),
    ('game_mp/g_checkpoint_sp.cpp', 'G_CheckpointTryLoad_SP'),
    ('game_mp/g_scr_main_mp.cpp', 'GScr_ContinueAfterDeath_SP'),
]:
    harness += '\n' + function(path, name)
pose_codec = function('game_mp/checkpoint_fake_fields.inc', 'FakePose')
general_start = pose_codec.index('    default:')
general_end = pose_codec.index('    v.Field("pose.constantSet.value"', general_start)
harness += '\n' + declaration('bgame/bg_local.h', 'CEntGeneral') + '\n'
harness += 'struct PoseFixture { CEntGeneral general; };\n'
harness += 'template<class V>void GeneralPose(V& v, PoseFixture& pose) {\n    switch (0) {\n'
harness += pose_codec[general_start:general_end] + '}\n'
harness += r"""
int main() {
 // General pose storage may contain another member's byte at the link offset.
 // Capture only the persistent geometry and let restored triggers link again.
 for (std::uint8_t linkByte : {1, 2}) {
  PoseFixture source{}, restored{};
  source.general.contents = 17;
  source.general.mins[0] = -32.0f;
  source.general.maxs[2] = 64.0f;
  std::memcpy(&source.general.linked, &linkByte, 1);
  restored.general.linked = true;
  Resources refs;
  std::vector<std::uint8_t> bytes;
  FieldWriter writer(bytes, refs);
  GeneralPose(writer, source);
  FieldReader reader(bytes, refs);
  GeneralPose(reader, restored);
  reader.Finish();
  assert(!restored.general.linked && restored.general.contents == 17);
  assert(restored.general.mins[0] == -32.0f && restored.general.maxs[2] == 64.0f);
 }
 puts("PASS: checkpoint preserves trigger geometry and rebuilds the link flag on restore");

 // Reproduce a vehicle allocated from reused, nonzero memory before unpause.
 for(unsigned poison:{0x02u,0x7fu,0xcdu,0xffu}) {
  alignas(NitrousVehicle) unsigned char storage[sizeof(NitrousVehicle)];
  memset(storage,poison,sizeof(storage));
  auto* vehicle=new(storage)NitrousVehicle;
  auto& controller=vehicle->mVehicleController;
  assert(!controller.m_cmd_valid&&!controller.m_stop_at_goal&&!controller.m_jump_button_pressed);
  assert(!controller.m_stun.m_changed_this_frame&&!vehicle->m_throttle_time_started);
  assert(controller.m_script_goal_position.x==0&&controller.m_stuck_position.z==0);
  assert(controller.m_script_goal_speed==0&&controller.m_cmd.serverTime==0);
  Resources refs;std::vector<std::uint8_t> bytes;
  FieldWriter writer(bytes,refs);NitroFields(writer,*vehicle);
  NitrousVehicle restored;FieldReader reader(bytes,refs);NitroFields(reader,restored);reader.Finish();
  std::vector<std::uint8_t> again;FieldWriter rewrite(again,refs);NitroFields(rewrite,restored);
  assert(bytes==again);
  controller.m_jump_button_pressed=true;controller.m_stun.m_changed_this_frame=true;
  controller.m_stun.m_stun_time=9;controller.Init();
  assert(controller.m_cmd_valid&&!controller.m_jump_button_pressed&&!controller.m_stun.m_changed_this_frame&&controller.m_stun.m_stun_time==0);
  // An invalid live bool is rejected before it can enter an archive.
  unsigned char bad=205;memcpy(&controller.m_jump_button_pressed,&bad,1);
  bool rejected=false;
  try { std::vector<std::uint8_t> invalid;FieldWriter w(invalid,refs);NitroFields(w,*vehicle); }
  catch(const std::runtime_error& error) { rejected=std::string(error.what()).find("NitrousVehicleController.m_jump_button_pressed=205")!=std::string::npos; }
  assert(rejected);
  vehicle->~NitrousVehicle();
 }
 // A corrupt archive bool is also rejected with its actual field name.
 Resources refs;std::vector<std::uint8_t> boolBytes;
 bool flag=true;FieldWriter bw(boolBytes,refs);bw.Field("vehicle.flag",flag);boolBytes.back()=17;
 bool rejected=false;try{FieldReader r(boolBytes,refs);r.Field("vehicle.flag",flag);}
 catch(const std::runtime_error& error){rejected=std::string(error.what()).find("vehicle.flag=17")!=std::string::npos;}
 assert(rejected);
 puts("PASS: inactive/reused vehicle memory, full vehicle-field roundtrip, controller reactivation, named boolean validation");

 on.current.enabled=true;
 // The first attempted save fails, so no previous checkpoint can be loaded.
 int first=G_CheckpointRequest_SP("first",false);assert(first>0);
 ReportCheckpointSaveFailure_SP("injected vehicle capture error");
 assert(!G_CheckpointSaveSuccessful_SP()&&!G_CheckpointCommit_SP(first));
 GScr_ContinueAfterDeath_SP();assert(commands.back()=="fast_restart\n");commands.clear();
 // Successfully commit a real archive and its companion resource/origin data.
 Image image;image.identity={11,22,2};image.time=123456;
 for(unsigned i=0;i<6;++i)image.sections.push_back({static_cast<SectionId>(i),{1,2,3}});
 int saved=G_CheckpointRequest_SP("valid",false);assert(saved>first);
 assert(store.Prepare(image,requestedName));std::string error;
 assert(Encode(image,preparedImage,error));preparedId=saved;requestPending=false;
 int owner=7;preparedResources.Intern(ResourceKind::Entity,&owner);preparedOrigin[0]=321;
 assert(G_CheckpointCommit_SP(saved));const auto good=committedImage;
 // An incomplete later candidate must not replace that successful checkpoint.
 int failed=G_CheckpointRequest_SP("later",false);assert(failed>saved);
 Image incomplete=image;incomplete.sections.pop_back();assert(!store.Prepare(incomplete,error));
 ReportCheckpointSaveFailure_SP(error.c_str());
 assert(!G_CheckpointCommit_SP(failed)&&committedId==saved&&committedImage==good);
 assert(committedResources.Resolve(ResourceKind::Entity,1)==&owner&&committedOrigin[0]==321);
 Image retained;assert(store.Read(image.identity,retained,error)&&retained.time==123456);
 GScr_ContinueAfterDeath_SP();assert(commands.size()==1&&commands[0]=="checkpoint_restore_main 19\n");
 assert(logs.back().find("CHECKPOINT_LOAD_SELECTED id="+std::to_string(saved))!=std::string::npos);
 GScr_ContinueAfterDeath_SP();assert(commands.size()==1); // One queued restore only.
 puts("PASS: actual SP commit/failure/death handlers retain and select the previous working checkpoint");
}
"""
out = root / 'build-nx/checkpoint-vehicle-tests'
out.mkdir(parents=True, exist_ok=True)
cpp = out / 'vehicles.cpp'
cpp.write_text(harness)
exe = out / 'vehicles.exe'
env = os.environ.copy()
env['PATH'] = str(Path(a.compiler).parent) + os.pathsep + env.get('PATH', '')
subprocess.run([a.compiler, '-std=c++20', '-O2', '-I'+str(root/'src'), str(cpp),
                str(root/'src/game_mp/checkpoint_archive.cpp'), '-o', str(exe)], check=True, env=env)
subprocess.run([str(exe)], check=True, env=env)
