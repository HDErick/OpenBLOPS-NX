#pragma once

void __cdecl CG_InitBolt(unsigned int localClientNum);
unsigned int __cdecl CG_AddBolt(
                int localClientNum,
                int targetEntityNum,
                unsigned int weapon,
                const float *position,
                const float *normal,
                unsigned __int8 tagIndex,
#ifdef KISAK_NX
                intptr_t physUserBody);
#else
                int physUserBody);
#endif
void __cdecl CG_UpdateBolt(int localClientNum);
