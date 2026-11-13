#pragma once

struct XZone;

int NX_TryLoadKbz(const char *zoneName, const char *ffFilename, XZone *zone);
void NX_FreeKbz(XZone *zone);
void NX_KBZ_StopReaders();
void NX_KBZ_AbortImageBatch();
