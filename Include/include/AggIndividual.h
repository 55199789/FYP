#ifndef __AGG_IND__
#define __AGG_IND__

#include <stdint.h>
#include "user_types.h"
#include "sgx_tcrypto.h"

uint32_t agg_individual(DATATYPE *data, DATATYPE *final_x, \
            uint8_t ctr[16], \
            const sgx_key_128bit_t *key, uint32_t dim);

#endif