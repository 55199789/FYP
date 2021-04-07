#include "Enclave.h"
#include "Enclave_t.h" /* print_string */
#include <math.h>
#include <stdarg.h>
#include <stdio.h> /* vsnprintf */
#include <string.h>
#include <algorithm>
#include "sgx_ecp_types.h"
#include "sgx_dh.h"
#include "sgx_tcrypto.h"
#include "minHeap.h"
#include "KeyExchange.h"
#include "AggIndividual.h"
#include "utils.h"
#include "user_types.h"

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

static void calcLoadedDim(uint32_t &loadedDim, uint32_t dim, uint32_t k) {
    loadedDim = dim<(90<<19) / sizeof(DATATYPE)? \
                    dim:(90<<19) / sizeof(DATATYPE);
    if (k>0&&k<dim) {
        loadedDim = dim<((90<<20)-1.5*k*sizeof(Element))/sizeof(DATATYPE)? \
                    dim:((90<<20)-1.5*k*sizeof(Element))/sizeof(DATATYPE);
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

uint32_t ecall_aggregate(DATATYPE *dataMat, \
            Vector *compressedData, \
            Vector *compressedX, \
            DATATYPE *final_x, \
            uint8_t *keys, \
            const uint32_t clientNum, const uint32_t dim, \
            const uint32_t k) {
    double t_enter, t_keys = 0, t_aggregation, \
        t_keysC = 0, t_compress = 0;
    ocall_gettime(&t_enter, "Enter enclave", 1);
    printf("%sTime of entering the enclave: %fms%s\n", KRED, t_enter*1000, KNRM);
    // 90MB available; Half for final_x, and half for user data 
    uint32_t loadedDim;
    calcLoadedDim(loadedDim, dim, k);
    if (k>0&&k<dim) {
        if (compressedData==NULL) {
            printf("Compressed data file missing!\n");
            return SGX_ERROR_INVALID_PARAMETER;
        }
    }
    uint32_t times = dim/loadedDim;
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
    Vector compressedXEnclave;
    DATATYPE *heap = NULL;
    DATATYPE *kth = NULL;
    uint8_t *data_enclave = NULL;
    if (k>0&&k<dim) 
        data_enclave = new uint8_t[k*sizeof(Element)]; 
    else data_enclave = new uint8_t[loadedDim*sizeof(DATATYPE)];

    for(int i=0;i<clientNum;i++) {
        // Generate Session Key
        ret = generate_key(t_keys, t_keysC, \
                        client_keys[i]);
        if (ret!=SGX_SUCCESS) {
            printf("Generate session key failed...\n");
            goto ret;
        }
        memcpy(keys+16*i, client_keys+i, 16);
    }
    printf("%sTime of key exchanges for %d clients: %fms\n%s\n", KRED, \
        clientNum, t_keys*1000, KNRM);
    printf("%sTime of key exchanges per client: %fms\n%s\n", KRED, \
                t_keysC*1000/clientNum, KNRM);

    printf("Encrpyting user data...\n");
    if (k) {
        ocall_compress(dataMat, compressedData, clientNum, dim, k);
    } 
    ocall_encrypt(keys, dataMat, compressedData, clientNum, dim, k);
    printf("Encprtion completed.\n\n");

// Testing code
printf("[DEBUG] Times: %d, loaded dim: %d\n", times, loadedDim);
// End 

    ocall_gettime(&t_aggregation, "Aggregation", 0);
    for(int t=0;t<times;t++) {
        // Load final_x into enclave
        ret = sgx_aes_ctr_decrypt(&KEY_Z, (uint8_t*)(final_x+t*loadedDim), \
                    loadedDim * sizeof(DATATYPE), ctr_x_en, 128, \
                    (uint8_t*)final_x_enclave);
        if (ret!=SGX_SUCCESS) {
            printf("Decrypt final x into enclave faild.\n");
            goto ret;
        }
        for(int i=0;i<clientNum; i++) {
            if(k>0&&k<dim) {
                // Decrypt and aggregate the data
                ret = agg_individual(compressedData[i].elements, \
                            final_x_enclave, (Element*) data_enclave, \
                            ctr_clients[i], compressedData[i].beta, \
                            client_keys+i, loadedDim, k, t*loadedDim);
                if (ret!=SGX_SUCCESS) {
                    printf("Aggregate compressed client %d failed...\n", i);
                    goto ret;
                }
            } else {
                // Decrypt and aggregate the data
                ret = agg_individual(dataMat + i*dim + t*loadedDim, \
                            final_x_enclave, (DATATYPE*) data_enclave, \
                            ctr_clients[i], client_keys+i, loadedDim);
                if (ret!=SGX_SUCCESS) {
                    printf("Aggregate client %d failed...\n", i);
                    goto ret;
                }
            }
        }
        // Normalization
        for(int i=0;i<loadedDim; i++) final_x_enclave[i] /= clientNum;
        // Move final_x from enclave to RAM 
        // memcpy(final_x + t*loadedDim, final_x_enclave, \
        //         sizeof(DATATYPE) *loadedDim);
        // Encryption
        ret = sgx_aes_ctr_encrypt(&KEY_Z, (uint8_t*)final_x_enclave, \
                    loadedDim * sizeof(DATATYPE), ctr_x_de, 128, \
                    (uint8_t*)final_x + t*loadedDim);
        if (ret!=SGX_SUCCESS) {
            printf("Encrypt final x into RAM faild.\n");
            goto ret;
        }
    }
    // Process the remaining part
    if(dim%loadedDim) { 
        const uint32_t remaining = dim % loadedDim;
        // Load final_x into enclave
        ret = sgx_aes_ctr_decrypt(&KEY_Z, \
                        (uint8_t*)(final_x+times*loadedDim), \
                        remaining * sizeof(DATATYPE), ctr_x_en, 128, \
                        (uint8_t*)final_x_enclave);
        if (ret!=SGX_SUCCESS) {
            printf("Decrypt final x into enclave faild.\n");
            goto ret;
        }
        for(int i=0;i<clientNum; i++) {if(k>0&&k<dim) {
            // Decrypt and aggregate the data
            ret = agg_individual(compressedData[i].elements, \
                        final_x_enclave, (Element*) data_enclave, \
                        ctr_clients[i], compressedData[i].beta, \
                        client_keys+i, loadedDim, remaining, \
                        times*loadedDim);
            if (ret!=SGX_SUCCESS) {
                printf("Aggregate compressed client %d failed...\n", i);
                goto ret;
            }
            } else {
                // Decrypt and aggregate the data
                ret = agg_individual(dataMat + i*dim + times*loadedDim, \
                            final_x_enclave, (DATATYPE*)data_enclave, \
                            ctr_clients[i], client_keys+i, remaining);
                if (ret!=SGX_SUCCESS) {
                    printf("Aggregate client %d failed...\n", i);
                    goto ret;
                }
            }
        }
        // Normalization
        for(int i=0;i<remaining; i++) final_x_enclave[i] /= clientNum;
        // Move final_x from enclave to RAM 
        // memcpy(final_x + times*loadedDim, final_x_enclave, \
        //         sizeof(DATATYPE) *remaining);
        // Encryption
        ret = sgx_aes_ctr_encrypt(&KEY_Z, (uint8_t*)final_x_enclave, \
                    remaining * sizeof(DATATYPE), ctr_x_de, 128, \
                    (uint8_t*)final_x + times*loadedDim);
        if (ret!=SGX_SUCCESS) {
            printf("Encrypt final x into RAM faild.\n");
            goto ret;
        }
    }
    ocall_gettime(&t_aggregation, "Aggregation", 1);
    printf("%sTime of secure aggregation: %fms%s\n\n", KRED, \
            t_aggregation*1000, KNRM);
    printf("%s[INFO] Aggregate successfully.\n%s", KYEL, KNRM);

    // Compress X for each client
    delete[] data_enclave;
    if (k<=0 || k>=dim)
        return SGX_SUCCESS;
    ocall_gettime(&t_compress, "\0", 0);
    if (compressedX==NULL) {
        printf("compressedX is NULL!\n");
        return SGX_ERROR_INVALID_PARAMETER;
    }
    memset(ctr_x_en, 0, sizeof(ctr_x_en));
    memset(ctr_x_de, 0, sizeof(ctr_x_de));
    memset(ctr_clients, 0, sizeof(ctr_clients));
    if (k>=0.02*dim) {
        heap = new DATATYPE[dim];
        for(int t=0;t<times;t++) {
            sgx_aes_ctr_decrypt(&KEY_Z, (uint8_t*)final_x+t*loadedDim, \
                        loadedDim * sizeof(DATATYPE), ctr_x_de, 128, \
                        (uint8_t*)(heap+t*loadedDim));
        }
        if(dim%loadedDim) {
            uint32_t remaining = dim - times * loadedDim;
            sgx_aes_ctr_decrypt(&KEY_Z, (uint8_t*)final_x+times*loadedDim, \
                        remaining * sizeof(DATATYPE), ctr_x_de, 128, \
                        (uint8_t*)(heap+times*loadedDim));
        }
        kth = heap+k-1; 
        std::nth_element(heap, kth, heap+dim, \
                [](const DATATYPE &a, const DATATYPE &b)->bool {
                    return abs(a)>abs(b);
                });
    } else {
        heap = new DATATYPE[k];
        sgx_aes_ctr_decrypt(&KEY_Z, (uint8_t*)final_x, \
                    k * sizeof(DATATYPE), ctr_x_de, 128, \
                    (uint8_t*)final_x_enclave);
        init(heap, final_x_enclave, k);
        for(int i=k;i<dim;i+=loadedDim) {
            const int lDim = dim-i<loadedDim?\
                dim-i:loadedDim;
            sgx_aes_ctr_decrypt(&KEY_Z, (uint8_t*)final_x+i, \
                        lDim * sizeof(DATATYPE), ctr_x_de, 128, \
                        (uint8_t*)final_x_enclave);
            for(int j=0;j<lDim;j++)
                insert(heap, k, abs(final_x_enclave[j]));
        }
        kth = heap;
    }
    memset(ctr_x_de, 0, sizeof(ctr_x_de));
    {
    DATATYPE beta = 0;
    int cnt = 0;
    compressedXEnclave.elements = new Element[k];
    for(int i=0;i<dim;i+=loadedDim) {
        const int lDim = dim-i<loadedDim?\
            dim-i:loadedDim;
        sgx_aes_ctr_decrypt(&KEY_Z, (uint8_t*)final_x+i, \
                    lDim * sizeof(DATATYPE), ctr_x_de, 128, \
                    (uint8_t*)final_x_enclave);
        for(int j=0;j<lDim;j++) {
            if(abs(final_x_enclave[j]>=*kth))
                compressedXEnclave.elements[cnt++] = {
                    j+i, final_x_enclave[j]>0
                };
            beta += final_x_enclave[j]*final_x_enclave[j];
        }
    }
    compressedXEnclave.beta = sqrt(beta)/sqrt(k);
    for(int i=0;i<clientNum;i++) {
        sgx_aes_ctr_encrypt((sgx_key_128bit_t *)(keys+16*i), \
            (uint8_t*)compressedX[i].elements, \
            sizeof(Element)*k, ctr_clients[i], 128, \
            (uint8_t *)compressedX[i].elements);
        sgx_aes_ctr_encrypt((sgx_key_128bit_t *)(keys+16*i), \
            (uint8_t*)compressedX[i].elements, \
            sizeof(DATATYPE), ctr_clients[i], 128, \
            (uint8_t *)&(compressedX[i].beta));
    }
    }
    ocall_gettime(&t_compress, "\0", 1);
    printf("%sTotal time of compression: %fms%s\n\n", KRED, \
            t_compress*1000, KNRM);
    printf("%sTotal time inside the Enclave: %fms%s\n\n", KRED, \
            (t_enter + t_keys + t_aggregation + t_compress)*1000, KNRM);
    printf("%s[INFO] Aggregate successfully.\n%s", KYEL, KNRM);
ret:
    delete[] client_keys;
    delete[] data_enclave;
    delete[] final_x_enclave;
    delete[] heap;
    return ret;
}