"""Exercise actual vehicle-list lookup and input clearing with native pointers."""
from pathlib import Path
import argparse
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler', default='/ucrt64/bin/g++.exe')
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
output = root / 'build-nx/vehicle-list-tests'
output.mkdir(parents=True, exist_ok=True)
text = (root / 'src/vehicle/nitrous_vehicle.cpp').read_text()
lookup = text[text.index('NitrousVehicle *__cdecl NitrousVehicle::add_vehicle(int id)'):text.index('void __cdecl NitrousVehicle::reinit_parms()')]
clear = text[text.index('void __cdecl G_ClearVehicleInputs()'):text.index('void __cdecl CG_SetVehDriverInputs(')]
source = output / 'vehicle.cpp'
source.write_text(r'''
#include <cassert>
#include <cstring>
#include <vector>
#undef __cdecl
#define __cdecl
#define PROF_SCOPED(x) ((void)0)
#define __debugbreak() assert(false)
constexpr int ENTITYNUM_NONE = 1023;
constexpr int CRITSECT_PHYSICS_UPDATE = 1, CRITSECT_PHYSICS = 2;
struct Entity { void *scr_vehicle; int occupant; };
struct NitrousVehicle {
    Entity *m_owner = nullptr;
    struct { struct { int buttons, movement[3]; } m_cmd; } mVehicleController{};
    int id = -1, refcount = 0, m_flags = 1;
    static NitrousVehicle *add_vehicle(int id);
};
struct List {
    std::vector<NitrousVehicle *> entries;
    int m_list_count = 0, allocations = 0;
    auto begin() { return entries.begin(); }
    auto end() { return entries.end(); }
    NitrousVehicle *add(int, const char *) {
        ++allocations;
        auto *vehicle = new NitrousVehicle;
        entries.push_back(vehicle);
        m_list_count = int(entries.size());
        return vehicle;
    }
} g_rb_vehicle_list;
static int locks;
static void Sys_EnterCriticalSection(int) { ++locks; }
static void Sys_LeaveCriticalSection(int) { --locks; }
static int VEH_GetSeatOccupantEntNum(Entity *owner, int) { return owner->occupant; }
template<class... T> static bool _tlAssert(T...) { return true; }
template<class... T> static bool Assert_MyHandler(T...) { return true; }
''' + lookup + clear + r'''
int main() {
    G_ClearVehicleInputs();
    assert(locks == 0);
    auto *vehicle = NitrousVehicle::add_vehicle(12);
    assert(vehicle->id == 12 && vehicle->refcount == 1);
    assert(NitrousVehicle::add_vehicle(12) == vehicle);
    assert(vehicle->refcount == 2 && g_rb_vehicle_list.allocations == 1);
    auto *other = NitrousVehicle::add_vehicle(13);
    assert(other != vehicle && other->id == 13);
    assert(NitrousVehicle::add_vehicle(-1) != NitrousVehicle::add_vehicle(-1));
    Entity owner{reinterpret_cast<void *>(1), ENTITYNUM_NONE};
    vehicle->m_owner = &owner;
    vehicle->mVehicleController.m_cmd.buttons = 123;
    vehicle->mVehicleController.m_cmd.movement[2] = 456;
    other->mVehicleController.m_cmd.buttons = 789;
    G_ClearVehicleInputs();
    assert(locks == 0 && vehicle->mVehicleController.m_cmd.buttons == 0);
    assert(vehicle->mVehicleController.m_cmd.movement[2] == 0);
    assert(other->mVehicleController.m_cmd.buttons == 789);
    owner.occupant = 4;
    vehicle->mVehicleController.m_cmd.buttons = 42;
    G_ClearVehicleInputs();
    assert(vehicle->mVehicleController.m_cmd.buttons == 42 && locks == 0);
    owner.occupant = ENTITYNUM_NONE;
    owner.scr_vehicle = nullptr;
    G_ClearVehicleInputs();
    assert(vehicle->mVehicleController.m_cmd.buttons == 42 && locks == 0);
    for (auto *entry : g_rb_vehicle_list.entries) delete entry;
}
''')
for mode in ('SP', 'MP'):
    exe = output / (mode + '.exe')
    subprocess.run([args.compiler, '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    '-DKISAK_NX', '-DKISAK_' + mode, str(source), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
    print('PASS:', mode, 'vehicle lookup/reuse, anonymous allocations, empty/occupied/unoccupied input clearing')