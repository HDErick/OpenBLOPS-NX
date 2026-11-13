#include "nx_bink.h"
#include "nx_bink_audio.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
extern "C" {
#include <libavutil/md5.h>
#include <libavutil/mem.h>
}

static NxBinkAudio *activeAudio;
static uint64_t nowUs;
static bool outputAvailable = true;

uint64_t NX_BinkTimeUs() { return nowUs; }
bool NX_BinkHasFocus() { return true; }
bool NX_AttachCinematicAudio(NxBinkAudio *audio)
{
    assert(!activeAudio);
    if (!outputAvailable)
        return false;
    activeAudio = audio;
    return true;
}
void NX_DetachCinematicAudio(NxBinkAudio *audio)
{
    assert(activeAudio == audio);
    activeAudio = nullptr;
}

struct Buffers
{
    BINKFRAMEBUFFERS info;
    std::vector<uint8_t> data[2][4];
    explicit Buffers(HBINK movie)
    {
        BinkGetFrameBuffersInfo(movie, &info);
        for (unsigned slot = 0; slot < 2; ++slot)
        {
            auto &f = info.Frames[slot];
            BINKPLANE *planes[] = {&f.YPlane, &f.cBPlane, &f.cRPlane, &f.APlane};
            for (unsigned p = 0; p < 4; ++p)
            {
                if (!planes[p]->Allocate)
                    continue;
                const unsigned width = (p == 1 || p == 2) ? info.cRcBBufferWidth : info.YABufferWidth;
                const unsigned height = (p == 1 || p == 2) ? info.cRcBBufferHeight : info.YABufferHeight;
                planes[p]->BufferPitch = width + 17;
                data[slot][p].assign((width + 17) * height + 32, 0xa7);
                planes[p]->Buffer = data[slot][p].data() + 16;
            }
        }
        BinkRegisterFrameBuffers(movie, &info);
    }

    std::string hash()
    {
        AVMD5 *md5 = av_md5_alloc();
        av_md5_init(md5);
        auto &f = info.Frames[info.FrameNum];
        BINKPLANE *planes[] = {&f.YPlane, &f.cBPlane, &f.cRPlane, &f.APlane};
        for (unsigned p = 0; p < 4; ++p)
        {
            if (!planes[p]->Allocate)
                continue;
            const unsigned width = (p == 1 || p == 2) ? info.cRcBBufferWidth : info.YABufferWidth;
            const unsigned height = (p == 1 || p == 2) ? info.cRcBBufferHeight : info.YABufferHeight;
            auto *bytes = static_cast<uint8_t *>(planes[p]->Buffer);
            for (unsigned y = 0; y < height; ++y)
            {
                av_md5_update(md5, bytes + y * planes[p]->BufferPitch, width);
                for (unsigned x = width; x < planes[p]->BufferPitch; ++x)
                    assert(bytes[y * planes[p]->BufferPitch + x] == 0xa7);
            }
            auto &storage = data[info.FrameNum][p];
            for (unsigned i = 0; i < 16; ++i)
                assert(storage[i] == 0xa7 && storage[storage.size() - 1 - i] == 0xa7);
        }
        uint8_t digest[16];
        av_md5_final(md5, digest);
        av_free(md5);
        char text[33];
        for (unsigned i = 0; i < 16; ++i)
            std::sprintf(text + i * 2, "%02x", digest[i]);
        return text;
    }
};

void audioTests()
{
    NxBinkAudio audio;
    assert(!audio.initialize(0));
    assert(audio.initialize(48000));
    float input[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    float output[8] = {};
    assert(audio.add(input, 4, 0, .5f));
    assert(audio.add(input, 4, 0, .25f));
    assert(!audio.add(input, 4, 48000, 1));
    audio.mix(output, 4, 0, 0);
    assert(output[0] == 0);
    audio.start();
    audio.mix(output, 4, 0, 0);
    for (unsigned i = 0; i < 8; ++i)
        assert(output[i] == input[i] * .75f);
    assert(audio.timeUs(1000) == 4 * 1000000 / 48000);
    audio.pause(true, 1000);
    const auto frozen = audio.timeUs(1000);
    std::memset(output, 0, sizeof(output));
    audio.mix(output, 4, 0, 9000);
    assert(output[0] == 0 && audio.timeUs(9000) == frozen);
    audio.pause(false, 9000);
    assert(audio.add(input, 4, 0, 1));
    audio.mix(output, 4, 0, 9000);
    assert(output[0] == 0);
}

void invalidTests(const std::vector<uint8_t> &file)
{
    outputAvailable = false;
    for (size_t size : {size_t(0), size_t(3), size_t(43), file.size() - 1})
        assert(!NX_BinkOpenMemory(file.data(), size, 0));
    for (unsigned at : {0u, 8u, 20u, 24u, 28u, 32u, 40u})
    {
        auto corrupt = file;
        std::memset(corrupt.data() + at, 0xff, 4);
        assert(!NX_BinkOpenMemory(corrupt.data(), corrupt.size(), 0));
    }
    for (unsigned i = 0; i < 30; ++i)
    {
        HBINK movie = NX_BinkOpenMemory(file.data(), file.size(), 0);
        assert(movie);
        Buffers buffers(movie);
        assert(BinkDoFrame(movie) == 0);
        const auto first = buffers.hash();
        assert(BinkWait(movie));
        BinkNextFrame(movie);
        assert(BinkWait(movie));
        BinkPause(movie, 1);
        nowUs += 60000000;
        assert(BinkWait(movie) && !BinkShouldSkip(movie));
        BinkPause(movie, 0);
        assert(BinkWait(movie));
        nowUs += (3000000ull * movie->FrameRateDiv + movie->FrameRate - 1) / movie->FrameRate;
        assert(!BinkWait(movie) && BinkShouldSkip(movie));
        assert(BinkDoFrame(movie) == 0);
        BinkClose(movie);
    }
    outputAvailable = true;
    HBINK bad = NX_BinkOpenMemory(file.data(), file.size(), 0);
    assert(bad);
    Buffers buffers(bad);
    buffers.info.Frames[0].YPlane.BufferPitch = 1;
    assert(BinkDoFrame(bad) < 0 && bad->ReadError && NX_BinkFinished(bad));
    BinkClose(bad);
    assert(!activeAudio);
}

int main(int argc, char **argv)
{
    assert(argc == 6);
    audioTests();
    std::ifstream input(argv[1], std::ios::binary);
    std::vector<uint8_t> file((std::istreambuf_iterator<char>(input)), {});
    assert(file.size() >= 44);
    const unsigned track = std::stoul(argv[3]);
    BinkSetSoundTrack(1, const_cast<unsigned *>(&track));
    invalidTests(file);
    nowUs = 0;
    HBINK movie = std::strcmp(argv[4], "memory") == 0 ? NX_BinkOpenMemory(file.data(), file.size(), 0) : BinkOpen(argv[1], 0);
    if (!movie) { std::fprintf(stderr, "%s\n", BinkGetError()); return 1; }
    Buffers buffers(movie);
    std::ofstream hashes(std::string(argv[2]) + ".md5");
    std::ofstream audio(std::string(argv[2]) + ".s16", std::ios::binary);
    uint64_t mixed = 0;
    const unsigned loops = std::stoul(argv[5]);
    std::vector<std::string> firstLoop;
    for (unsigned loop = 0; loop < loops; ++loop)
    {
        for (unsigned i = 0; i < movie->Frames; ++i)
        {
            assert(movie->FrameNum == i + 1);
            assert(!NX_BinkFinished(movie));
            if (BinkDoFrame(movie) < 0) { std::fprintf(stderr, "frame %u: %s\n", i, BinkGetError()); return 1; }
            auto hash = buffers.hash();
            hashes << hash << '\n';
            if (!loop) firstLoop.push_back(hash); else assert(firstLoop[i] == hash);
            const uint64_t next = uint64_t(loop * movie->Frames + i + 1) * 48000 * movie->FrameRateDiv / movie->FrameRate;
            while (mixed < next)
            {
                const unsigned count = std::min<uint64_t>(1024, next - mixed);
                float samples[2048] = {};
                if (activeAudio) activeAudio->mix(samples, count, 0, nowUs);
                int16_t pcm[2048];
                for (unsigned n = 0; n < count * 2; ++n)
                    pcm[n] = std::max(-32768l, std::min(32767l, std::lrint(samples[n] * 32768.0f)));
                audio.write(reinterpret_cast<const char *>(pcm), count * 4);
                mixed += count;
                nowUs = mixed * 1000000 / 48000;
            }
            if (i + 1 < movie->Frames || loop + 1 < loops)
                BinkNextFrame(movie);
        }
    }
    float tail[2048] = {};
    if (activeAudio)
        activeAudio->mix(tail, 1024, 0, nowUs);
    nowUs += 100000;
    assert(NX_BinkFinished(movie));
    BinkClose(movie);
    assert(!activeAudio);
    std::printf("Decoded %s: %u cycle(s), video planes, audio, pause, bounds and cleanup passed\n", argv[1], loops);
}
