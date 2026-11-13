"""Exercise native light interpolation with the logged PC cone failure and quantized snapshot endpoints."""
from pathlib import Path
import argparse,subprocess
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--compiler',default='/c/msys64/ucrt64/bin/g++.exe');args=p.parse_args()
root=Path(__file__).resolve().parents[3];out=root/'build-nx/primary-light-tests';out.mkdir(parents=True,exist_ok=True)
def function(sig):
 s=(root/'src/cgame_mp/cg_ents_mp.cpp').read_text();a=s.index(sig);b=s.index('{',a)+1;depth=1
 while depth:depth+=(s[b]=='{')-(s[b]=='}');b+=1
 return s[a:b]
code=r"""
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#define __cdecl
#define __int8 char
#define __debugbreak() assert(false)
using byte=unsigned char;
struct trajectory_t{};
union LightUnion{
 struct{byte colorAndExp[4];float intensity,radius,cosHalfFovOuter,cosHalfFovInner;}primaryLight;
 struct{float gunAngles[3];int ownerNum;float heatVal;}turret;
 LightUnion(){std::memset(this,0,sizeof(*this));}
};
struct LerpState{LightUnion u;trajectory_t apos,pos;};
struct centity_s{struct{int eType=10;struct{unsigned brushmodel=1;}index;LerpState lerp;}nextState;LerpState currentState;};
struct GfxLight{float color[3]{},diffuseColor[4]{},specularColor[4]{},dir[3]{},angles[3]{},origin[3]{},radius=0,cosHalfFovOuter=0,cosHalfFovInner=0;int exponent=0;struct{float m[4][4];}viewMatrix;};
struct ComPrimaryLight{float rotationLimit=1,translationLimit=0;}reference;
struct snapshot_s{int serverTime=0;};
struct cg_s{snapshot_s *snap=nullptr,*nextSnap=nullptr;int time=0;float frameInterpolation=0;struct{GfxLight primaryLights[2];}refdef;}game;
cg_s *CG_GetLocalClientGlobals(int n){assert(n==0);return &game;}
unsigned Com_GetPrimaryLightCount(){return 2;}const ComPrimaryLight *Com_GetPrimaryLight(unsigned n){assert(n==1);return &reference;}
template<class...T>bool Assert_MyHandler(T...){return false;}const char *va(const char*f,...){return f;}
void Byte4UnpackRgba(const byte*b,float*f){for(int i=0;i<4;++i)f[i]=b[i]/255.0f;}
void Vec3Lerp(const float*a,const float*b,float t,float*c){for(int i=0;i<3;++i)c[i]=a[i]+t*(b[i]-a[i]);}
void BG_EvaluateTrajectory(const trajectory_t*,int,float*){assert(false);}void AngleVectors(const float*,float*,void*,void*){assert(false);}
void SpotLightViewMatrix(const float*,float,float(*)[4]){assert(false);}
void CG_ClampPrimaryLightDir(GfxLight*,const ComPrimaryLight*){assert(false);}void CG_ClampPrimaryLightOrigin(GfxLight*,const ComPrimaryLight*){assert(false);}
"""
code+=function('void __cdecl CG_SetFrameInterpolation(')+'\n'
code+=function('void __cdecl CG_PrimaryLight(')+'\n'
code+=r"""
int main(){
 centity_s entity;auto &old=entity.currentState.u.primaryLight;auto &next=entity.nextState.lerp.u.primaryLight;
 old.colorAndExp[0]=next.colorAndExp[0]=255;old.colorAndExp[3]=4;next.colorAndExp[3]=8;
 old.intensity=1;next.intensity=2;old.radius=100;next.radius=200;
 old.cosHalfFovOuter=next.cosHalfFovOuter=.976296f;old.cosHalfFovInner=.999f;next.cosHalfFovInner=1;
 snapshot_s a{1000},b{1100};game.snap=&a;game.nextSnap=&b;game.time=1136;
 CG_SetFrameInterpolation(0);assert(std::abs(game.frameInterpolation-1.36f)<.00001f);
 float original=old.cosHalfFovInner+(next.cosHalfFovInner-old.cosHalfFovInner)*game.frameInterpolation;
 assert(original>1&&std::abs(original-1.00036f)<.00001f); // matches assert_18172.log
 CG_PrimaryLight(0,&entity);auto &light=game.refdef.primaryLights[1];
 assert(light.cosHalfFovInner==1&&light.cosHalfFovOuter==.976296f);
 assert(light.radius==200&&light.color[0]==2&&light.exponent==8);
 assert(game.frameInterpolation>1.35f); // do not alter global movement interpolation
 game.time=1050;CG_SetFrameInterpolation(0);CG_PrimaryLight(0,&entity);
 assert(light.radius==150&&light.color[0]==1.5f&&light.exponent==6);
 assert(std::abs(light.cosHalfFovInner-.9995f)<.00001f);
 // Quantization can reverse nearly equal cone endpoints. Repair even on PC.
 old.cosHalfFovInner=next.cosHalfFovInner=.97f;CG_PrimaryLight(0,&entity);
 assert(light.cosHalfFovOuter>0&&light.cosHalfFovOuter<light.cosHalfFovInner&&light.cosHalfFovInner<=1);
 // Out-of-range endpoint, boundary and rounding cases stay valid.
 for(float outer:{0.0f,-.001f,.99999f,1.0f,1.001f})
  for(float inner:{-.1f,.98f,1.0f,1.00036f}){
   old.cosHalfFovOuter=next.cosHalfFovOuter=outer;old.cosHalfFovInner=next.cosHalfFovInner=inner;
   CG_PrimaryLight(0,&entity);assert(light.cosHalfFovOuter>0&&light.cosHalfFovOuter<light.cosHalfFovInner&&light.cosHalfFovInner<=1);
  }
 game.frameInterpolation=-.1f;old.radius=100;next.radius=200;CG_PrimaryLight(0,&entity);assert(light.radius==100);
 puts("Native primary-light snapshot overshoot and cone-boundary regression passed");
}
"""
code=code.replace('#include <cstring>','#include <cstring>\n#include <initializer_list>')
for mode,flags in [('pc-sp',['-DKISAK_SP']),('pc-mp',[]),('nx-sp',['-DKISAK_SP','-DKISAK_NX'])]:
 cpp=out/f'{mode}.cpp';cpp.write_text(code);exe=cpp.with_suffix('.exe');subprocess.run([args.compiler,'-std=c++17','-w',*flags,str(cpp),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
