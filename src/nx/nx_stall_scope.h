#pragma once

// Coarse hang breadcrumbs stay available when nx_telemetry is off. Reuse
// the profiler's lock-free scope report without timestamps or GPU queries.
#ifdef KISAK_NX
#include <universal/profile.h>

struct NxStallScope
{
    int slot;
    explicit NxStallScope(NxProfSite *site) : slot(NxProf_ThreadSlot())
    {
        if (slot >= 0)
        {
            const int depth = g_nxProfDepth[slot];
            if (depth >= 0 && depth < NX_PROF_STACK)
                g_nxProfStack[slot][depth] = site;
            g_nxProfDepth[slot] = depth + 1;
        }
    }
    ~NxStallScope()
    {
        if (slot >= 0)
            --g_nxProfDepth[slot];
    }
    NxStallScope(const NxStallScope &) = delete;
    NxStallScope &operator=(const NxStallScope &) = delete;
};

#define NX_STALL_SCOPED(name) \
    static NxProfSite NX_PROF_CAT(nxStallSite_, __LINE__)(name); \
    NxStallScope NX_PROF_CAT(nxStallScope_, __LINE__)(&NX_PROF_CAT(nxStallSite_, __LINE__))
#endif
