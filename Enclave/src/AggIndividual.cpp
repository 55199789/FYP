#include "Enclave.h"
#include "Enclave_t.h"

#include <stdint.h>
#include <algorithm> 
#include "sgx_tcrypto.h"
#include "user_types.h"
#include "AggIndividual.h"

// data: in RAM,
// final_x: in Enclave
// Todo: add multi-threading
uint32_t agg_individual(DATATYPE *data, DATATYPE *final_x, \
            DATATYPE* data_enclave, uint8_t ctr[16], \
            const sgx_key_128bit_t *key, uint32_t dim) {
    // Aggregate individual's data into final_x 
    // First decrypt it
    uint32_t ret = sgx_aes_ctr_decrypt(key, (uint8_t *)data, \
                dim * sizeof(DATATYPE),\
                 ctr, 128, (uint8_t *)data_enclave);
    if(ret != SGX_SUCCESS) {
            printf("\033[33m Decrypt user data: \
                sgx_aes_ctr_decrypt failed \033[0m\n");
            return ret;
    }
    // Aggregate
    for(int i=0;i<dim;i++)
        final_x[i] += data_enclave[i];
    return SGX_SUCCESS;
}

uint32_t agg_individual(Element *data, DATATYPE *final_x, \
            Element* data_enclave, uint8_t ctr[16], \
            DATATYPE beta, const sgx_key_128bit_t *key, \
            uint32_t dim, uint32_t k, uint32_t st) {
    // Aggregate compressed data 
    // First decrypt beta
    DATATYPE beta_en;
    uint32_t ret = sgx_aes_ctr_decrypt(key, (uint8_t *)data, \
                k * sizeof(Element),\
                 ctr, 128, (uint8_t *)data_enclave);
    if(ret != SGX_SUCCESS) {
            printf("\033[33m Decrypt user data: \
                sgx_aes_ctr_decrypt failed \033[0m\n");
            return ret;
    }

    ret = sgx_aes_ctr_decrypt(key, (uint8_t *)&beta, \
                sizeof(DATATYPE),\
                ctr, 128, (uint8_t *)&beta_en);
    if(ret != SGX_SUCCESS) {
            printf("\033[33m Decrypt user data: \
                sgx_aes_ctr_decrypt failed \033[0m\n");
            return ret;
    }
    // Find left and right bounds
    Element tmp = {st, 0};
    Element* lb = std::lower_bound(data_enclave, data_enclave+k, \
            tmp, \
            [](const Element &a, const Element &b)->bool { \
                return a.idx<b.idx; 
            });
    uint32_t lb_ = lb-data_enclave;
    for(int i=lb_;i<k;i++) {
        if(data_enclave[i].idx>=st+dim)
            break;
        else final_x[data_enclave[i].idx-st] += \
                beta*(data_enclave[i].sign?1:-1);
    }
    return SGX_SUCCESS;
}