#include "Enclave.h"
#include "Enclave_t.h" /* print_string */
#include <stdarg.h>
#include <stdio.h> /* vsnprintf */
#include <string.h>
#include "sgx_ecp_types.h"
#include "sgx_dh.h"
#include "threads_conf.h"
#include "sgx_tcrypto.h"
#include "KeyExchange.h"
#include "AggIndividual.h"
#include "utils.h"
#include "user_types.h"
//#include <sgx_intrin.h>

//#include "xmmintrin.h"
//#include "immintrin.h"


/*
 * printf:
 *   Invokes OCALL to display the enclave buffer to the terminal.
 */


extern "C"
int puts(const char *str)
{
    printf(str);
    printf("\n");
    return 0;
}

extern "C"
int printf(const char* fmt, ...)
{
    char buf[BUFSIZ] = { '\0' };
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, BUFSIZ, fmt, ap);
    va_end(ap);
    ocall_print_string(buf);
    return (int)strnlen(buf, BUFSIZ - 1) + 1;
}


// multi-threading
bool volatile is_exit = false;
extern std::atomic<bool> sync_flag_[THREAD_NUM];
extern std::function<void()> fun_ptrs[THREAD_NUM];


void task_notify(int threads) {
    for (int i = 1; i < threads; i++) {
        sync_flag_[i] = true;
    }
}

void ecall_loop(int tid) {
    printf("[ecall_loop][thread][%d][begin]\n", tid);

    while(true) {
        while(sync_flag_[tid] == false);

            if (is_exit == true)
                return;

            if (fun_ptrs[tid] == NULL)
                printf("[ecall_loop][%d][func is null]\n", tid);
            
            fun_ptrs[tid]();

            fun_ptrs[tid] = NULL;
            sync_flag_[tid] = false;
            // is_done[tid] = true;
    }
}

void task_wait_done(int threads) {
    for (int i = 1; i < threads; i++)
        while(sync_flag_[i] == true);
}

void ecall_threads_down() {
    is_exit = true;
    for (int i = 1; i < THREAD_NUM; i++)
        sync_flag_[i] = true;
}

void threads_init() {
    is_exit = false;

    for (int i = 0; i < THREAD_NUM; i++) {
        fun_ptrs[i] = NULL;
        sync_flag_[i] =false;
    }
}

uint32_t ecall_clear_final_x(DATATYPE *final_x, uint32_t dim) {
    uint8_t ctr[16] = {0};
    const uint32_t LDN = dim<(90<<20) / sizeof(DATATYPE)? \
                dim:(90<<20) / sizeof(DATATYPE); // 90 MB / (2*sizeof(dt))
    const uint32_t TMS = dim/LDN;
    DATATYPE *tmp = new DATATYPE[LDN];
// Testing code
printf("[DEBUG] loaded num: %d, times: %d\n", LDN, TMS);
// End testing
    memset(tmp, 0, sizeof(DATATYPE)*LDN);
    for(int i=0;i<TMS;i++)
        if(sgx_aes_ctr_encrypt(&KEY_Z, (uint8_t *)tmp, LDN * sizeof(DATATYPE),\
                 ctr, 128, (uint8_t *)(final_x + i*LDN)) != SGX_SUCCESS)
            printf("\033[33m Clear final x: sgx_aes_ctr_encrypt failed \033[0m\n");
    if(dim%LDN)
        if(sgx_aes_ctr_encrypt(&KEY_Z, (uint8_t *)tmp, \
                (dim - LDN*TMS) * sizeof(DATATYPE),\
                 ctr, 128, (uint8_t *)(final_x + TMS*LDN)) != SGX_SUCCESS)
            printf("\033[33m Clear final x: sgx_aes_ctr_encrypt failed \033[0m\n");
    delete[] tmp;    
    return SGX_SUCCESS;
}

uint32_t ecall_aggregate(DATATYPE *dataMat, DATATYPE *final_x, \
            uint8_t *keys,
            const uint32_t clientNum, const uint32_t dim) {
    double t_enter, t_keys = 0, t_aggregation;
    ocall_gettime(&t_enter, "Enter enclave", 1);
    printf("%sTime of entering the enclave: %fms%s\n", KRED, t_enter*1000, KNRM);
    // 90MB available; Half for final_x, and half for user data 
    const uint32_t loadedDim = dim<(90<<19) / sizeof(DATATYPE)? \
                        dim:(90<<19) / sizeof(DATATYPE);
    const uint32_t times = dim/loadedDim;
// Testing code
printf("[DEBUG] Sizeof all required data in enclave: %d KB\n", \
        (sizeof(DATATYPE)*loadedDim*2 + clientNum*16)>>10);
// End
    uint32_t ret = SGX_SUCCESS;
    sgx_key_128bit_t *client_keys = new sgx_key_128bit_t[clientNum];

    uint8_t ctr_x_en[16] = {0};
    uint8_t ctr_x_de[16] = {0};
    uint8_t ctr_clients[clientNum][16] = {0};
    DATATYPE *final_x_enclave = new DATATYPE[loadedDim];
    DATATYPE *data_enclave = new DATATYPE[loadedDim];

    for(int i=0;i<clientNum;i++) {
        // Generate Session Key
        ret = generate_key(t_keys, client_keys[i]);
        if (ret!=SGX_SUCCESS) {
            printf("Generate session key failed...\n");
            goto ret;
        }
        memcpy(keys+16*i, client_keys+i, 16);
    }
    printf("%sTime of key exchanges for %d clients: %fms\n%s\n", KRED, \
        clientNum, t_keys*1000, KNRM);

    printf("Encrpyting user data...\n");
    ocall_encrypt(keys, dataMat, clientNum, dim);
    printf("Encprtion completed.\n\n");

printf("[DEBUG] Times: %d, loaded dim: %d\n", times, loadedDim);

    ocall_gettime(&t_aggregation, "Aggregation", 0);
    for(int t=0;t<times;t++) {
        // Load final_x into enclave
        // printf("Loading %d to %d data.\n", t*loadedDim, (t+1)*loadedDim);
        ret = sgx_aes_ctr_decrypt(&KEY_Z, (uint8_t*)(final_x + t*loadedDim), \
                    loadedDim * sizeof(DATATYPE), ctr_x_en, 128, \
                    (uint8_t*)final_x_enclave);
        if (ret!=SGX_SUCCESS) {
            printf("Decrypt final x into enclave faild.\n");
            goto ret;
        }
        for(int i=0;i<clientNum; i++) {
            // Decrypt and aggregate the data
            ret = agg_individual(dataMat + i*dim + t*loadedDim, \
                        final_x_enclave, data_enclave, \
                        ctr_clients[i], client_keys+i, loadedDim);
            if (ret!=SGX_SUCCESS) {
                printf("Aggregate client %d failed...\n", i);
                goto ret;
            }
        }
        // Normalization
        for(int i=0;i<loadedDim; i++) final_x_enclave[i] /= clientNum;
        // Move final_x from enclave to RAM 
        memcpy(final_x + t*loadedDim, final_x_enclave, \
                sizeof(DATATYPE) *loadedDim);
        // Encryption
        // ret = sgx_aes_ctr_encrypt(&KEY_Z, (uint8_t*)final_x_enclave, \
        //             loadedDim * sizeof(DATATYPE), ctr_x_de, 128, \
        //             (uint8_t*)final_x + t*loadedDim);
        // if (ret!=SGX_SUCCESS) {
        //     printf("Encrypt final x into RAM faild.\n");
        //     goto ret;
        // }
    }
    // Process the remaining part
    if(dim%loadedDim) { 
        printf("Processing remaining part...\n");
        const uint32_t remaining = dim % loadedDim;
        // Load final_x into enclave
        ret = sgx_aes_ctr_decrypt(&KEY_Z, (uint8_t*)(final_x+times*loadedDim), \
                        remaining * sizeof(DATATYPE), ctr_x_en, 128, \
                        (uint8_t*)final_x_enclave);
        if (ret!=SGX_SUCCESS) {
            printf("Decrypt final x into enclave faild.\n");
            goto ret;
        }
        for(int i=0;i<clientNum; i++) {
            // Decrypt and aggregate the data
            ret = agg_individual(dataMat + i*dim + times*loadedDim, \
                        final_x_enclave, data_enclave, \
                        ctr_clients[i], client_keys+i, remaining);
            if (ret!=SGX_SUCCESS) {
                printf("Aggregate client %d failed...\n", i);
                goto ret;
            }
        }
        // Normalization
        for(int i=0;i<loadedDim; i++) final_x_enclave[i] /= clientNum;
        // Move final_x from enclave to RAM 
        memcpy(final_x + times*loadedDim, final_x_enclave, \
                sizeof(DATATYPE) *remaining);
        // Encryption
        // ret = sgx_aes_ctr_encrypt(&KEY_Z, (uint8_t*)final_x_enclave, \
        //             remaining * sizeof(DATATYPE), ctr_x_de, 128, \
        //             (uint8_t*)final_x + times*loadedDim);
        // if (ret!=SGX_SUCCESS) {
        //     printf("Encrypt final x into RAM faild.\n");
        //     goto ret;
        // }
    }
    ocall_gettime(&t_aggregation, "Aggregation", 1);
    printf("%sTime of secure aggregation: %fms%s\n\n", KRED, \
            t_aggregation*1000, KNRM);
    printf("%sTotal time inside the Enclave: %fms%s\n\n", KRED, \
            (t_enter + t_keys + t_aggregation)*1000, KNRM);
    printf("%s[INFO] Aggregate successfully.\n%s", KYEL, KNRM);
ret:
    delete[] client_keys;
    delete[] data_enclave;
    delete[] final_x_enclave;
    return ret;
}