#pragma once
#ifdef KISAK_SP
#include "cscr_instance.h"
#include <cstdint>
void Scr_ResetCheckpointIdentity_SP(scriptInstance_t inst);
void Scr_RecordCheckpointSource_SP(scriptInstance_t inst,const char* filename,const char* source,int size);
std::uint32_t Scr_CheckpointIdentity_SP(scriptInstance_t inst);
std::uint32_t Scr_CheckpointSourceCount_SP(scriptInstance_t inst);
#endif
