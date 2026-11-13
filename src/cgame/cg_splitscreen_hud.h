#pragma once
#include <ui/ui_shared.h>

#ifdef KISAK_EXPERIMENTAL_SPLITSCREEN
struct snapshot_s;
void CG_SplitGuestCaptureHudText(int localClientNum, UiContext *dc, itemDef_s *item);
void CG_SplitGuestResetMpHud();
void CG_SplitGuestUpdateMpHud(snapshot_s *snapshot);
void CG_SplitGuestDrawMpHudText();

// Capture layout/style only; replay reads the guest's own player state. No menu
// object, UI context or client-info animation ownership is transferred.
void CG_SplitGuestCaptureHudWidget(int localClientNum, bool text, int ownerDraw,
    const rectDef_s &parent, const rectDef_s &rect, int flags, const float *color,
    Material *material, Font_s *font = nullptr, float scale = 0.0f,
    float textX = 0.0f, float textY = 0.0f, int align = 0,
    int textStyle = 0, int textAlignMode = 0);
#endif
