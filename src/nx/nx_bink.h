#pragma once

#include <binklib/bink.h>
#include <cstddef>

HBINK NX_BinkOpenMemory(const void *data, size_t size, unsigned flags);
bool NX_BinkFinished(HBINK bink);
