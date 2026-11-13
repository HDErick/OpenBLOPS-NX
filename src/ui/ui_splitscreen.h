#pragma once
#ifdef KISAK_EXPERIMENTAL_SPLITSCREEN
struct UiContext;
struct menuDef_t;
struct itemDef_s;
void UI_SplitScreenAddMenus(int localClientNum, UiContext *dc, menuDef_t *menu);
void UI_SplitScreenMenuOpening(int localClientNum, UiContext *dc, menuDef_t *menu);
bool UI_SplitScreenIsCustomMenu(const menuDef_t *menu);
bool UI_SplitScreenHandleEvent(int localClientNum, UiContext *dc, itemDef_s *item, const char *event);
bool UI_SplitScreenStartZombieMap(int localClientNum, const char *notify);
#endif
#ifdef KISAK_EXPERIMENTAL_SPLITSCREEN
bool UI_SplitScreenIsHiddenLayer(int contextIndex, const itemDef_s *item);
bool UI_SplitScreenStartMpMatch();
#endif
