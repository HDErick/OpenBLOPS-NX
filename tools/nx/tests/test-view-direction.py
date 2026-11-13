"""Keep SP ground tilt separate from link/body angles when calculating use and aim."""
from pathlib import Path
import argparse
import re
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler', default='/ucrt64/bin/g++.exe')
parser.add_argument('--baseline', metavar='REF', help='Verify the old SP direction regression')
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
output = root / 'build-nx/view-direction-tests'
output.mkdir(parents=True, exist_ok=True)

def read(name):
    if args.baseline:
        return subprocess.check_output(['git', 'show', args.baseline + ':' + name], cwd=root, text=True, encoding='utf-8', errors='replace')
    return (root / name).read_text(encoding='utf-8', errors='replace')

def function(code, signature):
    start = code.index(signature)
    end = code.index('{', start) + 1
    depth = 1
    while depth:
        depth += (code[end] == '{') - (code[end] == '}')
        end += 1
    return code[start:end]

angles = read('src/universal/com_math_anglevectors.cpp')
math = read('src/universal/com_math.cpp')
view = function(read('src/bgame/bg_misc.cpp'), 'void __cdecl BG_GetPlayerViewDirection(')
msg_header = read('src/qcommon/msg_mp.h')
start = msg_header.index('#ifdef KISAK_SP\n#define NUM_CLIENT_STATE_FIELDS')
counts = msg_header[start:msg_header.index('#endif', start) + len('#endif')]
msg = read('src/qcommon/msg_mp.cpp')
start = msg.index('const int numPlayerStateFields =')
fields = msg[start:msg.index('\n};', start) + 3]
# The real table uses offsetof for these extension fields; ordinary shared entries
# retain their fixed offsets. The engine build checks the full player-state layout.
source = output / 'view.cpp'
source.write_text(r'''
#include <cassert>
#include <initializer_list>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#ifndef __cdecl
#define __cdecl
#endif
struct playerState_s {
    float viewangles[3], linkAngles[3], groundTiltAngles[3];
    int deltaTime, killCamEntity, killCamTargetEntity;
};
#ifdef KISAK_SP
#define PS_PM_FLAGS_NET_BITS 32
#define PS_WEAP_FLAGS_NET_BITS 26
#else
#define PS_PM_FLAGS_NET_BITS 25
#define PS_WEAP_FLAGS_NET_BITS 24
#endif
''' + counts + r'''
#ifndef NUM_PLAYER_STATE_FIELDS
#define NUM_PLAYER_STATE_FIELDS 179
#endif
struct NetField {
    const char *name; int offset, size, bits; unsigned changeHints;
    const char *bitsName, *changeName;
};
''' + fields + '\n' + function(angles, 'void __cdecl AngleVectors(') + '\n'
    + function(angles, 'void __cdecl AnglesToAxis(') + '\n'
    + function(math, 'void __cdecl MatrixMultiply(') + '\n' + view + r'''
static void same(const float *a, const float *b) {
    for (int i = 0; i < 3; ++i) assert(std::fabs(a[i] - b[i]) < 0.00001f);
}
static void compare_outputs(const playerState_s &ps, const float *f, const float *r, const float *u) {
    // Every optional-output combination is supported by the real helper.
    for (int mask = 0; mask < 8; ++mask) {
        float actualF[3]{}, actualR[3]{}, actualU[3]{};
        BG_GetPlayerViewDirection(&ps, mask & 1 ? actualF : nullptr,
            mask & 2 ? actualR : nullptr, mask & 4 ? actualU : nullptr);
        if (mask & 1) same(actualF, f);
        if (mask & 2) same(actualR, r);
        if (mask & 4) same(actualU, u);
    }
}
static void link_is_not_view_tilt() {
    for (float yaw : {0.0f, 90.0f, 113.18f, 180.0f, 270.0f}) {
        playerState_s ps{};
        ps.viewangles[0] = 12.10f; ps.viewangles[1] = yaw; ps.viewangles[2] = 5;
        float f[3], r[3], u[3]; AngleVectors(ps.viewangles, f, r, u);
        // Ascension's intro leaves a 90-degree link yaw after detaching.
        ps.linkAngles[1] = 90;
        compare_outputs(ps, f, r, u);
        ps.linkAngles[0] = 27; ps.linkAngles[1] = -135; ps.linkAngles[2] = 16;
        compare_outputs(ps, f, r, u);
    }
    // Recorded Quick Revive situation: aiming at the actual bounds center must
    // pass the 0.76 look-at threshold; aiming 90 degrees away must fail it.
    playerState_s ps{}; ps.linkAngles[1] = 90;
    const float offset[3] = {-183.55f + 101.60f, 829.65f - 709.71f, -459.50f + 425.88f};
    const float horizontal = std::hypot(offset[0], offset[1]);
    ps.viewangles[0] = -std::atan2(offset[2], horizontal) * 180.0f / 3.14159265358979323846f;
    ps.viewangles[1] = std::atan2(offset[1], offset[0]) * 180.0f / 3.14159265358979323846f;
    float f[3]; BG_GetPlayerViewDirection(&ps, f, nullptr, nullptr);
    const float length = std::sqrt(horizontal * horizontal + offset[2] * offset[2]);
    auto dot = [&] { return (offset[0]*f[0] + offset[1]*f[1] + offset[2]*f[2]) / length; };
    assert(dot() > 0.999f);
    ps.viewangles[1] -= 90;
    BG_GetPlayerViewDirection(&ps, f, nullptr, nullptr);
    assert(dot() < 0.76f);
}
static void real_ground_tilt() {
    playerState_s ps{}; ps.linkAngles[1] = 37;
    ps.groundTiltAngles[1] = 90;
    float expectedAngles[3] = {0, 90, 0}, f[3], r[3], u[3];
#ifdef KISAK_SP
    AngleVectors(expectedAngles, f, r, u);
#else
    AngleVectors(ps.viewangles, f, r, u);
#endif
    compare_outputs(ps, f, r, u);
    ps.groundTiltAngles[0] = 30; ps.groundTiltAngles[1] = 0; ps.groundTiltAngles[2] = 0;
#ifdef KISAK_SP
    expectedAngles[0] = 30; expectedAngles[1] = 0;
    AngleVectors(expectedAngles, f, r, u);
#endif
    compare_outputs(ps, f, r, u);
    ps.groundTiltAngles[0] = 0; ps.groundTiltAngles[2] = 45;
#ifdef KISAK_SP
    expectedAngles[0] = 0; expectedAngles[2] = 45;
    AngleVectors(expectedAngles, f, r, u);
#endif
    compare_outputs(ps, f, r, u);
}
static void snapshot_fields() {
    assert(numPlayerStateFields == NUM_PLAYER_STATE_FIELDS);
    assert(sizeof(playerStateFields) / sizeof(playerStateFields[0]) == NUM_PLAYER_STATE_FIELDS);
#ifdef KISAK_SP
    assert(NUM_PLAYER_STATE_FIELDS == 182);
    playerState_s original{}, decoded{};
    original.linkAngles[1] = 90;
    original.groundTiltAngles[0] = 12.5f; original.groundTiltAngles[1] = -20; original.groundTiltAngles[2] = 7;
    for (int i = 0; i < 3; ++i) {
        const NetField &field = playerStateFields[179 + i];
        char name[32]; std::snprintf(name, sizeof(name), "groundTiltAngles[%d]", i);
        assert(std::strcmp(field.name, name) == 0);
        assert(field.offset == offsetof(playerState_s, groundTiltAngles) + 4*i);
        assert(field.size == 4 && field.bits == 0 && field.changeHints == 0);
        std::memcpy(reinterpret_cast<char *>(&decoded) + field.offset,
            reinterpret_cast<char *>(&original) + field.offset, field.size);
    }
    same(original.groundTiltAngles, decoded.groundTiltAngles);
    assert(decoded.linkAngles[1] == 0);
#else
    assert(NUM_PLAYER_STATE_FIELDS == 179);
    for (const auto &field : playerStateFields) assert(std::strstr(field.name, "groundTiltAngles") == nullptr);
#endif
}
int main() {
    link_is_not_view_tilt(); real_ground_tilt(); snapshot_fields();
    puts("view direction, ground tilt, and SP/MP netfield checks passed");
}
''')
for name, defines in [('pc-sp', ['-DKISAK_SP']), ('nx-sp', ['-DKISAK_SP', '-DKISAK_NX']), ('mp', [])]:
    exe = output / (name + '.exe')
    subprocess.run([args.compiler, '-std=c++20', *defines, str(source), '-o', str(exe)], check=True)
    result = subprocess.run([str(exe)])
    if args.baseline:
        if name.endswith('-sp') and result.returncode == 0:
            raise SystemExit('Baseline unexpectedly passed the SP direction regression')
        if name == 'mp' and result.returncode != 0:
            raise SystemExit('Baseline MP behavior failed')
        print(name + ': expected baseline ' + ('failure' if result.returncode else 'pass'), flush=True)
    else:
        result.check_returncode()
        print(name + ': passed', flush=True)