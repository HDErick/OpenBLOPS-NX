// kbzpack: compress a converted zone (.kbz) for the Switch, losslessly.
//
// Output: "KBZZ", the original size (u64, little endian), then one zstd stream
// (with zstd's content checksum) of the original file. The Switch build
// (src/nx/nx_kbz_storage.cpp) recognises the header and decompresses while it
// loads the zone, so a packed file keeps its .kbz name. Built natively by
// build.sh and to WebAssembly for the installer (installer/build.py).
//
//   kbzpack [-l level] in.kbz out.kbz   compress (default level 9)
//   kbzpack -d in.kbz out.kbz           decompress (a plain .kbz is copied)
//   kbzpack -t in.kbz                   compress in memory and check the round trip
#include <zstd.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
const char PackedMagic[4] = {'K', 'B', 'Z', 'Z'};

bool read_all(const char *path, std::vector<unsigned char> &out)
{
    FILE *f = std::fopen(path, "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    out.resize(size > 0 ? size_t(size) : 0);
    const bool ok = size >= 0 && std::fread(out.data(), 1, out.size(), f) == out.size();
    std::fclose(f);
    return ok;
}

bool write_all(const char *path, const unsigned char *data, size_t size)
{
    FILE *f = std::fopen(path, "wb");
    if (!f) return false;
    const bool ok = std::fwrite(data, 1, size, f) == size;
    return std::fclose(f) == 0 && ok;
}

bool pack(const std::vector<unsigned char> &in, int level, std::vector<unsigned char> &out)
{
    if (in.size() < 4 || std::memcmp(in.data(), "KBZ1", 4) != 0) {
        std::fprintf(stderr, "kbzpack: not a converted zone (no KBZ1 header)\n");
        return false;
    }
    ZSTD_CCtx *cctx = ZSTD_createCCtx();
    if (!cctx) return false;
    ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, level);
    ZSTD_CCtx_setParameter(cctx, ZSTD_c_checksumFlag, 1);
    out.resize(12 + ZSTD_compressBound(in.size()));
    std::memcpy(out.data(), PackedMagic, 4);
    const uint64_t original = in.size();
    for (int i = 0; i < 8; ++i) out[4 + i] = (unsigned char)(original >> (8 * i));
    const size_t n = ZSTD_compress2(cctx, out.data() + 12, out.size() - 12, in.data(), in.size());
    ZSTD_freeCCtx(cctx);
    if (ZSTD_isError(n)) {
        std::fprintf(stderr, "kbzpack: %s\n", ZSTD_getErrorName(n));
        return false;
    }
    out.resize(12 + n);
    return true;
}

bool unpack(const std::vector<unsigned char> &in, std::vector<unsigned char> &out)
{
    if (in.size() < 12 || std::memcmp(in.data(), PackedMagic, 4) != 0) {
        out = in;   // already plain
        return true;
    }
    uint64_t original = 0;
    for (int i = 0; i < 8; ++i) original |= uint64_t(in[4 + i]) << (8 * i);
    out.resize(size_t(original));
    const size_t n = ZSTD_decompress(out.data(), out.size(), in.data() + 12, in.size() - 12);
    if (ZSTD_isError(n) || n != original) {
        std::fprintf(stderr, "kbzpack: damaged stream (%s)\n", ZSTD_isError(n) ? ZSTD_getErrorName(n) : "size");
        return false;
    }
    return true;
}
}

int main(int argc, char **argv)
{
    int level = 9;
    char mode = 'c';
    int arg = 1;
    while (arg < argc && argv[arg][0] == '-') {
        if (!std::strcmp(argv[arg], "-l") && arg + 1 < argc) { level = std::atoi(argv[arg + 1]); arg += 2; }
        else if (!std::strcmp(argv[arg], "-d")) { mode = 'd'; ++arg; }
        else if (!std::strcmp(argv[arg], "-t")) { mode = 't'; ++arg; }
        else break;
    }
    if (argc - arg < (mode == 't' ? 1 : 2)) {
        std::fprintf(stderr, "usage: kbzpack [-l level] in.kbz out.kbz | -d in.kbz out.kbz | -t in.kbz\n");
        return 2;
    }
    std::vector<unsigned char> in, out, back;
    if (!read_all(argv[arg], in)) {
        std::fprintf(stderr, "kbzpack: cannot read %s\n", argv[arg]);
        return 1;
    }
    if (mode == 'd') return unpack(in, out) && write_all(argv[arg + 1], out.data(), out.size()) ? 0 : 1;
    if (!pack(in, level, out)) return 1;
    if (mode == 't') {
        const bool same = unpack(out, back) && back == in;
        std::printf("%s: %zu -> %zu bytes (x%.2f) round trip %s\n", argv[arg], in.size(), out.size(),
                    double(in.size()) / double(out.size()), same ? "OK" : "FAILED");
        return same ? 0 : 1;
    }
    return write_all(argv[arg + 1], out.data(), out.size()) ? 0 : 1;
}
