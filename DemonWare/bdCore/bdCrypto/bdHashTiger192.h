#pragma once
#include <cstddef>
#include <cstdint>
#include <libs/libtomcrypt-1.17/src/headers/tomcrypt.h>

// fuck whoever made demonware. Quote me on that

struct bdHashTiger192
{
    static constexpr std::size_t HASH_SIZE = 24; // 192 bits
#if defined(KISAK_SP) && !defined(KISAK_DW)
    bdHashTiger192();
    virtual ~bdHashTiger192() = default;
    virtual bool hash(const void *data, unsigned int length, unsigned char *out, unsigned int *outLength);
    static void operator delete(void *memory);
#endif

    static bool hash(
        const void *data,
        std::size_t length,
        uint8_t out[HASH_SIZE])
    {
        if (!data || !out)
            return false;

        hash_state md;

        if (tiger_init(&md) != 0)
            return false;

        if (tiger_process(&md,
            static_cast<const unsigned char *>(data),
            static_cast<unsigned long>(length)) != 0)
            return false;

        if (tiger_done(&md, out) != 0)
            return false;

        return true;
    }
};
#if defined(KISAK_SP) && !defined(KISAK_DW)
static_assert(sizeof(void *) != 4 || sizeof(bdHashTiger192) == 4);
#endif
