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

void ecall_loop(int tid);
void ecall_threads_down(void);
uint32_t ecall_clear_final_x(DATATYPE* final_x, uint32_t dim);
uint32_t ecall_aggregate(DATATYPE* dataMat, DATATYPE* final_x, uint32_t clientNum, uint32_t dim);

sgx_status_t SGX_CDECL ocall_print_string(const char* str);
sgx_status_t SGX_CDECL ocall_gettime(double* retval, const char* name, int is_end);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
