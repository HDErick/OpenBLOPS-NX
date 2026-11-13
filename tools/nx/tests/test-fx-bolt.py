"""Check SP use-count and MP origin-sentinel packing in the engine's FX bolt union."""
from pathlib import Path
import argparse
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler', default='/ucrt64/bin/g++.exe')
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
output = root / 'build-nx/fx-bolt-tests'
output.mkdir(parents=True, exist_ok=True)
header = (root / 'src/gfx_d3d/fxprimitives.h').read_text()
union = header[header.index('union FxBoltAndSortOrder'):header.index('struct FxSpatialFrame')]
source = output / 'bolt.cpp'
source.write_text('#include <cassert>\n#include <cstring>\n#define __int32 int\n' + union + r'''
int main() {
    static_assert(sizeof(FxBoltAndSortOrder) == 4);
    FxBoltAndSortOrder bolt = {};
    bolt.dobjHandle = 123;
    bolt.type = 1;
#ifdef KISAK_SP
    for (unsigned count = 0; count < 4; ++count) {
        bolt.temporalBits = count;
        bolt.boneIndex = 255;
        assert(bolt.temporalBits == count && bolt.boneIndex == 255);
        unsigned bits;
        std::memcpy(&bits, &bolt, sizeof(bits));
        assert(((bits >> 19) & 3) == count && ((bits >> 21) & 255) == 255);
    }
#else
    bolt.temporalBits = 1;
    bolt.boneIndex = 511;
    assert(bolt.temporalBits == 1 && bolt.boneIndex == 511);
    unsigned bits;
    std::memcpy(&bits, &bolt, sizeof(bits));
    assert(((bits >> 19) & 1) == 1 && ((bits >> 20) & 511) == 511);
#endif
    assert(bolt.dobjHandle == 123 && bolt.type == 1);
    bolt = {};
    bolt.absDynEntId = 0x1fffff;
    bolt._type = 2;
    assert(bolt.absDynEntId == 0x1fffff && bolt.type == 2);
}
''')
for mode in ('SP', 'MP'):
    exe = output / (mode + '.exe')
    subprocess.run([args.compiler, '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    '-DKISAK_' + mode, str(source), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
    print('PASS:', mode, 'effect bolt packing and origin sentinel')

# Exercise the actual event-to-effect call, not just the packed representation.
event = (root / 'src/cgame/cg_event.cpp').read_text()
function = event[event.index('unsigned int __cdecl CG_PlayBoltedEffect('):event.index('void __cdecl CG_PlayFxOnTag(')]
constant_header = (root / 'src/EffectsCore/fx_system.h').read_text()
constant = next(line for line in constant_header.splitlines() if line.startswith('constexpr int FX_BONE_INDEX_NONE'))
call_source = output / 'origin-call.cpp'
call_source.write_text('#include <cassert>\n#undef __cdecl\n#define __cdecl\n#ifdef KISAK_SP\n' + constant + '\n#else\nconstexpr int FX_BONE_INDEX_NONE = 511;\n#endif\n' + r'''
struct FxEffectDef {};
struct cg_s { int time; };
static cg_s globals = { 1234 };
static bool found;
static int actualBone, actualTime;
static cg_s *CG_GetLocalClientGlobals(int) { return &globals; }
static bool CG_GetBoneIndex(int, int, unsigned int, unsigned char *index) {
    *index = 7;
    return found;
}
static unsigned FX_PlayBoltedEffect(int, const FxEffectDef *, int time, int, int bone) {
    actualBone = bone;
    actualTime = time;
    return bone == FX_BONE_INDEX_NONE || bone == 7 ? 42 : 0;
}
''' + function + r'''
int main() {
    FxEffectDef effect;
    found = false;
    assert(CG_PlayBoltedEffect(0, &effect, 1, 0) == 42);
    assert(actualBone == FX_BONE_INDEX_NONE && actualTime == globals.time);
    assert(CG_PlayBoltedEffect(0, &effect, 1, 99) == 42);
    assert(actualBone == FX_BONE_INDEX_NONE);
    found = true;
    assert(CG_PlayBoltedEffect(0, &effect, 1, 99) == 42);
    assert(actualBone == 7);
}
''')
for mode in ('SP', 'MP'):
    exe = output / ('origin-' + mode + '.exe')
    subprocess.run([args.compiler, '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    '-DKISAK_' + mode, str(call_source), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
    print('PASS:', mode, 'no-tag/missing-tag origin effects and valid bone effects')