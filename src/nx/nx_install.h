#pragma once

#define NX_GAME_DIR "sdmc:/switch/OpenBLOPS-NX"
#define NX_SP_NRO "OpenBLOPS-NX.nro"
#define NX_MP_NRO "OpenBLOPS-NX-MP.nro"

#if defined(KISAK_SP)
#define NX_GAME_NRO NX_SP_NRO
#else
#define NX_GAME_NRO NX_MP_NRO
#endif
