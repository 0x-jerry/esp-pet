#ifndef BTSTACK_UECC_WRAPPER_H
#define BTSTACK_UECC_WRAPPER_H

#define uECC_SUPPORTS_secp256r1 1
#define uECC_NO_DEFAULT_RNG

#include "tinycrypt/ecc.h"
#include "tinycrypt/ecc_dh.h"

/* btstack micro-ecc compat macros */
#define uECC_BYTES NUM_ECC_BYTES

#endif
