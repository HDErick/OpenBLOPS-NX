#pragma once
// The in-game updater (after the BO2 Switch port's nx/updater). At boot both
// executables ask GitHub for the newest release of HDErick/OpenBLOPS-NX
// (HDErick/rqm-updater-nx when that one cannot be reached or has none). When
// its tag differs from this build's NACP version, the popup
// (nx_update_menu.cpp) offers it; installing downloads OpenBLOPS-NX.zip and
// unpacks it at the SD card's root (the zip holds switch/OpenBLOPS-NX/...).
//
// Each file goes to <file>.new first and is then renamed into place. One that
// cannot be replaced while it is in use is named in update_pending.txt and
// moved by the next start (NX_UpdateApplyPending), before anything opens it.
#include <cstdint>

enum class NxUpdatePhase : int {
    Idle,         // nothing checked yet
    Checking,     // asking GitHub for the newest release
    Available,    // a release with another version and the zip
    UpToDate,     // the newest release is this version
    CheckFailed,  // offline, or GitHub did not answer (error)
    Downloading,  // the zip: done / total bytes
    Extracting,   // its files: done / total bytes
    Installed,    // every file replaced: restart
    Failed,       // the download or the unpacking failed (error)
};

struct NxUpdateInfo {
    NxUpdatePhase phase;
    uint64_t done, total;
    char current[32];   // this build
    char version[64];   // the release's tag
    char title[256];    // its name
    char notes[2048];   // its description
    char error[256];
};

const char *NX_UpdateCurrentVersion();
void NX_UpdateApplyPending();   // main(), before the engine opens any file
void NX_UpdateStartCheck();
void NX_UpdateStartInstall();
void NX_UpdateCancel();
void NX_UpdateStop();           // cancel and wait for the worker (exit, before the sockets close)
void NX_UpdateGetInfo(NxUpdateInfo &info);
