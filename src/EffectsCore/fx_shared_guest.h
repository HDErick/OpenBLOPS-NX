#pragma once
struct FxSystem;
struct FxEffect;
struct refdef_s;
#ifdef KISAK_EXPERIMENTAL_SPLITSCREEN
bool FX_SplitGuestEffectVisible(const FxSystem *system, const FxEffect *effect);
bool FX_SplitGuestIsDrawing();
void FX_SplitGuestReset();
void FX_SplitGuestMuzzleFlash(const refdef_s *view);
void FX_DrawSharedGuest(const refdef_s *view);
#endif
