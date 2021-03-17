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

sgx_status_t ecall_loop(sgx_enclave_id_t eid, int tid);
sgx_status_t ecall_threads_down(sgx_enclave_id_t eid);
sgx_status_t ecall_key_exchange(sgx_enclave_id_t eid, uint32_t* retval, int clientNum);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
