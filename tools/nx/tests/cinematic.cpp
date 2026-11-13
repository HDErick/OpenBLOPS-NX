#include <cassert>
#include <cstring>
#include <vector>
#include <binklib/bink.h>

#define __int8 char
#define __int64 long long
#define PROF_SCOPED(x)
#define iassert assert
#define __debugbreak() assert(false)
template<class... T> bool Assert_MyHandler(T...) { assert(false); return false; }

using HRESULT = int;
constexpr unsigned D3DLOCK_DISCARD = 0x2000;
struct D3DLOCKED_RECT { int Pitch; void *pBits; };
struct IDirect3DDevice9 {};
struct IDirect3DTexture9
{
    unsigned locks = 0, unlocks = 0;
    int pitch = 7;
    bool fail = false, failUnlock = false;
    std::vector<unsigned char> bytes = std::vector<unsigned char>(64, 0xa7);
    HRESULT LockRect(unsigned, D3DLOCKED_RECT *out, void *, unsigned flags)
    {
        assert(flags == D3DLOCK_DISCARD);
        ++locks;
        if (fail) return -1;
        *out = {pitch, bytes.data()};
        return 0;
    }
    HRESULT UnlockRect(unsigned) { ++unlocks; return failUnlock ? -1 : 0; }
};
struct BINKFRAMETEXTURES { IDirect3DTexture9 *Ytexture, *cRtexture, *cBtexture, *Atexture; };
struct BINKTEXTURESET { BINKFRAMEBUFFERS bink_buffers; BINKFRAMETEXTURES textures[2], tex_draw; };
enum CinematicEnum { CINEMATIC_NOT_PAUSED, CINEMATIC_PAUSED, CINEMATIC_SCRIPT_PAUSED };
enum { CIN_IOSTATE_RELINQUISHED, CIN_IOSTATE_SEIZED };
struct
{
    BINK *bink;
    BINKTEXTURESET binkTextureSet;
    void *YTextures[2], *crTextures[2], *cbTextures[2], *aTextures[2];
    CinematicEnum targetPaused, currentPaused;
    bool underrun, uploadFailed, firstFrameNotify, lastFrameNotify;
    unsigned playbackFlags, binkIOSize;
    int startTimeMS, frameNum, activeImageFrame = -1, activeImageFrameTextureSet, activeTextureSet, fileIoState;
} cinematicGlob;
struct { IDirect3DDevice9 *device; } dx;
static unsigned decodes, now;
static bool decoderFails, waitFrame, skipFrames, renderThread = true;
unsigned R_Cinematic_GetPercentageFull() { return 100; }
int Sys_Milliseconds() { return now; }
void NET_Sleep(int) { ++now; }
int R_Cinematic_BinkShouldWait(bool) { return waitFrame; }
int R_Cinematic_BinkShouldSkip(bool) { return skipFrames; }
bool Sys_IsRenderThread() { return renderThread; }
void RB_Resource_Callback(void (*callback)()) { callback(); }
void RB_Resource_Flush() {}
void R_Cinematic_CheckBinkError() {}
void R_Cinematic_UpdateTimeInMsec(const BINKREALTIME *) {}
void R_Cinematic_RelinquishIO() { cinematicGlob.fileIoState = CIN_IOSTATE_RELINQUISHED; }
void R_Cinematic_SeizeIO() { cinematicGlob.fileIoState = CIN_IOSTATE_SEIZED; }
bool NX_BinkFinished(HBINK b) { return b->LastFrameNum == b->Frames; }
S32 BinkPause(HBINK, S32 paused) { return paused; }
S32 BinkDoFrame(HBINK b)
{
    ++decodes;
    if (decoderFails) return -1;
    b->LastFrameNum = b->FrameNum;
    cinematicGlob.binkTextureSet.bink_buffers.FrameNum = (b->FrameNum - 1) % 2;
    return 0;
}
void BinkNextFrame(HBINK b) { b->FrameNum = b->FrameNum % b->Frames + 1; }
void BinkGetRealtime(HBINK, BINKREALTIME *r, U32) { std::memset(r, 0, sizeof(*r)); }

#include "cinematic-functions.inc"

int main()
{
    unsigned char source[12] = {1, 2, 3, 9, 9, 9, 4, 5, 6, 9, 9, 9};
    BINKPLANE plane = {1, source, 6};
    IDirect3DTexture9 texture;
    assert(R_Cinematic_UploadPlane(&texture, plane, 3, 2));
    assert(texture.bytes[0] == 1 && texture.bytes[2] == 3 && texture.bytes[7] == 4 && texture.bytes[9] == 6);
    assert(texture.bytes[3] == 0xa7 && texture.bytes[10] == 0xa7);
    texture.fail = true;
    assert(!R_Cinematic_UploadPlane(&texture, plane, 3, 2));
    assert(texture.unlocks == 1);
    texture.fail = false;
    texture.pitch = 1;
    assert(!R_Cinematic_UploadPlane(&texture, plane, 3, 2));
    assert(texture.unlocks == 2);
    texture.pitch = 7;
    texture.failUnlock = true;
    assert(!R_Cinematic_UploadPlane(&texture, plane, 3, 2));
    texture.failUnlock = false;

    BINK bink = {};
    bink.Frames = 3;
    bink.FrameNum = 1;
    cinematicGlob.bink = &bink;
    auto &buffers = cinematicGlob.binkTextureSet.bink_buffers;
    buffers.TotalFrames = 2;
    buffers.YABufferWidth = buffers.YABufferHeight = buffers.cRcBBufferWidth = buffers.cRcBBufferHeight = 2;
    cinematicGlob.binkTextureSet.tex_draw = {&texture, &texture, &texture, nullptr};
    for (unsigned i = 0; i < 2; ++i)
    {
        cinematicGlob.YTextures[i] = source;
        cinematicGlob.crTextures[i] = source;
        cinematicGlob.cbTextures[i] = source;
    }
    assert(R_Cinematic_Advance(false) == 1 && decodes == 1 && cinematicGlob.firstFrameNotify);
    assert(R_Cinematic_Advance(false) == 1 && decodes == 2 && !cinematicGlob.lastFrameNotify);
    assert(R_Cinematic_Advance(false) == 0 && decodes == 3 && cinematicGlob.lastFrameNotify);
    cinematicGlob.targetPaused = CINEMATIC_SCRIPT_PAUSED;
    R_Cinematic_Advance(false);
    assert(decodes == 3);
    cinematicGlob.targetPaused = CINEMATIC_NOT_PAUSED;
    cinematicGlob.playbackFlags = 2;
    cinematicGlob.lastFrameNotify = false;
    renderThread = false;
    for (unsigned i = 0; i < 30; ++i)
        assert(R_Cinematic_Advance(false) == 1 && !cinematicGlob.lastFrameNotify);
    const unsigned before = decodes;
    skipFrames = true;
    R_Cinematic_Advance(false);
    assert(decodes == before + 4);
    decoderFails = true;
    assert(R_Cinematic_Advance(false) == 0 && cinematicGlob.lastFrameNotify);
    decoderFails = skipFrames = false;
    cinematicGlob.lastFrameNotify = false;
    texture.fail = true;
    assert(R_Cinematic_Advance(false) == 0 && cinematicGlob.lastFrameNotify);
}
