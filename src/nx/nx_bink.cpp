#include "nx_bink.h"
#include "nx_bink_audio.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/mathematics.h>
#include <libavutil/mem.h>
#include <libswresample/swresample.h>
extern const AVCodec ff_bink_decoder;
extern const AVCodec ff_binkaudio_dct_decoder;
extern const AVCodec ff_binkaudio_rdft_decoder;
}

namespace
{
constexpr size_t FileBufferSize = 256 * 1024;
constexpr unsigned MaxPacketSize = 16 * 1024 * 1024;
thread_local char errorText[256];
unsigned selectedTracks[256] = {0};
unsigned selectedCount = 1;

bool fail(const char *text)
{
    std::snprintf(errorText, sizeof(errorText), "%s", text);
    return false;
}

uint32_t little32(const void *data)
{
    const auto *p = static_cast<const uint8_t *>(data);
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}

struct Track
{
    unsigned id = 0, rate = 0, channels = 0, flags = 0;
    int volume = 32768;
    uint64_t samples = 0;
    bool selected = false;
    AVCodecContext *codec = nullptr;
    AVFrame *frame = nullptr;
    SwrContext *resampler = nullptr;

    ~Track()
    {
        swr_free(&resampler);
        av_frame_free(&frame);
        avcodec_free_context(&codec);
    }
};

struct Movie : BINK
{
    std::mutex mutex;
    FILE *file = nullptr;
    std::unique_ptr<char[]> fileBuffer;
    const uint8_t *memory = nullptr;
    uint64_t sourceSize = 0, offset = 0;
    std::unique_ptr<uint32_t[]> index;
    std::unique_ptr<Track[]> tracks;
    AVCodecContext *video = nullptr;
    AVFrame *frame = nullptr;
    AVPacket *packet = nullptr;
    uint8_t *compressed = nullptr;
    unsigned packetCapacity = 0;
    unsigned tag = 0, videoFlags = 0;
    NxBinkAudio audio;
    bool audioAttached = false, started = false, paused = false, rewind = false;
    uint64_t sequence = 0, epoch = 0, pausedAt = 0, loopSample = 0;

    Movie() : BINK{} {}
    ~Movie()
    {
        if (audioAttached)
            NX_DetachCinematicAudio(&audio);
        av_packet_free(&packet);
        av_frame_free(&frame);
        avcodec_free_context(&video);
        av_free(compressed);
        if (file)
            std::fclose(file);
    }

    bool read(void *dest, size_t size)
    {
        if (offset > sourceSize || size > sourceSize - offset)
            return fail("Truncated Bink data");
        if (memory)
            std::memcpy(dest, memory + offset, size);
        else if (std::fread(dest, 1, size, file) != size)
            return fail("Bink file read failed");
        offset += size;
        return true;
    }

    bool seek(uint64_t position)
    {
        if (position > sourceSize || (!memory && position != offset && fseeko(file, position, SEEK_SET)))
            return fail("Bink file seek failed");
        offset = position;
        return true;
    }

    uint64_t timestamp(uint64_t frameNumber, unsigned rate) const
    {
        return av_rescale_q(frameNumber, AVRational{int(FrameRateDiv), int(FrameRate)}, AVRational{1, int(rate)});
    }

    uint64_t clock()
    {
        if (!started)
            return 0;
        const uint64_t now = NX_BinkTimeUs();
        if (audioAttached)
            return audio.timeUs(now);
        return (paused ? pausedAt : now) - epoch;
    }
};

bool extraData(AVCodecContext *codec, uint32_t value)
{
    codec->extradata = static_cast<uint8_t *>(av_mallocz(4 + AV_INPUT_BUFFER_PADDING_SIZE));
    if (!codec->extradata)
        return fail("Out of memory opening Bink codec");
    codec->extradata_size = 4;
    for (unsigned i = 0; i < 4; ++i)
        codec->extradata[i] = value >> (i * 8);
    return true;
}

bool openTrack(Track &track, unsigned tag)
{
    const AVCodec *decoder = (track.flags & 0x1000) ? &ff_binkaudio_dct_decoder : &ff_binkaudio_rdft_decoder;
    track.codec = avcodec_alloc_context3(decoder);
    track.frame = av_frame_alloc();
    if (!track.codec || !track.frame)
        return fail("Out of memory opening Bink audio");
    track.codec->sample_rate = track.rate;
    av_channel_layout_default(&track.codec->ch_layout, track.channels);
    if (!extraData(track.codec, tag) || avcodec_open2(track.codec, decoder, nullptr) < 0)
        return fail("Cannot open Bink audio decoder");
    AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
    if (swr_alloc_set_opts2(&track.resampler, &stereo, AV_SAMPLE_FMT_FLT, NxBinkAudio::Rate,
                          &track.codec->ch_layout, track.codec->sample_fmt, track.rate, 0, nullptr) < 0
        || swr_init(track.resampler) < 0)
        return fail("Cannot open Bink audio resampler");
    return true;
}

bool openMovie(Movie &m, unsigned flags)
{
    uint8_t header[44];
    if (!m.read(header, sizeof(header)))
        return false;
    if (std::memcmp(header, "BIK", 3) || !std::strchr("bfghik", header[3]) || !header[3])
        return fail("Unsupported Bink signature");
    m.tag = little32(header);
    const uint64_t size = uint64_t(little32(header + 4)) + 8;
    m.Frames = little32(header + 8);
    m.Width = little32(header + 20);
    m.Height = little32(header + 24);
    m.FrameRate = little32(header + 28);
    m.FrameRateDiv = little32(header + 32);
    m.videoFlags = little32(header + 36);
    const unsigned trackCount = little32(header + 40);
    if (size > m.sourceSize || size > UINT32_MAX || !m.Frames || m.Frames > 1000000
        || !m.Width || !m.Height || m.Width > 4096 || m.Height > 2160
        || !m.FrameRate || !m.FrameRateDiv || m.FrameRate > INT32_MAX || m.FrameRateDiv > INT32_MAX
        || m.FrameRateDiv > m.FrameRate || uint64_t(m.FrameRate) > uint64_t(m.FrameRateDiv) * 240
        || trackCount > 256)
        return fail("Invalid Bink header");
    m.sourceSize = size;
    m.Size = size;
    m.OpenFlags = flags;
    m.FrameNum = 1;
    m.NumTracks = trackCount;
    if (header[3] == 'k' && !m.seek(m.offset + 4))
        return false;
    uint8_t trackHeaders[256 * 12];
    if (!m.read(trackHeaders, trackCount * 12))
        return false;
    m.tracks.reset(new (std::nothrow) Track[trackCount]);
    m.index.reset(new (std::nothrow) uint32_t[m.Frames + 1]);
    if (!m.tracks || !m.index)
        return fail("Out of memory opening Bink index");
    unsigned audioCapacity = NxBinkAudio::Rate * 2;
    for (unsigned i = 0; i < trackCount; ++i)
    {
        Track &track = m.tracks[i];
        const unsigned format = little32(trackHeaders + (trackCount + i) * 4);
        track.rate = format & 0xffff;
        track.flags = format >> 16;
        track.channels = (track.flags & 0x2000) ? 2 : 1;
        track.id = little32(trackHeaders + (trackCount * 2 + i) * 4);
        track.selected = std::find(selectedTracks, selectedTracks + selectedCount, track.id) != selectedTracks + selectedCount;
        const unsigned maxPCM = little32(trackHeaders + i * 4);
        if (track.rate < 1000 || maxPCM > MaxPacketSize)
            return fail("Invalid Bink audio header");
        if (track.selected)
        {
            const uint64_t frames = uint64_t(maxPCM) * NxBinkAudio::Rate / (2 * track.channels * track.rate);
            if (frames > NxBinkAudio::Rate * 14)
                return fail("Bink audio packet exceeds buffer limit");
            audioCapacity = std::max(audioCapacity, unsigned(frames) + NxBinkAudio::Rate * 2);
        }
    }
    const uint64_t headerEnd = m.offset + uint64_t(m.Frames) * 4;
    if (headerEnd > size)
        return fail("Truncated Bink index");
    for (unsigned i = 0; i < m.Frames; ++i)
    {
        uint8_t value[4];
        if (!m.read(value, sizeof(value)))
            return false;
        m.index[i] = little32(value) & ~1u;
        if (m.index[i] < headerEnd || m.index[i] >= size || (i && m.index[i] <= m.index[i - 1]))
            return fail("Invalid Bink frame offset");
    }
    m.index[m.Frames] = unsigned(size);
    for (unsigned i = 0; i < m.Frames; ++i)
    {
        const unsigned bytes = m.index[i + 1] - m.index[i];
        if (bytes > MaxPacketSize || bytes < trackCount * 4 + 1)
            return fail("Invalid Bink frame size");
        m.packetCapacity = std::max(m.packetCapacity, bytes);
    }
    m.LargestFrameSize = m.packetCapacity;
    m.compressed = static_cast<uint8_t *>(av_malloc(m.packetCapacity + AV_INPUT_BUFFER_PADDING_SIZE));
    m.video = avcodec_alloc_context3(&ff_bink_decoder);
    m.frame = av_frame_alloc();
    m.packet = av_packet_alloc();
    if (!m.compressed || !m.video || !m.frame || !m.packet)
        return fail("Out of memory opening Bink video");
    m.video->width = m.Width;
    m.video->height = m.Height;
    m.video->codec_tag = m.tag;
    m.video->thread_count = 1;
    if (!extraData(m.video, m.videoFlags) || avcodec_open2(m.video, &ff_bink_decoder, nullptr) < 0)
        return fail("Cannot open Bink video decoder");
    if (trackCount && selectedCount)
    {
        if (!m.audio.initialize(audioCapacity))
            return fail("Out of memory opening Bink audio buffer");
        m.audioAttached = NX_AttachCinematicAudio(&m.audio);
        if (m.audioAttached)
            for (unsigned i = 0; i < trackCount; ++i)
                if (m.tracks[i].selected && !openTrack(m.tracks[i], m.tag))
                    return false;
    }
    return m.seek(m.index[0]);
}

bool sendPacket(Movie &m, AVCodecContext *codec, const uint8_t *data, unsigned size)
{
    av_packet_unref(m.packet);
    if (av_new_packet(m.packet, size) < 0)
        return fail("Out of memory decoding Bink packet");
    std::memcpy(m.packet->data, data, size);
    if (avcodec_send_packet(codec, m.packet) < 0)
        return fail("Bink packet decode failed");
    return true;
}

bool decodeAudio(Movie &m, Track &track, const uint8_t *data, unsigned size)
{
    if (!track.codec || size < 4)
        return true;
    if (little32(data) > MaxPacketSize || !sendPacket(m, track.codec, data, size))
        return fail("Invalid Bink audio packet");
    int result;
    float samples[4096 * 2];
    const uint64_t end = m.timestamp((m.sequence / m.Frames + 1) * m.Frames, NxBinkAudio::Rate);
    while ((result = avcodec_receive_frame(track.codec, track.frame)) >= 0)
    {
        const uint8_t **input = const_cast<const uint8_t **>(track.frame->extended_data);
        int count = track.frame->nb_samples;
        do
        {
            uint8_t *output = reinterpret_cast<uint8_t *>(samples);
            const int frames = swr_convert(track.resampler, &output, 4096, input, count);
            if (frames < 0)
                return fail("Bink audio resampling failed");
            const uint64_t first = m.loopSample + track.samples;
            const size_t audible = first < end ? std::min<uint64_t>(frames, end - first) : 0;
            if (!m.audio.add(samples, audible, first, track.volume / 32768.0f))
                return fail("Bink audio queue overflow");
            track.samples += frames;
            input = nullptr;
            count = 0;
            if (frames < 4096)
                break;
        } while (true);
        av_frame_unref(track.frame);
    }
    return result == AVERROR(EAGAIN) || result == AVERROR_EOF || fail("Bink audio decode failed");
}

bool copyPlane(BINKPLANE &dest, const uint8_t *source, int stride, unsigned width, unsigned height, unsigned range = 0)
{
    if (!dest.Buffer || dest.BufferPitch < width || !source || stride < int(width))
        return fail("Invalid Bink output plane");
    for (unsigned y = 0; y < height; ++y)
    {
        auto *row = static_cast<uint8_t *>(dest.Buffer) + y * dest.BufferPitch;
        if (!range)
            std::memcpy(row, source + y * stride, width);
        else
            for (unsigned x = 0; x < width; ++x)
                row[x] = 16 + (source[y * stride + x] * range + 127) / 255;
    }
    return true;
}
}

HBINK RADEXPLINK BinkOpen(const char *name, U32 flags)
{
    errorText[0] = 0;
    std::unique_ptr<Movie> m(new (std::nothrow) Movie);
    if (!m || !name || (flags & BINKFROMMEMORY))
    {
        fail("Invalid Bink file source");
        return nullptr;
    }
    m->file = std::fopen(name, "rb");
    if (!m->file)
    {
        fail("Cannot open Bink file");
        return nullptr;
    }
    m->fileBuffer.reset(new (std::nothrow) char[FileBufferSize]);
    if (m->fileBuffer)
        std::setvbuf(m->file, m->fileBuffer.get(), _IOFBF, FileBufferSize);
    if (fseeko(m->file, 0, SEEK_END))
    {
        fail("Cannot determine Bink file size");
        return nullptr;
    }
    const auto size = ftello(m->file);
    if (size < 0 || fseeko(m->file, 0, SEEK_SET))
    {
        fail("Cannot seek Bink file");
        return nullptr;
    }
    m->sourceSize = size;
    return openMovie(*m, flags) ? m.release() : nullptr;
}

HBINK NX_BinkOpenMemory(const void *data, size_t size, unsigned flags)
{
    errorText[0] = 0;
    std::unique_ptr<Movie> m(new (std::nothrow) Movie);
    if (!m || !data)
    {
        fail("Invalid Bink memory source");
        return nullptr;
    }
    m->memory = static_cast<const uint8_t *>(data);
    m->sourceSize = size;
    return openMovie(*m, flags | BINKFROMMEMORY) ? m.release() : nullptr;
}

void RADEXPLINK BinkClose(HBINK bink)
{
    delete static_cast<Movie *>(bink);
    errorText[0] = 0;
}

char *RADEXPLINK BinkGetError()
{
    return errorText;
}

void RADEXPLINK BinkSetSoundTrack(U32 total, U32 *tracks)
{
    selectedCount = tracks ? std::min(total, 256u) : 0;
    if (selectedCount)
        std::memcpy(selectedTracks, tracks, selectedCount * sizeof(*tracks));
}

void RADEXPLINK BinkSetVolume(HBINK bink, U32 id, S32 volume)
{
    auto &m = *static_cast<Movie *>(bink);
    std::lock_guard<std::mutex> lock(m.mutex);
    for (int i = 0; i < m.NumTracks; ++i)
        if (m.tracks[i].id == id)
            m.tracks[i].volume = std::max(0, std::min(volume, 32768));
}

void RADEXPLINK BinkGetFrameBuffersInfo(HBINK bink, BINKFRAMEBUFFERS *buffers)
{
    std::memset(buffers, 0, sizeof(*buffers));
    buffers->TotalFrames = 2;
    buffers->YABufferWidth = bink->Width;
    buffers->YABufferHeight = bink->Height;
    buffers->cRcBBufferWidth = (bink->Width + 1) / 2;
    buffers->cRcBBufferHeight = (bink->Height + 1) / 2;
    for (auto &frame : buffers->Frames)
    {
        frame.YPlane.Allocate = frame.cRPlane.Allocate = frame.cBPlane.Allocate = 1;
        frame.APlane.Allocate = (static_cast<Movie *>(bink)->videoFlags & 0x100000) != 0;
    }
}

void RADEXPLINK BinkRegisterFrameBuffers(HBINK bink, BINKFRAMEBUFFERS *buffers)
{
    bink->FrameBuffers = buffers;
}

S32 RADEXPLINK BinkDoFrame(HBINK bink)
{
    auto &m = *static_cast<Movie *>(bink);
    std::lock_guard<std::mutex> lock(m.mutex);
    if (m.ReadError)
        return -1;
    if (m.LastFrameNum == m.FrameNum && !m.rewind)
        return 0;
    if (m.rewind)
    {
        avcodec_flush_buffers(m.video);
        for (int i = 0; i < m.NumTracks; ++i)
        {
            Track &track = m.tracks[i];
            track.samples = 0;
            if (track.codec)
            {
                avcodec_flush_buffers(track.codec);
                swr_close(track.resampler);
                if (swr_init(track.resampler) < 0)
                    m.ReadError = 1;
            }
        }
        m.loopSample = m.timestamp(m.sequence, NxBinkAudio::Rate);
        m.rewind = false;
    }
    const unsigned first = m.index[m.FrameNum - 1];
    const unsigned bytes = m.index[m.FrameNum] - first;
    unsigned position = 0;
    bool ok = !m.ReadError && m.seek(first) && m.read(m.compressed, bytes);
    for (int i = 0; ok && i < m.NumTracks; ++i)
    {
        if (bytes - position < 4)
        {
            ok = fail("Truncated Bink audio packet header");
            break;
        }
        const unsigned size = little32(m.compressed + position);
        position += 4;
        if (size > bytes - position)
        {
            ok = fail("Truncated Bink audio packet");
            break;
        }
        ok = decodeAudio(m, m.tracks[i], m.compressed + position, size);
        position += size;
    }
    if (ok)
        ok = position < bytes && sendPacket(m, m.video, m.compressed + position, bytes - position)
            && avcodec_receive_frame(m.video, m.frame) >= 0;
    if (ok)
    {
        BINKFRAMEBUFFERS *buffers = m.FrameBuffers;
        ok = buffers && buffers->TotalFrames == 2 && m.frame->width == int(m.Width) && m.frame->height == int(m.Height)
            && (m.frame->format == AV_PIX_FMT_YUV420P || m.frame->format == AV_PIX_FMT_YUVA420P);
        if (ok)
        {
            const unsigned slot = (m.FrameNum - 1) & 1;
            BINKFRAMEPLANESET &output = buffers->Frames[slot];
            const bool fullRange = m.frame->color_range == AVCOL_RANGE_JPEG;
            ok = copyPlane(output.YPlane, m.frame->data[0], m.frame->linesize[0], m.Width, m.Height, fullRange ? 219 : 0)
                && copyPlane(output.cBPlane, m.frame->data[1], m.frame->linesize[1], (m.Width + 1) / 2, (m.Height + 1) / 2, fullRange ? 224 : 0)
                && copyPlane(output.cRPlane, m.frame->data[2], m.frame->linesize[2], (m.Width + 1) / 2, (m.Height + 1) / 2, fullRange ? 224 : 0);
            if (ok && output.APlane.Allocate)
                ok = copyPlane(output.APlane, m.frame->data[3], m.frame->linesize[3], m.Width, m.Height);
            if (ok)
                buffers->FrameNum = slot;
        }
    }
    av_frame_unref(m.frame);
    if (!ok)
    {
        m.ReadError = 1;
        if (m.audioAttached)
            m.audio.pause(true, NX_BinkTimeUs());
        if (!errorText[0])
            fail("Bink video decode failed");
        return -1;
    }
    m.LastFrameNum = m.FrameNum;
    if (!m.started)
    {
        m.epoch = NX_BinkTimeUs();
        m.pausedAt = m.epoch;
        m.started = true;
        if (m.audioAttached)
            m.audio.start();
    }
    return 0;
}

void RADEXPLINK BinkNextFrame(HBINK bink)
{
    auto &m = *static_cast<Movie *>(bink);
    std::lock_guard<std::mutex> lock(m.mutex);
    ++m.sequence;
    if (++m.FrameNum > m.Frames)
    {
        m.FrameNum = 1;
        m.rewind = true;
    }
}

S32 RADEXPLINK BinkWait(HBINK bink)
{
    auto &m = *static_cast<Movie *>(bink);
    std::lock_guard<std::mutex> lock(m.mutex);
    const uint64_t due = m.sequence + (m.LastFrameNum == m.FrameNum && !m.rewind);
    return !m.ReadError && (!NX_BinkHasFocus() || m.paused || (m.started && m.clock() < m.timestamp(due, 1000000)));
}

S32 RADEXPLINK BinkShouldSkip(HBINK bink)
{
    auto &m = *static_cast<Movie *>(bink);
    std::lock_guard<std::mutex> lock(m.mutex);
    return !m.ReadError && NX_BinkHasFocus() && !m.paused && m.started && m.clock() >= m.timestamp(m.sequence + 1, 1000000);
}

S32 RADEXPLINK BinkPause(HBINK bink, S32 paused)
{
    auto &m = *static_cast<Movie *>(bink);
    std::lock_guard<std::mutex> lock(m.mutex);
    const uint64_t now = NX_BinkTimeUs();
    if (bool(paused) != m.paused)
    {
        if (paused)
            m.pausedAt = now;
        else if (m.started)
            m.epoch += now - m.pausedAt;
        m.paused = paused != 0;
        if (m.audioAttached)
            m.audio.pause(m.paused, now);
    }
    return m.paused;
}

bool NX_BinkFinished(HBINK bink)
{
    auto &m = *static_cast<Movie *>(bink);
    std::lock_guard<std::mutex> lock(m.mutex);
    return m.ReadError || (m.LastFrameNum == m.Frames && m.clock() >= m.timestamp(m.sequence + 1, 1000000));
}

void RADEXPLINK BinkGetRealtime(HBINK bink, BINKREALTIME *realtime, U32)
{
    std::memset(realtime, 0, sizeof(*realtime));
    realtime->FrameNum = bink->LastFrameNum;
    realtime->FrameRate = bink->FrameRate;
    realtime->FrameRateDiv = bink->FrameRateDiv;
}
