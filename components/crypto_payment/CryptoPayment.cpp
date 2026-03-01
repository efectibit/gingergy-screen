#include "CryptoPayment.h"

CryptoPayment::CryptoPayment() {}

bool CryptoPayment::generatePayload(uint8_t  terminalId,
                                    uint16_t minutes,
                                    uint8_t* outBuf,
                                    size_t   outBufSize,
                                    size_t&  outLen)
{
    // A implementar por el usuario con su algoritmo
    outLen = 0;
    return true;
}

bool CryptoPayment::validatePIN(uint32_t pin,
                                uint8_t  terminalId,
                                uint16_t minutes)
{
    // A implementar por el usuario con su algoritmo
    return true;
}
