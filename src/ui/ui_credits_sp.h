#pragma once

#ifdef KISAK_SP
#include <universal/dvar.h>

struct Font_s;
struct rectDef_s;

extern const dvar_t *creditsScrollScale;

void __cdecl UI_CreditsInit_SP();
void __cdecl UI_DrawCredits_SP(
    int localClientNum,
    const rectDef_s *rect,
    Font_s *font,
    float scale,
    const float *color,
    int textStyle);
#endif
