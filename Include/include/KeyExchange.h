#ifndef __KEY_EXCHANGE_
#define __KEY_EXCHANGE_

#include "sgx.h"

void generate_key(sgx_key_128bit_t &dh_aek);

#endif