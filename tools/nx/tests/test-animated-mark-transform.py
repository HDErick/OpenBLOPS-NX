"""Exercise actual animated-decal matrix code, including the exploding-car MP regression."""
from pathlib import Path
import argparse
import os
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--compiler', default='C:/msys64/ucrt64/bin/g++.exe')
p.add_argument('--baseline', help='Git revision whose MP code must fail the same test')
a = p.parse_args()
r = Path(__file__).resolve().parents[3]
out = r / 'build-nx/animated-mark-tests'
out.mkdir(parents=True, exist_ok=True)


def function(source, signature):
    start = source.index(signature)
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


marks = (r / 'src/EffectsCore/fx_marks.cpp').read_text()
if a.baseline:
    marks = subprocess.check_output(['git', 'show', a.baseline + ':src/EffectsCore/fx_marks.cpp'], cwd=r, text=True)
code = r'''
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#define __cdecl
#define __debugbreak() abort()
#define LODWORD(x) (*reinterpret_cast<const uint32_t *>(&(x)))
template<class... Args> bool Assert_MyHandler(Args...) { abort(); }
struct DObjAnimMat { float quat[4],trans[3],transWeight; };
struct alignas(16) DObjSkelMat { float axis[3][4],origin[4]; };
static_assert(sizeof(DObjSkelMat)==64);
struct XModel { int numBones; DObjAnimMat *base; };
struct DObj { int count; XModel *models[3]; };
struct FxMark { struct { unsigned char modelTypeAndSurf=0xC0,lmapIndex=0; } context; };
int DObjGetNumModels(const DObj *d) { return d->count; }
XModel *DObjGetModel(const DObj *d,int n) { assert(n>=0&&n<d->count); return d->models[n]; }
int XModelNumBones(const XModel *m) { return m->numBones; }
const DObjAnimMat *XModelGetBasePose(const XModel *m) { return m->base; }
'''
for sig in ['void __cdecl ConvertQuatToSkelMat(', 'void __cdecl DObjSkelMatToMatrix43(',
            'void __cdecl ConvertQuatToInverseSkelMat(']:
    code += function(marks, sig) + '\n'
code += function((r / 'src/universal/com_math.cpp').read_text(), 'void __cdecl MatrixMultiply43(') + '\n'
code += function(marks, 'void    FX_GenerateMarkVertsForMark_MatrixFromAnim(') + '\n'
code += r'''
DObjAnimMat pose(float angle,float x,float y,float z) {
 DObjAnimMat m{};m.quat[2]=std::sin(angle*.5f);m.quat[3]=std::cos(angle*.5f);
 m.trans[0]=x;m.trans[1]=y;m.trans[2]=z;m.transWeight=2;return m;
}
void close(float x,float y) { assert(std::isfinite(x)&&std::abs(x-y)<.001f); }
int main() {
 constexpr float pi=3.14159265358979323846f;
 DObjAnimMat b0[2]{pose(0,0,0,0),pose(0,0,0,0)};
 DObjAnimMat b1[3]{pose(0,0,0,0),pose(0,0,0,0),pose(0,0,0,0)};
 XModel models[2]{{2,b0},{3,b1}}; DObj obj{2,{&models[0],&models[1]}};
 DObjAnimMat anim[5];for(auto &m:anim)m=pose(0,1000,2000,3000);
 float viewOffset[3]{20000,-30000,40000};
 // Exercise every bone of two models. Offset into the concatenated animated
 // skeleton must differ from the per-model base-pose bone index.
 for(int iteration=0;iteration<1000;++iteration)for(int model=0;model<2;++model)
  for(int bone=0;bone<models[model].numBones;++bone) {
   int offset=(model?2:0)+bone;
   float baseAngle=(iteration%4)*pi/2,angle=((iteration+1)%4)*pi/2;
   auto &base=models[model].base[bone];base=pose(baseAngle,3+bone,5+model,7);
   anim[offset]=pose(angle,50+offset,60+offset,70+offset);
   FxMark mark;mark.context.modelTypeAndSurf=0xC0|model;mark.context.lmapIndex=bone;
   struct Guarded { uint32_t before[4];float matrix[4][3];uint32_t after[4]; } result;
   for(int i=0;i<4;++i)result.before[i]=result.after[i]=0x13579bdf;
   FX_GenerateMarkVertsForMark_MatrixFromAnim(&mark,&obj,anim,viewOffset,result.matrix);
   for(int i=0;i<4;++i)assert(result.before[i]==0x13579bdf&&result.after[i]==0x13579bdf);
   // A point in bind-model space must rotate/translate with its animated bone.
   // This independent 2D calculation also catches wrong multiplication order.
   float point[3]{base.trans[0]+std::cos(baseAngle)*2-std::sin(baseAngle)*3,
                  base.trans[1]+std::sin(baseAngle)*2+std::cos(baseAngle)*3,base.trans[2]+4};
   float expected[3]{anim[offset].trans[0]+std::cos(angle)*2-std::sin(angle)*3,
                     anim[offset].trans[1]+std::sin(angle)*2+std::cos(angle)*3,anim[offset].trans[2]+4};
   for(int k=0;k<3;++k)close(point[0]*result.matrix[0][k]+point[1]*result.matrix[1][k]+
                           point[2]*result.matrix[2][k]+result.matrix[3][k],expected[k]);
  }
 puts("PASS: 5000 animated decal transforms, multiple models/bones, inverse bind pose, rotations/translations and output guards");
}
'''
env = os.environ.copy()
env['PATH'] = str(Path(a.compiler).parent) + os.pathsep + env.get('PATH', '')
configs = [('pc-mp', ['KISAK_MP']), ('nx-mp', ['KISAK_MP','KISAK_NX']),
           ('pc-sp', ['KISAK_SP']), ('nx-sp', ['KISAK_SP','KISAK_NX'])]
if a.baseline:
    configs = configs[:1]
for name, defines in configs:
    cpp = out / (('baseline-' if a.baseline else '') + name + '.cpp')
    exe = cpp.with_suffix('.exe')
    cpp.write_text(code)
    subprocess.run([a.compiler,'-std=c++17','-O0','-fstack-protector-all','-fno-strict-aliasing',
                    '-fpermissive','-w',*[('-D'+d) for d in defines],str(cpp),'-o',str(exe)], check=True,env=env)
    run = subprocess.run([str(exe)],env=env,capture_output=True,text=True)
    if a.baseline:
        assert run.returncode != 0, 'Baseline unexpectedly passed; regression not reproduced'
        print(f'PASS: pre-fix MP decal code reproduces failure (exit {run.returncode:#x})')
    else:
        print(run.stdout,end='')
        if run.returncode:
            print(run.stderr)
            run.check_returncode()
        print(name+': passed',flush=True)
