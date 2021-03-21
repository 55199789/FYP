#include "Enclave.h"
#include "Enclave_t.h"

#include <stdint.h>
#include "sgx_tcrypto.h"
#include "user_types.h"
#include "AggIndividual.h"
#include "threads_conf.h"

std::atomic<bool> sync_flag_[THREAD_NUM];
std::function<void()> fun_ptrs[THREAD_NUM];

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
    // for(int t=0;t<THREAD_NUM;t++) {
    //     const int st = dim / THREAD_NUM * t;
    //     const int ed = t==THREAD_NUM-1?dim:dim/THREAD_NUM * (t+1);
    //     for(int i=st;i<ed;i++)
    // }
    for(int i=0;i<dim;i++)
        final_x[i] += data_enclave[i];
    return SGX_SUCCESS;
}
