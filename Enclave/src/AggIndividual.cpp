#include "Enclave.h"
#include "Enclave_t.h"

#include <stdint.h>
#include "sgx_tcrypto.h"
#include "user_types.h"
#include "AggIndividual.h"

// data: in RAM,
// final_x: in Enclave
// Todo: add multi-threading
uint32_t agg_individual(DATATYPE *data, DATATYPE *final_x, \
            uint8_t ctr[16],
            const sgx_key_128bit_t &key, uint32_t dim) {
    // Aggregate individual's data into final_x 
    // First decrypt it
    DATATYPE *data_enclave = new DATATYPE[dim];
    if(sgx_aes_ctr_decrypt(&KEY_Z, (uint8_t *)data, \
                dim * sizeof(DATATYPE),\
                 ctr, 128, (uint8_t *)data_enclave) != SGX_SUCCESS)
            printf("\033[33m Clear final x: sgx_aes_ctr_encrypt failed \033[0m\n");
    for(int i=0;i<dim;i++)
        final_x[i] += data_enclave[i];
    delete[] data_enclave;
    return SGX_SUCCESS;
}
