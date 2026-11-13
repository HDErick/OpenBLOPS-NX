"""Exercise kill-feed icon encoding, console parsing, wrapping and HUD rendering."""
from pathlib import Path
import argparse
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler', default='/ucrt64/bin/g++.exe')
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
output = root / 'build-nx/kill-feed-tests'
output.mkdir(parents=True, exist_ok=True)

def function(path, name):
    text = (root / path).read_text()
    start = text.index(name)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end] + '\n'

source = r'''
#include <atomic>
#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#ifndef __cdecl
#define __cdecl
#endif
#define _DWORD unsigned int
#define _WORD unsigned short
#define BYTE1(x) (((unsigned int)(x)>>8)&255)
#define iassert(x) assert(x)
#define bcassert(x,n) assert((unsigned)(x)<(unsigned)(n))
#define rangeassert(x,a,b) assert((x)>=(a)&&(x)<=(b))
#define __debugbreak() throw std::runtime_error("debugbreak")
constexpr int COLOR_FIRST=48, COLOR_LAST=64, GFX_PRIM_STATS_HUD=0;
struct Material { const char *name; } shotgun{"shotgun"}, melee{"knife"};
struct Font_s { int pixelHeight=16; } font;
struct { int lineOffset=0, prevChannel=5, visiblePixelWidth=10000; char textTempLine[512]{}; } con;
struct { Font_s *consoleFont=&font; } cls;
int scrPlaceView[1],callDepth;
static Material *s_textIcons[1024];
static int s_textIconCount;
static int validations;
static const Material *rendered;
static std::vector<std::string> lines;
bool IsValidMaterialHandle(Material *m){
    ++validations;
    if(m!=&shotgun && m!=&melee) throw std::runtime_error("encoded icon ID was used as a raw material pointer");
    return true;
}
Material *Material_FromHandle(Material *m){return m;}
template<class... T> void RB_DrawStretchPicRotate(const Material *m,T...){rendered=m;}
template<class... T> bool Assert_MyHandler(T...){throw std::runtime_error("engine assertion");}
char *va(const char *,...){static char msg[]="engine assertion";return msg;}
void Con_UpdateNotifyMessage(int,int,int,int){}
void Con_Linefeed(int,int,int){lines.emplace_back(con.textTempLine,con.lineOffset);con.lineOffset=0;}
void Con_UpdateNotifyLine(int,int,int,int){lines.emplace_back(con.textTempLine,con.lineOffset);}
Font_s *UI_GetFontHandle(int *,int,double){return &font;}
double R_NormalizedTextScale(Font_s *,double){return 1;}
int R_LetterWidth(unsigned int,Font_s *){return 8;}
int SEH_ReadCharFromString(const char **text,int){return static_cast<unsigned char>(*(*text)++);}
'''
source += function('src/gfx_d3d/r_font.cpp', 'unsigned int R_TextIconHandle(')
source += function('src/gfx_d3d/r_font.cpp', 'Material *R_TextIconMaterial(')
source += function('src/gfx_d3d/r_font.cpp', 'const char *__cdecl R_TextLineWrapPosition(')
source += function('src/client/cl_console.cpp', 'const char *__cdecl CL_TextLineWrapPosition(')
source += function('src/client/cl_console.cpp', 'int __cdecl CL_AddMessageChar(')
source += function('src/client/cl_console.cpp', 'unsigned int __cdecl CL_AddMessageIcon(')
source += function('src/client/cl_console.cpp', 'void CL_ConsolePrint_AddLine(')
source += function('src/gfx_d3d/rb_backend.cpp', 'double __cdecl RB_DrawHudIcon(')
source += r'''
void feed(Material *material,bool flip,int pixels){
    char message[1024]{};unsigned length=0;
    for(char c:std::string("^3Host Player ")) length=CL_AddMessageChar(message,length,sizeof(message),c);
    const unsigned iconOffset=length;
    length=CL_AddMessageIcon(message,length,sizeof(message),material,1.4f,1.4f,flip);
    assert(length-iconOffset==8);
    unsigned int encoded=0;std::memcpy(&encoded,message+iconOffset+4,4);
    assert(encoded && R_TextIconMaterial(encoded)==material);
    for(char c:std::string(" ^4Remote Player\n"))length=CL_AddMessageChar(message,length,sizeof(message),c);
    message[length]=0;
    lines.clear();con.lineOffset=0;callDepth=0;
    CL_ConsolePrint_AddLine(0,5,message,0,pixels,'7',0);
    assert(!lines.empty() && callDepth==0);
    // Every output icon retains the same 32-bit slot, even across a wrap.
    unsigned copied=0;
    for(const auto &line:lines){
        for(size_t i=0;i+7<line.size();++i){
            if(line[i]=='^'&&(line[i+1]==1||line[i+1]==2)){
                assert(line.compare(i,8,message+iconOffset,8)==0);++copied;i+=7;
            }
        }
    }
    assert(copied==1);
    rendered=nullptr;
    assert(RB_DrawHudIcon(message+iconOffset+1,0,0,0,1,&font,1,1,0xffffffff)>0);
    assert(rendered==material);
}
int main(){
    try {
        // The original PC parser failed on handle 1, before rendering either feed.
        for(int pixels:{10000,160})for(bool flip:{false,true}){
            feed(&shotgun,flip,pixels);feed(&melee,flip,pixels);
        }
        assert(R_TextIconHandle(&shotgun)==1 && R_TextIconHandle(&melee)==2);
        assert(validations>=24);
        puts("PASS: kill-feed material round trip, console parsing/wrap and HUD rendering");
        return 0;
    } catch(const std::exception &e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
}
'''
cpp = output / 'kill-feed.cpp'
cpp.write_text(source)
for name, define in [('pc', '-DWIN32'), ('switch', '-DKISAK_NX')]:
    exe = output / (name + '.exe')
    subprocess.run([args.compiler, '-std=c++20', '-O2', define, str(cpp), '-o', str(exe)], check=True)
    result = subprocess.run([str(exe)])
    result.check_returncode()
    print(name + ': passed', flush=True)
