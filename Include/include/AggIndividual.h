#ifndef __AGG_IND__
#define __AGG_IND__

#include <stdint.h>
#include "user_types.h"
#include "sgx_tcrypto.h"

uint32_t agg_individual(DATATYPE *data, DATATYPE *final_x, \
            DATATYPE* data_enclave, uint8_t ctr[16], \
            const sgx_key_128bit_t *key, uint32_t dim);

uint32_t agg_individual(Element *data, DATATYPE *final_x, \
            Element *data_enclave, uint8_t ctr[16], \
            DATATYPE beta, const sgx_key_128bit_t *key, \
            uint32_t dim, uint32_t k, uint32_t st);

#endif