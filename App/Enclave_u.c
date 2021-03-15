#include "Enclave_u.h"
#include <errno.h>

typedef struct ms_ecall_loop_t {
	int ms_tid;
} ms_ecall_loop_t;

typedef struct ms_ocall_print_string_t {
	const char* ms_str;
} ms_ocall_print_string_t;

typedef struct ms_ocall_gettime_t {
	const char* ms_name;
	int ms_is_end;
} ms_ocall_gettime_t;

static sgx_status_t SGX_CDECL Enclave_ocall_print_string(void* pms)
{
	ms_ocall_print_string_t* ms = SGX_CAST(ms_ocall_print_string_t*, pms);
	ocall_print_string(ms->ms_str);

	return SGX_SUCCESS;
}

static sgx_status_t SGX_CDECL Enclave_ocall_gettime(void* pms)
{
	ms_ocall_gettime_t* ms = SGX_CAST(ms_ocall_gettime_t*, pms);
	ocall_gettime(ms->ms_name, ms->ms_is_end);

	return SGX_SUCCESS;
}

static const struct {
	size_t nr_ocall;
	void * table[2];
} ocall_table_Enclave = {
	2,
	{
		(void*)Enclave_ocall_print_string,
		(void*)Enclave_ocall_gettime,
	}
};
sgx_status_t ecall_loop(sgx_enclave_id_t eid, int tid)
{
	sgx_status_t status;
	ms_ecall_loop_t ms;
	ms.ms_tid = tid;
	status = sgx_ecall(eid, 0, &ocall_table_Enclave, &ms);
	return status;
}

sgx_status_t ecall_threads_down(sgx_enclave_id_t eid)
{
	sgx_status_t status;
	status = sgx_ecall(eid, 1, &ocall_table_Enclave, NULL);
	return status;
}

