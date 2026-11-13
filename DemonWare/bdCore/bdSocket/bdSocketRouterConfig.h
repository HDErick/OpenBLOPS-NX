#pragma once

#if defined(KISAK_SP) && !defined(KISAK_DW)
#include <DemonWare/bdPlatform/bdPlatformLog/bdPlatformLog.h>

struct bdSocketRouterConfig
{
    bool m_ignoreConnectionReset;
    unsigned char m_padding[3];
    float m_receiveTimeout;
    unsigned int m_maxConnectionResets;

    bdSocketRouterConfig()
    {
        m_ignoreConnectionReset = true;
        m_receiveTimeout = 121.0f;
        m_maxConnectionResets = 10;
    }

    void sanityCheckConfig() const
    {
        const char *source = "C:\\projects_pc\\cod\\codsrc\\DemonWare\\bdSocket\\bdSocketRouterConfig.cpp";
        const char *method = "bdSocketRouterConfig::sanityCheckConfig";
        if (m_ignoreConnectionReset != true)
            bdLogMessage(BD_LOG_WARNING, "warn/", "socketRouterConfig", source, method, 49,
                "Ignore connection reset differs from recommended setting of %s", m_ignoreConnectionReset ? "False" : "True");
        if (m_receiveTimeout > 150.0f || m_receiveTimeout < 10.0f)
            bdLogMessage(BD_LOG_WARNING, "warn/", "socketRouterConfig", source, method, 55,
                "Receive timeout configured to %f. This is outside the normal recommended range", static_cast<double>(m_receiveTimeout));
        if (m_maxConnectionResets > 100 || m_maxConnectionResets < 1)
            bdLogMessage(BD_LOG_WARNING, "warn/", "socketRouterConfig", source, method, 61,
                "Max connection resets to ignore configured to %u. This is outside the normal recommended range", m_maxConnectionResets);
    }
};
static_assert(sizeof(bdSocketRouterConfig) == 12);
#endif
