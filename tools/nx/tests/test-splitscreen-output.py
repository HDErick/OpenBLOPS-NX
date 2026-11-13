"""Check split-view clear clipping and post-filter placement without launching the game."""
from pathlib import Path
import argparse
import subprocess
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler',default='/c/msys64/ucrt64/bin/g++.exe')
args=parser.parse_args()
root=Path(__file__).resolve().parents[3]
out=root/'build-nx/splitscreen-output-tests';out.mkdir(parents=True,exist_ok=True)
def function(path,signature):
    source=(root/path).read_text(encoding='utf-8',errors='replace')
    start=source.index(signature);end=source.index('{',start)+1;depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]
code=r'''
#include <cassert>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <vector>
#ifndef __cdecl
#define __cdecl
#endif
#define KISAK_EXPERIMENTAL_SPLITSCREEN 1
#define iassert(x) assert(x)
#define __debugbreak() assert(false)
struct GfxViewport {int x,y,width,height;};
struct FakeDevice {GfxViewport viewport{0,0,8,4};int updates=0;} device;
struct GfxCmdBufState {struct {FakeDevice *device=&::device;} prim;GfxViewport viewport{0,0,8,4};float depthRangeNear=0,depthRangeFar=1;} gfxCmdBufState;
struct {struct {int integer=0;} current;} logDvar;
auto r_logFile=&logDvar;
void RB_LogPrint(const char *){}
char *va(const char *,...){return nullptr;}
template<class...T> bool Assert_MyHandler(T...){return false;}
void R_HW_SetViewport(FakeDevice *d,const GfxViewport *v,float,float){d->viewport=*v;++d->updates;}
struct {bool splitscreen=true;} backend;
auto backEndData=&backend;
struct GfxViewInfo {struct {GfxViewport sceneViewport,displayViewport;} cullViewInfo;bool isMissileCamera=false;};
'''
code+=function('src/gfx_d3d/r_state.cpp','void __cdecl R_SetViewport(')+'\n'
code+=function('src/gfx_d3d/rb_draw3d.cpp','static const GfxViewport *RB_SetSceneClearViewport(')+'\n'
code+=r'''
enum GfxViewportBehavior {GFX_USE_VIEWPORT_FULL,GFX_USE_VIEWPORT_FOR_VIEW};
constexpr int VIEW_MODE_NONE=0,VIEW_MODE_2D=1,CONST_SRC_CODE_VIEWPORT_DIMENSIONS=1,GFX_PRIM_STATS_CODE=1;
struct GfxCmdBufSourceState {
    GfxViewport sceneViewport{0,0,1025,384};
    GfxViewportBehavior viewportBehavior=GFX_USE_VIEWPORT_FULL;
    int viewMode=VIEW_MODE_NONE,viewportIsDirty=0,renderTargetWidth=128,renderTargetHeight=96;
} gfxCmdBufSourceState;
struct {int displayWidth=1025,displayHeight=769;} vidConfig;
struct {int indexCount=0;} tess;
struct Material {} material;
struct Draw {GfxViewport viewport;float w,h,s0,t0,s1,t1;unsigned color;};
std::vector<Draw> draws;
float constants[4];
void RB_EndTessSurface(){tess.indexCount=0;}
void R_Set2D(GfxCmdBufSourceState *s){s->viewMode=VIEW_MODE_2D;s->viewportIsDirty=1;}
void R_UpdateCodeConstant(GfxCmdBufSourceState *,int,float s,float t,float ds,float dt){constants[0]=s;constants[1]=t;constants[2]=ds;constants[3]=dt;}
void RB_DrawStretchPic(const Material *,float x,float y,float w,float h,float s0,float t0,float s1,float t1,unsigned color,int){
    assert(x==0&&y==0&&gfxCmdBufSourceState.viewMode==VIEW_MODE_2D);
    GfxViewport viewport=gfxCmdBufSourceState.sceneViewport;
    if(gfxCmdBufSourceState.viewportBehavior==GFX_USE_VIEWPORT_FULL)
        viewport={0,0,gfxCmdBufSourceState.renderTargetWidth,gfxCmdBufSourceState.renderTargetHeight};
    draws.push_back({viewport,w,h,s0,t0,s1,t1,color});tess.indexCount=6;
}
void RB_SplitScreenTexCoords(float,float,float,float,float *,float *,float *,float *);
'''
code+=function('src/gfx_d3d/r_state.cpp','void __cdecl R_SetViewportStruct(')+'\n'
for sig in ['static GfxViewport RB_SplitFilterViewport(', 'static void RB_SharedSplitScreenFilter(',
    'void __cdecl RB_SplitScreenFilter(', 'void __cdecl RB_SplitScreenTexCoords(']:
    code+=function('src/gfx_d3d/rb_backend.cpp',sig)+'\n'
code+=r'''
bool same(const GfxViewport &a,const GfxViewport &b){return std::memcmp(&a,&b,sizeof(a))==0;}
int pixels[8][8];float depth[8][8];
void clear(const GfxViewport &rect){
    // IDirect3DDevice9::Clear clips pRects to the active device viewport.
    auto vp=device.viewport;
    for(int y=std::max(rect.y,vp.y);y<std::min(rect.y+rect.height,vp.y+vp.height);++y)
        for(int x=std::max(rect.x,vp.x);x<std::min(rect.x+rect.width,vp.x+vp.width);++x){pixels[y][x]=0;depth[y][x]=1;}
}
int main(){
    GfxViewInfo top{{{0,0,8,4},{0,0,8,4}}},bottom{{{0,4,8,4},{0,4,8,4}}};
    for(int y=0;y<8;++y)for(int x=0;x<8;++x){pixels[y][x]=y<4?1:9;depth[y][x]=0;}
    clear(bottom.cullViewInfo.sceneViewport);
    assert(pixels[4][0]==9&&depth[4][0]==0); // reproduce old stale-depth failure
    const auto *rect=RB_SetSceneClearViewport(&bottom,nullptr);clear(*rect);
    assert(device.updates==1&&device.viewport.y==4);
    for(int y=0;y<8;++y)for(int x=0;x<8;++x){
        if(y<4){assert(pixels[y][x]==1&&depth[y][x]==0);}
        else{assert(pixels[y][x]==0&&depth[y][x]==1);if(0.5f<depth[y][x])pixels[y][x]=2;assert(pixels[y][x]==2);}
    }
    RB_SetSceneClearViewport(&top,nullptr);assert(device.viewport.y==0&&device.updates==2);
    backend.splitscreen=false;assert(RB_SetSceneClearViewport(&bottom,nullptr)==nullptr&&device.updates==2);
    backend.splitscreen=true;bottom.isMissileCamera=true;
    assert(RB_SetSceneClearViewport(&bottom,nullptr)==nullptr&&device.updates==2);bottom.isMissileCamera=false;
    top.cullViewInfo.displayViewport={0,0,1025,384};bottom.cullViewInfo.displayViewport={0,384,1025,385};
    for(auto dims:std::vector<GfxViewport>{{0,0,1025,769},{0,0,128,96},{0,0,800,600},{0,0,1,1}}){
        gfxCmdBufSourceState.renderTargetWidth=dims.width;gfxCmdBufSourceState.renderTargetHeight=dims.height;
        int boundary=0;
        for(auto *view:{&top,&bottom}){
            const GfxViewport saved=gfxCmdBufSourceState.sceneViewport;
            const auto expected=RB_SplitFilterViewport(&view->cullViewInfo.displayViewport,1025,769,dims.width,dims.height);
            const auto before=draws.size();tess.indexCount=6;
            RB_SplitScreenFilter(&material,view,0xABCDEF12);
            assert(same(gfxCmdBufSourceState.sceneViewport,saved));
            assert(gfxCmdBufSourceState.viewportBehavior==GFX_USE_VIEWPORT_FULL&&tess.indexCount==0);
            assert(expected.y==boundary);boundary+=expected.height;
            if(!expected.height){assert(draws.size()==before);continue;}
            const auto &draw=draws.back();assert(draws.size()==before+1&&same(draw.viewport,expected));
            assert(draw.w==expected.width&&draw.h==expected.height&&draw.color==0xABCDEF12);
            assert(draw.s0==0&&draw.s1==1);
            const float t0=float(view->cullViewInfo.displayViewport.y)/769;
            const float t1=float(view->cullViewInfo.displayViewport.y+view->cullViewInfo.displayViewport.height)/769;
            assert(std::fabs(draw.t0-t0)<1e-6&&std::fabs(draw.t1-t1)<1e-6);
            assert(std::fabs(constants[3]-1/(t1-t0))<1e-5);
            assert(gfxCmdBufSourceState.viewMode==VIEW_MODE_2D);
        }
        assert(boundary==dims.height); // neither gaps nor overlaps at odd heights
    }
    backend.splitscreen=false;gfxCmdBufSourceState.renderTargetWidth=1025;gfxCmdBufSourceState.renderTargetHeight=769;
    RB_SplitScreenFilter(&material,&top,7);assert(draws.back().h==384&&draws.back().color==7);
    puts("PASS: split clear changes hardware viewport before clearing depth; host pixels preserved");
    puts("PASS: split post-filters scale to scene/bloom targets with independent UVs, odd-height coverage and state restoration");
}
'''
cpp=out/'output.cpp';cpp.write_text(code,encoding='utf-8')
exe=out/'output.exe'
subprocess.run([args.compiler,'-std=c++20',str(cpp),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
