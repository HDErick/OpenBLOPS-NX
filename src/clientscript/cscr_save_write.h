#pragma once

#ifdef KISAK_SP
#include "cscr_main.h"

struct MemoryFile;

unsigned short Scr_WriteSaveId(unsigned short id, scriptInstance_t inst);
unsigned short Scr_ReadSaveId(unsigned short saveId, scriptInstance_t inst);

void Scr_PrepareSave(scriptInstance_t inst);
void Scr_SaveVariables(scriptInstance_t inst, MemoryFile *memFile);
#endif
