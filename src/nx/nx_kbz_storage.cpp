#include "nx_kbz_storage.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <zstd.h>

namespace {
constexpr size_t ReadSize = 256 * 1024;
constexpr unsigned RuntimeBlock = 1;

struct Reader {
    FILE *file;
    size_t total, completed;
    NxKbzProgress progress;
    ZSTD_DStream *stream = nullptr;
    uint8_t *buffer = nullptr;
    ZSTD_inBuffer input{};
    bool frameEnded = false;

    bool decompress(void *destination, size_t size, size_t &written) {
        if (frameEnded) return false;
        if (input.pos == input.size) {
            input = {buffer, fread(buffer, 1, ReadSize, file), 0};
            if (!input.size && ferror(file)) return false;
        }
        ZSTD_outBuffer output{destination, size, 0};
        const size_t before = input.pos;
        const size_t remaining = ZSTD_decompressStream(stream, &output, &input);
        if (ZSTD_isError(remaining)) return false;
        frameEnded = remaining == 0;
        written = output.pos;
        return frameEnded || written != 0 || input.pos != before;
    }

    bool finish() {
        if (completed != total) return false;
        if (!stream) return true;
        uint8_t extra;
        while (!frameEnded) {
            size_t written;
            if (!decompress(&extra, 1, written) || written) return false;
        }
        return input.pos == input.size && fgetc(file) == EOF && !ferror(file);
    }

    bool read(void *destination, size_t size) {
        if (size > total - completed) return false;
        auto *out = static_cast<uint8_t *>(destination);
        while (size) {
            size_t count = std::min(size, ReadSize);
            if (stream) {
                if (!decompress(out, count, count)) return false;
            } else if (fread(out, 1, count, file) != count) {
                return false;
            }
            out += count;
            size -= count;
            completed += count;
            if (progress) progress(completed, total);
        }
        return true;
    }
};

struct Cursor {
    const uint8_t *p;
    size_t remaining;

    bool skip(size_t size) {
        if (size > remaining) return false;
        p += size;
        remaining -= size;
        return true;
    }
    template<typename T> bool take(T &value) {
        if (sizeof(T) > remaining) return false;
        memcpy(&value, p, sizeof(T));
        return skip(sizeof(T));
    }
};

bool contains(const NxKbzStorage &storage, uint8_t block, uint32_t offset, size_t size) {
    return block < storage.blockCount && storage.blocks[block]
        && offset <= storage.sizes[block] && size <= storage.sizes[block] - offset;
}

bool validate(const NxKbzStorage &storage) {
    Cursor cursor{storage.metadata, storage.metadataSize};
    for (uint32_t i = 0; i < storage.relocCount; ++i) {
        uint8_t source, target;
        uint32_t offset, targetOffset;
        if (!cursor.take(source) || !cursor.take(offset)
            || !cursor.take(target) || !cursor.take(targetOffset)
            || !contains(storage, source, offset, sizeof(void *))
            || !contains(storage, target, targetOffset, 0)) return false;
    }
    for (uint32_t i = 0; i < storage.assetCount; ++i) {
        uint8_t block;
        uint32_t type, offset;
        if (!cursor.take(type) || !cursor.take(block) || !cursor.take(offset)
            || !contains(storage, block, offset, 1)) return false;
    }
    if (storage.version >= 2) {
        uint32_t strings, slots;
        if (!cursor.take(strings) || strings > cursor.remaining / 2) return false;
        for (uint32_t i = 0; i < strings; ++i) {
            uint16_t length;
            if (!cursor.take(length) || !cursor.skip(length)) return false;
        }
        if (!cursor.take(slots) || slots > cursor.remaining / 5) return false;
        for (uint32_t i = 0; i < slots; ++i) {
            uint8_t block;
            uint32_t offset;
            uint16_t index;
            if (!cursor.take(block) || !cursor.take(offset)
                || !contains(storage, block, offset, sizeof(index))) return false;
            memcpy(&index, storage.blocks[block] + offset, sizeof(index));
            if (index >= strings) return false;
        }
    }
    if (storage.version >= 3) {
        uint32_t slots;
        if (!cursor.take(slots) || slots > cursor.remaining / 5) return false;
        for (uint32_t i = 0; i < slots; ++i) {
            uint8_t block;
            uint32_t offset;
            if (!cursor.take(block) || !cursor.take(offset)
                || !contains(storage, block, offset, sizeof(void *))) return false;
        }
    }
    return cursor.remaining == 0;
}

bool readStorage(Reader &reader, NxKbzStorage &storage) {
    char magic[4];
    if (!reader.read(magic, sizeof(magic)) || memcmp(magic, "KBZ1", 4)
        || !reader.read(&storage.version, 4) || storage.version < 1 || storage.version > 3
        || !reader.read(&storage.blockCount, 4) || !storage.blockCount || storage.blockCount > 16
        || !reader.read(storage.sizes, storage.blockCount * 4)
        || !reader.read(&storage.relocCount, 4) || !reader.read(&storage.assetCount, 4)) return false;

    uint64_t storedBytes = 0;
    for (unsigned i = 0; i < storage.blockCount; ++i)
        if (i != RuntimeBlock) storedBytes += storage.sizes[i];
    const uint64_t tableBytes = uint64_t(storage.relocCount) * 10 + uint64_t(storage.assetCount) * 9;
    if (storedBytes + tableBytes > reader.total - reader.completed) return false;
    storage.metadataSize = reader.total - reader.completed - size_t(storedBytes);

    for (unsigned i = 0; i < storage.blockCount; ++i) {
        if (!storage.sizes[i]) continue;
        storage.blocks[i] = static_cast<uint8_t *>(i == RuntimeBlock
            ? calloc(1, storage.sizes[i]) : malloc(storage.sizes[i]));
        if (!storage.blocks[i]) return false;
        if (i != RuntimeBlock && !reader.read(storage.blocks[i], storage.sizes[i])) return false;
    }
    storage.metadata = static_cast<uint8_t *>(malloc(std::max(size_t(1), storage.metadataSize)));
    return storage.metadata && reader.read(storage.metadata, storage.metadataSize)
        && reader.finish() && validate(storage);
}
}

int NX_ReadKbz(const char *path, NxKbzStorage **storage, NxKbzProgress progress) {
    *storage = nullptr;
    FILE *file = fopen(path, "rb");
    if (!file) return errno == ENOENT ? 0 : -1;
    long length = -1;
    if (fseek(file, 0, SEEK_END) == 0) length = ftell(file);
    if (length < 20 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return -1;
    }
    auto *result = static_cast<NxKbzStorage *>(calloc(1, sizeof(NxKbzStorage)));
    Reader reader{file, size_t(length), 0, progress};
    std::unique_ptr<ZSTD_DStream, decltype(&ZSTD_freeDStream)> stream(nullptr, ZSTD_freeDStream);
    std::unique_ptr<uint8_t, decltype(&free)> buffer(nullptr, free);
    char magic[4];
    bool ready = fread(magic, 1, sizeof(magic), file) == sizeof(magic);
    if (ready && memcmp(magic, "KBZZ", 4) == 0) {
        uint8_t lengthBytes[8];
        ready = fread(lengthBytes, 1, sizeof(lengthBytes), file) == sizeof(lengthBytes);
        if (ready) {
            uint64_t uncompressedLength = 0;
            for (unsigned i = 0; i < sizeof(lengthBytes); ++i)
                uncompressedLength |= uint64_t(lengthBytes[i]) << (8 * i);
            ready = uncompressedLength >= 20 && uncompressedLength <= std::numeric_limits<size_t>::max();
            if (ready) {
                stream.reset(ZSTD_createDStream());
                buffer.reset(static_cast<uint8_t *>(malloc(ReadSize)));
                ready = stream && buffer && !ZSTD_isError(ZSTD_initDStream(stream.get()));
                reader.total = size_t(uncompressedLength);
                reader.stream = stream.get();
                reader.buffer = buffer.get();
            }
        }
    } else if (ready) {
        ready = fseek(file, 0, SEEK_SET) == 0;
    }
    if (ready && progress) progress(0, reader.total);
    const bool ok = ready && result && readStorage(reader, *result);
    fclose(file);
    if (!ok) {
        NX_FreeKbzStorage(result);
        return -1;
    }
    if (stream)
        printf("[nx-kbz] packed zone (openblops-kbzz-1): %s, %zu bytes unpacked\n", path, reader.total);
    *storage = result;
    return 1;
}

void NX_FreeKbzStorage(NxKbzStorage *storage) {
    if (!storage) return;
    for (auto *block : storage->blocks) free(block);
    free(storage->metadata);
    free(storage);
}
