#ifndef ENCLAVE_U_H__
#define ENCLAVE_U_H__

#include <stdint.h>
#include <wchar.h>
#include <stddef.h>
#include <string.h>
#include "sgx_edger8r.h" /* for sgx_status_t etc. */

#include "user_types.h"

#include <stdlib.h> /* for size_t */

#define SGX_CAST(type, item) ((type)(item))

#ifdef __cplusplus
extern "C" {
#endif

#ifndef OCALL_PRINT_STRING_DEFINED__
#define OCALL_PRINT_STRING_DEFINED__
void SGX_UBRIDGE(SGX_NOCONVENTION, ocall_print_string, (const char* str));
#endif
#ifndef OCALL_GETTIME_DEFINED__
#define OCALL_GETTIME_DEFINED__
double SGX_UBRIDGE(SGX_NOCONVENTION, ocall_gettime, (const char* name, int is_end));
#endif
#ifndef OCALL_ENCRYPT_DEFINED__
#define OCALL_ENCRYPT_DEFINED__
void SGX_UBRIDGE(SGX_NOCONVENTION, ocall_encrypt, (uint8_t* keys, DATATYPE* dataMat, Vector* compressedData, uint32_t clientNum, uint32_t dim, uint32_t k));
#endif
#ifndef OCALL_COMPRESS_DEFINED__
#define OCALL_COMPRESS_DEFINED__
void SGX_UBRIDGE(SGX_NOCONVENTION, ocall_compress, (DATATYPE* dataMat, Vector* compressedData, uint32_t clientNum, uint32_t dim, uint32_t k));
#endif

sgx_status_t ecall_clear_final_x(sgx_enclave_id_t eid, uint32_t* retval, DATATYPE* final_x, uint32_t dim);
sgx_status_t ecall_aggregate(sgx_enclave_id_t eid, uint32_t* retval, DATATYPE* dataMat, Vector* compressedData, Vector* compressedX, DATATYPE* final_x, uint8_t* keys, uint32_t clientNum, uint32_t dim, uint32_t k);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
