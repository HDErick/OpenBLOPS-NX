#pragma once
#include <cstdint>

using u64 = uint64_t;
using u32 = uint32_t;
using s32 = int32_t;
using Handle = u32;
using Result = u32;

constexpr Handle CUR_PROCESS_HANDLE = 1, CUR_THREAD_HANDLE = 2;
enum InfoType { InfoType_CoreMask, InfoType_PriorityMask };
#define R_SUCCEEDED(result) ((result) == 0)
#define R_FAILED(result) ((result) != 0)

Result svcGetInfo(u64*, InfoType, Handle, u64);
Result svcGetThreadPriority(s32*, Handle);
Result svcSetThreadPriority(Handle, s32);
Result svcSetThreadCoreMask(Handle, s32, u32);
