#pragma once

#include "cscr_instance.h"

struct MemoryFile;

unsigned int Scr_ReadObjectReference(scriptInstance_t inst, MemoryFile *memFile, unsigned int header);
void Scr_LoadVariables(scriptInstance_t inst, MemoryFile *memFile);
void Scr_PostLoadVariables(scriptInstance_t inst);
