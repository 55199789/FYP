#ifndef __UTILS_
#define __UTILS_
#include "sgx_tcrypto.h"
extern sgx_aes_ctr_128bit_key_t KEY_Z;
void ctr128_inc(uint8_t *counter);
#endif