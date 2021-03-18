#include "Enclave_u.h"
#include <errno.h>

typedef struct ms_ecall_loop_t {
	int ms_tid;
} ms_ecall_loop_t;

typedef struct ms_ecall_clear_final_x_t {
	uint32_t ms_retval;
	DATATYPE* ms_final_x;
	uint32_t ms_dim;
} ms_ecall_clear_final_x_t;

typedef struct ms_ecall_aggregate_t {
	uint32_t ms_retval;
	DATATYPE* ms_dataMat;
	DATATYPE* ms_final_x;
	uint8_t* ms_keys;
	uint32_t ms_clientNum;
	uint32_t ms_dim;
} ms_ecall_aggregate_t;

typedef struct ms_ocall_print_string_t {
	const char* ms_str;
} ms_ocall_print_string_t;

typedef struct ms_ocall_gettime_t {
	double ms_retval;
	const char* ms_name;
	int ms_is_end;
} ms_ocall_gettime_t;

typedef struct ms_ocall_encrypt_t {
	uint8_t* ms_keys;
	DATATYPE* ms_dataMat;
	uint32_t ms_clientNum;
	uint32_t ms_dim;
} ms_ocall_encrypt_t;

static sgx_status_t SGX_CDECL Enclave_ocall_print_string(void* pms)
{
	ms_ocall_print_string_t* ms = SGX_CAST(ms_ocall_print_string_t*, pms);
	ocall_print_string(ms->ms_str);

	return SGX_SUCCESS;
}

static sgx_status_t SGX_CDECL Enclave_ocall_gettime(void* pms)
{
	ms_ocall_gettime_t* ms = SGX_CAST(ms_ocall_gettime_t*, pms);
	ms->ms_retval = ocall_gettime(ms->ms_name, ms->ms_is_end);

	return SGX_SUCCESS;
}

static sgx_status_t SGX_CDECL Enclave_ocall_encrypt(void* pms)
{
	ms_ocall_encrypt_t* ms = SGX_CAST(ms_ocall_encrypt_t*, pms);
	ocall_encrypt(ms->ms_keys, ms->ms_dataMat, ms->ms_clientNum, ms->ms_dim);

	return SGX_SUCCESS;
}

static const struct {
	size_t nr_ocall;
	void * table[3];
} ocall_table_Enclave = {
	3,
	{
		(void*)Enclave_ocall_print_string,
		(void*)Enclave_ocall_gettime,
		(void*)Enclave_ocall_encrypt,
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

sgx_status_t ecall_clear_final_x(sgx_enclave_id_t eid, uint32_t* retval, DATATYPE* final_x, uint32_t dim)
{
	sgx_status_t status;
	ms_ecall_clear_final_x_t ms;
	ms.ms_final_x = final_x;
	ms.ms_dim = dim;
	status = sgx_ecall(eid, 2, &ocall_table_Enclave, &ms);
	if (status == SGX_SUCCESS && retval) *retval = ms.ms_retval;
	return status;
}

sgx_status_t ecall_aggregate(sgx_enclave_id_t eid, uint32_t* retval, DATATYPE* dataMat, DATATYPE* final_x, uint8_t* keys, uint32_t clientNum, uint32_t dim)
{
	sgx_status_t status;
	ms_ecall_aggregate_t ms;
	ms.ms_dataMat = dataMat;
	ms.ms_final_x = final_x;
	ms.ms_keys = keys;
	ms.ms_clientNum = clientNum;
	ms.ms_dim = dim;
	status = sgx_ecall(eid, 3, &ocall_table_Enclave, &ms);
	if (status == SGX_SUCCESS && retval) *retval = ms.ms_retval;
	return status;
}

