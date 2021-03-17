#ifndef __KEY_EXCHANGE_
#define __KEY_EXCHANGE_

#include "sgx.h"

uint32_t generate_key(double &totTime, sgx_key_128bit_t &dh_aek);

#endif