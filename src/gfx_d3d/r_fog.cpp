#include "r_fog.h"

#include <cstring>
#include "r_init.h"
#ifdef KISAK_SP
#include "r_rendercmds.h"
#endif

void __cdecl R_ClearFogs()
{
    unsigned int i; // [esp+0h] [ebp-4h]

    for ( i = 0; i < 4; ++i )
    {
        memset(&rg.clientFogs[i], 0, 0x190u);
        rg.clientFogs[i].index = 0;
    }
}

void __cdecl R_SetFogFromServer(
                unsigned int localClientNum,
                float start,
                float r,
                float g,
                float b,
                float density,
                float heightDensity,
                float baseHeight,
                float fogColorScale,
                float sunFogColorR,
                float sunFogColorG,
                float sunFogColorB,
                float sunFogDirX,
                float sunFogDirY,
                float sunFogDirZ,
                float sunFogStartAng,
                float sunFogEndAng,
                float fogMaxOpacity)
{
    GfxClientFog *fog; // [esp+0h] [ebp-4h]

    if ( localClientNum >= 4
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\gfx_d3d\\r_fog.cpp",
                    32,
                    0,
                    "localClientNum doesn't index ARRAY_COUNT(rg.clientFogs)\n\t%i not in [0, %i)",
                    localClientNum,
                    4) )
    {
        __debugbreak();
    }
    fog = &rg.clientFogs[localClientNum];
    fog->settings[1].fogStart = start;
    fog->settings[1].heightDensity = heightDensity;
    fog->settings[1].density = density;
    fog->settings[1].baseHeight = baseHeight;
    fog->settings[1].color[0] = r;
    fog->settings[1].color[1] = g;
    fog->settings[1].color[2] = b;
    fog->settings[1].color[3] = fogColorScale;
    fog->settings[1].sunFogColor[0] = sunFogColorR;
    fog->settings[1].sunFogColor[1] = sunFogColorG;
    fog->settings[1].sunFogColor[2] = sunFogColorB;
    fog->settings[1].sunFogColor[3] = fogMaxOpacity;
    fog->settings[1].sunFogDir[0] = sunFogDirX;
    fog->settings[1].sunFogDir[1] = sunFogDirY;
    fog->settings[1].sunFogDir[2] = sunFogDirZ;
    fog->settings[1].sunFogEndAng = sunFogEndAng;
    fog->settings[1].sunFogStartAng = sunFogStartAng;
    fog->settings[1].maxDensity = 100.0 * density;
}

void __cdecl R_GetFogSettings(float *fogSettings)
{
    float *fogSettingsa; // [esp+Ch] [ebp+8h]

    *fogSettings = rg.clientFogs[0].settings[1].fogStart;
    fogSettingsa = fogSettings + 1;
    *fogSettingsa++ = 1.0 / rg.clientFogs[0].settings[1].density;
    *fogSettingsa++ = rg.clientFogs[0].settings[1].baseHeight;
    *fogSettingsa++ = 1.0 / rg.clientFogs[0].settings[1].heightDensity;
    *fogSettingsa++ = rg.clientFogs[0].settings[1].color[0];
    *fogSettingsa++ = rg.clientFogs[0].settings[1].color[1];
    *fogSettingsa++ = rg.clientFogs[0].settings[1].color[2];
    *fogSettingsa++ = rg.clientFogs[0].settings[1].color[3];
    *fogSettingsa++ = rg.clientFogs[0].settings[1].sunFogColor[0];
    *fogSettingsa++ = rg.clientFogs[0].settings[1].sunFogColor[1];
    *fogSettingsa++ = rg.clientFogs[0].settings[1].sunFogColor[2];
    *fogSettingsa++ = rg.clientFogs[0].settings[1].sunFogDir[0];
    *fogSettingsa++ = rg.clientFogs[0].settings[1].sunFogDir[1];
    *fogSettingsa++ = rg.clientFogs[0].settings[1].sunFogDir[2];
    *fogSettingsa++ = rg.clientFogs[0].settings[1].sunFogStartAng;
    *fogSettingsa = rg.clientFogs[0].settings[1].sunFogEndAng;
    fogSettingsa[1] = rg.clientFogs[0].settings[1].sunFogColor[3];
}

void __cdecl R_SwitchFog(unsigned int localClientNum, unsigned int fogvar, int startTime, int transitionTime)
{
    GfxClientFog *fog; // [esp+8h] [ebp-4h]

    if ( localClientNum >= 4
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\gfx_d3d\\r_fog.cpp",
                    91,
                    0,
                    "localClientNum doesn't index ARRAY_COUNT(rg.clientFogs)\n\t%i not in [0, %i)",
                    localClientNum,
                    4) )
    {
        __debugbreak();
    }
    if ( fogvar > 4
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\gfx_d3d\\r_fog.cpp",
                    92,
                    0,
                    "%s\n\t(fogvar) = %i",
                    "(fogvar >= FOG_NONE && fogvar < FOG_COUNT)",
                    fogvar) )
    {
        __debugbreak();
    }
    fog = &rg.clientFogs[localClientNum];
    fog->index = fogvar;
    if ( fog->settings[2].density == 0.0 )
    {
        memcpy(&fog->settings[3], &fog->settings[fog->index], sizeof(fog->settings[3]));
        transitionTime = 0;
    }
    else
    {
        memcpy(&fog->settings[3], &fog->settings[2], sizeof(fog->settings[3]));
    }
    memcpy(&fog->settings[4], &fog->settings[fog->index], sizeof(fog->settings[4]));
    if ( transitionTime )
    {
        fog->settings[4].startTime = startTime;
        fog->settings[4].finishTime = transitionTime + startTime;
    }
    else
    {
        fog->settings[4].startTime = 0;
        fog->settings[4].finishTime = 0;
    }
}
#ifdef KISAK_SP

void __cdecl R_SetWaterFog_SP(const WaterFogDef *waterFog)
{
    GfxFog *fog = &frontEndDataOut->fogSettings;
    if (waterFog->density == 0.0f)
    {
        fog->startTime = 0;
        fog->finishTime = 0;
        fog->color[0] = 0.3137255012989044f;
        fog->color[1] = 0.250980406999588f;
        fog->color[2] = 0.0f;
        fog->color[3] = 0.0f;
        fog->fogStart = -100.0f;
        fog->density = 0.012f;
        fog->heightDensity = 0.01f;
        fog->baseHeight = -900.0f;
        fog->sunFogColor[0] = 1.0f;
        fog->sunFogColor[1] = 1.0f;
        fog->sunFogColor[2] = 1.0f;
        fog->sunFogColor[3] = 1.0f;
        fog->sunFogDir[0] = 1.0f;
        fog->sunFogDir[1] = 0.0f;
        fog->sunFogDir[2] = 0.0f;
        fog->sunFogStartAng = 0.0f;
        fog->sunFogEndAng = 0.0f;
    }
    else
    {
        fog->startTime = waterFog->startTime;
        fog->finishTime = waterFog->finishTime;
        fog->color[0] = waterFog->color[0];
        fog->color[1] = waterFog->color[1];
        fog->color[2] = waterFog->color[2];
        fog->color[3] = waterFog->color[3];
        fog->fogStart = waterFog->fogStart;
        fog->density = waterFog->density;
        fog->heightDensity = waterFog->heightDensity;
        fog->baseHeight = waterFog->baseHeight;
        fog->sunFogColor[0] = waterFog->sunFogColor[0];
        fog->sunFogColor[1] = waterFog->sunFogColor[1];
        fog->sunFogColor[2] = waterFog->sunFogColor[2];
        fog->sunFogColor[3] = waterFog->sunFogColor[3];
        fog->sunFogDir[0] = waterFog->sunFogDir[0];
        fog->sunFogDir[1] = waterFog->sunFogDir[1];
        fog->sunFogDir[2] = waterFog->sunFogDir[2];
        fog->sunFogStartAng = waterFog->sunFogStartAng;
        fog->sunFogEndAng = waterFog->sunFogEndAng;
    }
    fog->maxDensity = fog->density;
}
#endif

