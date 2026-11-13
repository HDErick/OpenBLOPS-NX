#if defined(KISAK_SP) && !defined(KISAK_DW)
#pragma once
#include <tomcrypt.h>

class bdECCKey
{
public:
    bdECCKey();
    ~bdECCKey();
    bool init();
    bool generateSharedSecret(const unsigned char *publicKey, unsigned int publicKeyLength,
        unsigned char *out, unsigned int outCapacity);
private:
    int m_initialized;
    ecc_key m_key;
};

static_assert(sizeof(void *) != 4 || sizeof(bdECCKey) == 0x20);
static_assert(sizeof(void *) != 4 || sizeof(ltc_math_descriptor) == 0xC4);
#endif
