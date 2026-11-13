#include <atomic>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include "../../../src/qcommon/unzip.h"

static std::atomic<unsigned> opens, closes;
static thread_local bool failRead;
static void require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
FILE *FS_FileOpenReadBinary(const char *path) {
    FILE *file = fopen(path, "rb"); if (file) ++opens; return file;
}
void FS_FileClose(FILE *file) { fclose(file); ++closes; }
unsigned FS_FileRead(void *buffer, unsigned size, FILE *file) {
    if (failRead) { failRead = false; return 0; }
    return unsigned(fread(buffer, 1, size, file));
}
int FS_FileSeek(FILE *file, int offset, int origin) {
    return fseek(file, offset, origin == 2 ? SEEK_SET : origin == 1 ? SEEK_END : SEEK_CUR);
}
int FS_FileGetFileSize(FILE *file) {
    auto current = ftell(file); fseek(file, 0, SEEK_END);
    auto length = ftell(file); fseek(file, current, SEEK_SET); return int(length);
}
#define __int8 char
#define _iobuf FILE
#define __debugbreak() throw std::runtime_error("engine assertion")
#include "file-types.inc"
struct iwd_t { unzFile handle; bool hasOpenFile; };
static fileHandleData_t fsh[70];
static struct { struct { int integer = 0; } current; } debug;
static auto *fs_debug = &debug;
static void *fs_searchpaths;
static int FS_filelength(int h) { return int(reinterpret_cast<unz_s *>(fsh[h].handleFiles.file.z)->cur_file_info.uncompressed_size); }
static const char *Sys_GetCurrentThreadName() { return "test"; }
static bool Assert_MyHandler(const char *, int, int, const char *, ...) { return false; }
static void Com_Printf(int, const char *, ...) {}
static void Com_Memset(unsigned int *p, int value, size_t size) { memset(p, value, size); }
static void SEH_Shutdown_StringEd() {}
static void FS_RemoveCommands() {}
static void FS_ShutdownSearchPaths(void *paths) { unzClose(reinterpret_cast<unzFile>(paths)); }
enum FsThread { FS_THREAD_MAIN };
static struct { struct { const char *string = ""; } current; } homepath;
static auto *fs_homepath = &homepath;
static char fs_gamedir[256];
static int FS_HandleForFile(const char *, FsThread) {
    for (int h = 26; h < 50; ++h) if (!fsh[h].handleFiles.file.o) return h;
    throw std::runtime_error("no free output handles");
}
static int FS_HandleForFileCurrentThread(const char *name) { return FS_HandleForFile(name, FS_THREAD_MAIN); }
static void I_strncpyz(char *dest, const char *source, size_t size) { snprintf(dest, size, "%s", source); }
static void FS_BuildOSPath(char *, char *, char *name, char *path) { I_strncpyz(path, name, 256); }
static bool FS_CreatePath(char *) { return false; }
static FILE *openOutput(const char *path, const char *mode) {
    FILE *file = fopen(path, mode); if (file) ++opens; return file;
}
static FILE *FS_FileOpenWriteBinary(const char *path) { return openOutput(path, "wb"); }
static FILE *FS_FileOpenWriteText(const char *path) { return openOutput(path, "wb"); }
static FILE *FS_FileOpenAppendText(const char *path) { return openOutput(path, "ab"); }
static unsigned FS_FileWrite(const void *buffer, unsigned size, FILE *file) { return unsigned(fwrite(buffer, 1, size, file)); }
#include "file-functions.inc"
static std::vector<char> expected(const std::string &path) {
    std::ifstream file(path, std::ios::binary);
    return {(std::istreambuf_iterator<char>(file)), {}};
}
static void checkAt(unzFile file, const std::vector<char> &data, unsigned position, unsigned count) {
    require(unzSeekCurrentFile(file, position) == UNZ_OK, "seek failed");
    std::vector<char> read(count);
    require(unzReadCurrentFile(file, read.data(), count) == int(count), "short entry read");
    require(!memcmp(read.data(), data.data() + position, count), "entry data or offset changed");
}
int main(int argc, char **argv) {
    try {
        require(argc == 2, "missing fixture directory");
        const std::string root = argv[1], path = root + "/stream.iwd";
        const auto data = expected(root + "/payload.bin");
        unzFile archive = unzOpen(path.c_str()); require(archive, "archive open failed");
        fs_searchpaths = archive;
        std::map<std::string, unsigned long> positions;
        require(unzGoToFirstFile(archive) == UNZ_OK, "no entries");
        do {
            char name[256]; unsigned long position;
            require(unzGetCurrentFileInfo(archive, nullptr, name, sizeof(name), nullptr, 0, nullptr, 0) == UNZ_OK, "directory read failed");
            unzGetCurrentFileInfoPosition(archive, &position); positions[name] = position;
        } while (unzGoToNextFile(archive) == UNZ_OK);
        const auto original = *reinterpret_cast<unz_s *>(archive);
        auto stored = unzOpenEntry(path.c_str(), archive, positions.at("stored"));
        auto deflated = unzOpenEntry(path.c_str(), archive, positions.at("deflated"));
        require(stored && deflated, "entry open failed");
        iwd_t parent{archive, false};
        fsh[1].handleFiles.file.z = stored; fsh[1].handleFiles.iwdIsClone = 1; fsh[1].zipFile = &parent;
        require(FS_Seek(1, 123, 2) == 0 && unztell(stored) == 123, "absolute FS seek failed");
        require(FS_Seek(1, -23, 0) == 0 && unztell(stored) == 100, "relative FS seek failed");
        require(FS_Seek(1, -10, 1) == 0 && unztell(stored) == long(data.size()-10), "FS end seek failed");
        require(FS_Seek(1, 0, 0) == 0 && FS_Seek(1, -1, 2) == -1, "FS seek boundary error");
        require(FS_Seek(1, 1, 1) == -1 && FS_Seek(1, 0, 9) == -1, "invalid FS seek accepted");
        memset(&fsh[1], 0, sizeof(fsh[1]));
        for (unsigned position : {0u, 500000u, 130000u, 999000u, 16u}) {
            checkAt(stored, data, position, 1000);
            checkAt(deflated, data, position, 1000);
        }
        require(!memcmp(&original, archive, sizeof(original)), "shared archive metadata was mutated");
        require(unzSeekCurrentFile(stored, data.size()+1) != UNZ_OK, "seek beyond EOF accepted");
        require(unzSeekCurrentFile(deflated, data.size()) == UNZ_OK, "compressed EOF seek failed");
        require(unzReadCurrentFile(deflated, nullptr, 1) == 0, "read beyond EOF");
        unzCloseEntry(archive, stored); unzCloseEntry(archive, deflated);
        const unsigned warmed = opens;
        for (unsigned i = 0; i < 100; ++i) {
            auto entry = unzOpenEntry(path.c_str(), archive, positions.at(i % 2 ? "stored" : "deflated"));
            require(entry, "reuse failed"); checkAt(entry, data, 100, 1000); unzCloseEntry(archive, entry);
        }
        require(opens == warmed, "cached readers reopened the archive");
        std::atomic<bool> failed{false};
        std::vector<std::thread> workers;
        for (unsigned worker = 0; worker < 8; ++worker) workers.emplace_back([&, worker] {
            try {
                for (unsigned i = 0; i < 10; ++i) {
                    auto entry = unzOpenEntry(path.c_str(), archive, positions.at(worker % 2 ? "stored" : "deflated"));
                    require(entry, "concurrent open failed");
                    checkAt(entry, data, worker * 10000, 1000);
                    checkAt(entry, data, 0, unsigned(data.size()));
                    unzCloseEntry(archive, entry);
                }
            } catch (...) { failed = true; }
        });
        for (auto &worker : workers) worker.join();
        require(!failed, "concurrent readers interfered");
        require(opens - closes == 3, "archive reader pool is not bounded");
        require(!unzOpenEntry(path.c_str(), archive, 0), "invalid entry offset accepted");
        auto entry = unzOpenEntry(path.c_str(), archive, positions.at("deflated"));
        require(entry, "error test open failed"); failRead = true;
        require(unzSeekCurrentFile(entry, 100000) < 0, "failed compressed read reported successful seek");
        unzCloseEntry(archive, entry);
        entry = unzOpenEntry(path.c_str(), archive, positions.at("stored")); require(entry, "shutdown test open failed");
        fsh[1].handleFiles.file.z = entry; fsh[1].handleFiles.iwdIsClone = 1; fsh[1].zipFile = &parent;
        memset(fsh[1].name, 'x', sizeof(fsh[1].name));
        require(!fsh[1].fileSize, "shutdown fixture must model the read-only open path");
        fs_searchpaths = archive;
        FS_Shutdown();
        const fileHandleData_t cleared{};
        require(!memcmp(&fsh[1], &cleared, sizeof(cleared)), "file close retained part of the LP64 handle");
        require(opens == closes, "filesystem shutdown leaked active or cached readers");
        fs_searchpaths = unzOpen(path.c_str()); require(fs_searchpaths, "filesystem startup failed");
        const std::string textPath = root + "/console.log", appendPath = root + "/append.log", binaryPath = root + "/output.bin";
        const char seed[] = "existing\n";
        { std::ofstream seedFile(appendPath, std::ios::binary); seedFile << seed; }
        int outputs[] = {
            FS_FOpenTextFileWrite(const_cast<char *>(textPath.c_str())),
            FS_FOpenFileAppend(const_cast<char *>(appendPath.c_str())),
            FS_GetHandleAndOpenFile("output.bin", binaryPath.c_str(), FS_THREAD_MAIN)
        };
        for (int h : outputs) require(h > 0, "output open failed");
        fsh[outputs[1]].handleSync = 1;
        FS_Shutdown();
        std::string written;
        for (unsigned restart = 0; restart < 20; ++restart) {
            archive = unzOpen(path.c_str()); require(archive, "restart archive open failed");
            parent.handle = archive;
            for (int h : {1, 50, 65}) {
                fsh[h].handleFiles.file.z = unzOpenEntry(path.c_str(), archive, positions.at("stored"));
                require(fsh[h].handleFiles.file.z, "restart entry open failed");
                fsh[h].handleFiles.iwdIsClone = 1; fsh[h].zipFile = &parent;
            }
            fsh[2].handleFiles.file.o = FS_FileOpenReadBinary((root + "/payload.bin").c_str());
            require(fsh[2].handleFiles.file.o, "loose read open failed");
            fsh[2].fileSize = int(data.size());
            fs_searchpaths = archive;
            char message[] = "before restart\n";
            for (int h : outputs) require(FS_Write(message, sizeof(message)-1, h) == sizeof(message)-1, "pre-restart write failed");
            written += message;
            FS_Shutdown();
            for (int h : {1, 2, 50, 65}) require(!fsh[h].handleFiles.file.o, "restart leaked a reader");
            require(opens - closes == 3, "restart lost an output or leaked archive readers");
            fs_searchpaths = unzOpen(path.c_str()); require(fs_searchpaths, "filesystem startup failed");
            char startup[] = "Current search path:\n";
            for (int h : outputs) require(FS_Write(startup, sizeof(startup)-1, h) == sizeof(startup)-1, "stale output handle after restart");
            written += startup;
            FS_Shutdown();
        }
        fs_searchpaths = unzOpen(path.c_str()); require(fs_searchpaths, "filesystem startup failed");
        for (int h : outputs) {
            FS_FCloseFile(h);
            require(!memcmp(&fsh[h], &cleared, sizeof(cleared)), "owner close retained an output handle");
        }
        FS_Shutdown();
        require(opens == closes, "owner shutdown leaked file handles");
        const auto textData = expected(textPath), appendData = expected(appendPath), binaryData = expected(binaryPath);
        require(std::string(textData.begin(), textData.end()) == written, "restart truncated or corrupted text output");
        require(std::string(appendData.begin(), appendData.end()) == seed + written, "restart corrupted append output");
        require(std::string(binaryData.begin(), binaryData.end()) == written, "restart corrupted binary output");
        printf("PASS: 20 filesystem restarts preserve caller-owned text, append and binary outputs and release readers\n");
        printf("PASS: concurrent archive readers, bounded reuse, stored/deflated seeks, EOF and I/O errors (%u opens)\n", opens.load());
    } catch (const std::exception &error) { fprintf(stderr, "FAIL: %s\n", error.what()); return 1; }
}
