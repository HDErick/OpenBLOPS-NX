// The Vulkan device's allocation callbacks (README-SWITCH.md): a small-object
// allocator for what NVK allocates per device object.
//
// DXVK passed no allocator, so Mesa used its default, malloc and aligned_alloc
// under newlib's one heap lock. DXVK allocates a descriptor set for most draws
// and NVK allocates each with vk_object_zalloc (T6ZM-Rebuild-NX run 64: about a
// third of dxvk-cs's busy time was spent in the allocator).
//
// Blocks up to kMaxSmall bytes with at most 16-byte alignment come from size
// classes 16 bytes apart, carved from 64 KB chunks and kept on a free list per
// class behind a libnx mutex (most of these calls come from dxvk-cs alone; not a
// spin lock: dxvk-cs runs above the compiler threads that also allocate here, and
// spinning while one of them holds the lock can keep that thread off its core).
// Everything else goes to memalign. A 16-byte header before each block says
// which: the class (1..kClasses), or 0 with the block's size and the pointer
// memalign returned. Chunks are never given back.
#include <switch.h>
#include <cstdlib>
#include <cstring>
#include <malloc.h>
#include <vulkan/vulkan_core.h>

namespace {

constexpr size_t kHeader = 16;
constexpr size_t kGranule = 16;
constexpr size_t kMaxSmall = 2048;
constexpr int kClasses = int(kMaxSmall / kGranule);
constexpr size_t kChunk = 64 * 1024;

struct Header {
    u32 cls;    // 1..kClasses: a small block; 0: memalign
    u32 size;   // the requested size of a memalign block
    void *raw;  // what memalign returned
};
static_assert(sizeof(Header) == kHeader, "header size");

struct FreeNode { FreeNode *next; };

struct SizeClass {
    Mutex lock;
    FreeNode *free;
    char *bump, *end;
};

SizeClass s_classes[kClasses];
u64 s_chunk_bytes;

void lock(Mutex *l) { mutexLock(l); }

void unlock(Mutex *l) { mutexUnlock(l); }

void *large_alloc(size_t size, size_t align)
{
    if (align < kHeader) align = kHeader;
    char *raw = static_cast<char *>(memalign(align, size + align));
    if (!raw) return nullptr;
    char *user = raw + align;
    Header *h = reinterpret_cast<Header *>(user - kHeader);
    h->cls = 0;
    h->size = u32(size);
    h->raw = raw;
    return user;
}

void *alloc(void *, size_t size, size_t align, VkSystemAllocationScope)
{
    if (align > kGranule || size > kMaxSmall)
        return large_alloc(size, align);
    const int cls = size ? int((size + kGranule - 1) / kGranule) : 1;
    SizeClass &c = s_classes[cls - 1];
    const size_t block = kHeader + size_t(cls) * kGranule;
    char *p;
    lock(&c.lock);
    if (c.free) {
        p = reinterpret_cast<char *>(c.free);
        c.free = c.free->next;
    } else {
        if (size_t(c.end - c.bump) < block) {
            char *chunk = static_cast<char *>(memalign(kGranule, kChunk));
            if (!chunk) { unlock(&c.lock); return nullptr; }
            __atomic_fetch_add(&s_chunk_bytes, kChunk, __ATOMIC_RELAXED);
            c.bump = chunk;
            c.end = chunk + kChunk;
        }
        p = c.bump;
        c.bump += block;
    }
    unlock(&c.lock);
    reinterpret_cast<Header *>(p)->cls = u32(cls);
    return p + kHeader;
}

void release(void *, void *memory)
{
    if (!memory) return;
    char *user = static_cast<char *>(memory);
    Header *h = reinterpret_cast<Header *>(user - kHeader);
    if (h->cls == 0) {
        free(h->raw);
        return;
    }
    SizeClass &c = s_classes[h->cls - 1];
    FreeNode *n = reinterpret_cast<FreeNode *>(user - kHeader);
    lock(&c.lock);
    n->next = c.free;
    c.free = n;
    unlock(&c.lock);
}

void *realloc_(void *user, void *memory, size_t size, size_t align, VkSystemAllocationScope scope)
{
    if (!memory) return alloc(user, size, align, scope);
    if (!size) { release(user, memory); return nullptr; }
    const Header *h = reinterpret_cast<const Header *>(static_cast<char *>(memory) - kHeader);
    const size_t old = h->cls ? size_t(h->cls) * kGranule : h->size;
    void *p = alloc(user, size, align, scope);
    if (p) {
        memcpy(p, memory, old < size ? old : size);
        release(user, memory);
    }
    return p;
}

const VkAllocationCallbacks s_callbacks = { nullptr, alloc, realloc_, release, nullptr, nullptr };

}  // namespace

extern "C" const VkAllocationCallbacks *NX_DxvkDeviceAllocator(void) { return &s_callbacks; }

extern "C" u64 NX_DxvkDeviceAllocatorBytes(void) { return __atomic_load_n(&s_chunk_bytes, __ATOMIC_RELAXED); }
