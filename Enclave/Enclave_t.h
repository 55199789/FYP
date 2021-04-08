#ifndef ENCLAVE_T_H__
#define ENCLAVE_T_H__

#include <stdint.h>
#include <wchar.h>
#include <stddef.h>
#include "sgx_edger8r.h" /* for sgx_ocall etc. */

#include "user_types.h"

#include <stdlib.h> /* for size_t */

#define SGX_CAST(type, item) ((type)(item))

#ifdef __cplusplus
extern "C" {
#endif

uint32_t ecall_clear_final_x(DATATYPE* final_x, uint32_t dim);
uint32_t ecall_aggregate(DATATYPE* dataMat, Vector* compressedData, Vector* compressedX, DATATYPE* final_x, uint8_t* keys, uint32_t clientNum, uint32_t dim, uint32_t k);

sgx_status_t SGX_CDECL ocall_print_string(const char* str);
sgx_status_t SGX_CDECL ocall_gettime(double* retval, const char* name, int is_end);
sgx_status_t SGX_CDECL ocall_encrypt(uint8_t* keys, DATATYPE* dataMat, Vector* compressedData, uint32_t clientNum, uint32_t dim, uint32_t k);
sgx_status_t SGX_CDECL ocall_compress(DATATYPE* dataMat, Vector* compressedData, uint32_t clientNum, uint32_t dim, uint32_t k);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
