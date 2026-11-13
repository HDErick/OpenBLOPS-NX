#pragma once
#include <universal/local_client_limits.h>

int __cdecl CL_LocalClient_GetActiveCount();
char __cdecl CL_LocalClient_IsActive(int localClientNum);
char __cdecl CL_LocalClient_IsFirstActive(int localClientNum);
char __cdecl CL_AllLocalClientsDisconnected();
char __cdecl CL_AnyLocalClientStateActive();
char __cdecl CL_AnyLocalClientChallenging();
void __cdecl CL_LocalClients_SetAllUsedActive();
int __cdecl CL_LocalActiveIndexFromClientNum(int localClientNum);
const char *__cdecl CL_ControllerIndex_GetUsername();

// Pixel bounds for one full-screen view or two top/bottom views. This is kept
// independent of activation while the per-client renderer is being restored.
struct LocalClientViewport
{
    int x, y, width, height;
};

inline bool CL_CalculateLocalClientViewport(int width, int height, int clientIndex,
    int clientCount, LocalClientViewport *viewport)
{
    if (!viewport || width <= 0 || height <= 0 || clientCount < 1 || clientCount > 2
        || clientIndex < 0 || clientIndex >= clientCount || height < clientCount)
        return false;
    viewport->x = 0;
    viewport->width = width;
    viewport->y = clientCount == 2 && clientIndex == 1 ? height / 2 : 0;
    viewport->height = clientCount == 1 ? height
        : clientIndex == 0 ? height / 2 : height - height / 2;
    return true;
}

#ifdef KISAK_EXPERIMENTAL_SPLITSCREEN
bool CL_SplitScreenSessionRequested();
void CL_SplitScreenResetSession();
void CL_SplitScreenFrontendEntered();
void CL_SplitScreenSessionActivated();
bool CL_SplitScreenCanStartMap(const char *map);
bool CL_SplitScreenStartMap(const char *map);
void CL_SplitScreenMapLoading(const char *map);
struct playerState_s;
struct usercmd_s;
void CL_SplitGuestResetPrediction();
void CL_SplitGuestUpdatePrediction();
const playerState_s *CL_SplitGuestPredictedState();
bool CL_SplitGuestPredictedCamera(float *origin, float *angles);
bool CL_SplitGuestPredictionRunning();
const char *CL_SplitGuestPredictionStatus();
const char *CL_SplitGuestScriptMainMenu();
bool CL_SplitGuestDrawingMenu();
void CL_SplitGuestDrawMenu();
bool CL_SplitGuestOwnsMenu();
bool CL_SplitGuestMenuAcceptsUiInput();
bool CL_SplitGuestMenuBlocksInput();
void CL_SplitGuestResetMenu();
bool CL_SplitGuestMenuResponse(const char *menu, const char *response);
void CL_SplitGuestHandleMenuCommand(const char *text);
void CL_SplitGuestMenuFrame();
void CL_SplitGuestRegisterInputDvars();
void CL_SplitGuestResetInput();
void CL_SplitGuestSelectWeapon(unsigned int weapon);
bool CL_SplitGuestOwnsController(int port);
void CL_SplitGuestBuildInput(usercmd_s *command, const playerState_s *player,
    int realtime, int frameMsec, bool enabled);
bool CL_SplitGuestGetViewAngles(float *angles);
const char *CL_SplitGuestInputStatus();
// The guest camera/input share the loaded world without activating guest UI.
struct refdef_s;
void CG_SplitGuestResetHud();
bool CG_SplitGuestDrawHud(const refdef_s *view);
const char *CG_SplitGuestHudStatus();
void CL_SplitGuestResetPresentation();
void CL_SplitGuestSmoothCamera(refdef_s *view);
void CL_SplitGuestApplyVision(refdef_s *view);
void CL_SplitGuestSetFxCamera();
bool CL_SplitGuestHasViewport();
bool CL_SplitGuestHasView();
bool CL_SplitGuestGetWorldCamera(float *origin, float (*axis)[3]);
void CL_SplitGuestUpdateSound();
void CL_SplitGuestApplyHostViewport(refdef_s *view);
void CL_SplitGuestRender();
int CL_SplitGuestExpectedPlayers();
void CL_SplitGuestBeginMap();
bool CL_IsSplitGuest(int localClientNum);
bool CL_SplitGuestPrime(int localClientNum);
void CL_SplitGuestLog(const char *event);
void CL_SplitGuestRegisterCommands();
void CL_SplitGuestFrame();
void CL_SplitGuestRemove(bool notifyServer);
#endif
