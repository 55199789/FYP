#include "Enclave_t.h"

#include "sgx_trts.h" /* for sgx_ocalloc, sgx_is_outside_enclave */
#include "sgx_lfence.h" /* for sgx_lfence */

#include <errno.h>
#include <mbusafecrt.h> /* for memcpy_s etc */
#include <stdlib.h> /* for malloc/free etc */

#define CHECK_REF_POINTER(ptr, siz) do {	\
	if (!(ptr) || ! sgx_is_outside_enclave((ptr), (siz)))	\
		return SGX_ERROR_INVALID_PARAMETER;\
} while (0)

#define CHECK_UNIQUE_POINTER(ptr, siz) do {	\
	if ((ptr) && ! sgx_is_outside_enclave((ptr), (siz)))	\
		return SGX_ERROR_INVALID_PARAMETER;\
} while (0)

#define CHECK_ENCLAVE_POINTER(ptr, siz) do {	\
	if ((ptr) && ! sgx_is_within_enclave((ptr), (siz)))	\
		return SGX_ERROR_INVALID_PARAMETER;\
} while (0)

#define ADD_ASSIGN_OVERFLOW(a, b) (	\
	((a) += (b)) < (b)	\
)


typedef struct ms_ecall_clear_final_x_t {
	uint32_t ms_retval;
	DATATYPE* ms_final_x;
	uint32_t ms_dim;
} ms_ecall_clear_final_x_t;

typedef struct ms_ecall_aggregate_t {
	uint32_t ms_retval;
	DATATYPE* ms_dataMat;
	Vector* ms_compressedData;
	Vector* ms_compressedX;
	DATATYPE* ms_final_x;
	uint8_t* ms_keys;
	uint32_t ms_clientNum;
	uint32_t ms_dim;
	uint32_t ms_k;
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
	Vector* ms_compressedData;
	uint32_t ms_clientNum;
	uint32_t ms_dim;
	uint32_t ms_k;
} ms_ocall_encrypt_t;

typedef struct ms_ocall_compress_t {
	DATATYPE* ms_dataMat;
	Vector* ms_compressedData;
	uint32_t ms_clientNum;
	uint32_t ms_dim;
	uint32_t ms_k;
} ms_ocall_compress_t;

static sgx_status_t SGX_CDECL sgx_ecall_clear_final_x(void* pms)
{
	CHECK_REF_POINTER(pms, sizeof(ms_ecall_clear_final_x_t));
	//
	// fence after pointer checks
	//
	sgx_lfence();
	ms_ecall_clear_final_x_t* ms = SGX_CAST(ms_ecall_clear_final_x_t*, pms);
	sgx_status_t status = SGX_SUCCESS;
	DATATYPE* _tmp_final_x = ms->ms_final_x;



	ms->ms_retval = ecall_clear_final_x(_tmp_final_x, ms->ms_dim);


	return status;
}

static sgx_status_t SGX_CDECL sgx_ecall_aggregate(void* pms)
{
	CHECK_REF_POINTER(pms, sizeof(ms_ecall_aggregate_t));
	//
	// fence after pointer checks
	//
	sgx_lfence();
	ms_ecall_aggregate_t* ms = SGX_CAST(ms_ecall_aggregate_t*, pms);
	sgx_status_t status = SGX_SUCCESS;
	DATATYPE* _tmp_dataMat = ms->ms_dataMat;
	Vector* _tmp_compressedData = ms->ms_compressedData;
	Vector* _tmp_compressedX = ms->ms_compressedX;
	DATATYPE* _tmp_final_x = ms->ms_final_x;
	uint8_t* _tmp_keys = ms->ms_keys;



	ms->ms_retval = ecall_aggregate(_tmp_dataMat, _tmp_compressedData, _tmp_compressedX, _tmp_final_x, _tmp_keys, ms->ms_clientNum, ms->ms_dim, ms->ms_k);


	return status;
}

SGX_EXTERNC const struct {
	size_t nr_ecall;
	struct {void* ecall_addr; uint8_t is_priv; uint8_t is_switchless;} ecall_table[2];
} g_ecall_table = {
	2,
	{
		{(void*)(uintptr_t)sgx_ecall_clear_final_x, 0, 0},
		{(void*)(uintptr_t)sgx_ecall_aggregate, 0, 0},
	}
};

SGX_EXTERNC const struct {
	size_t nr_ocall;
	uint8_t entry_table[4][2];
} g_dyn_entry_table = {
	4,
	{
		{0, 0, },
		{0, 0, },
		{0, 0, },
		{0, 0, },
	}
};


sgx_status_t SGX_CDECL ocall_print_string(const char* str)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_str = str ? strlen(str) + 1 : 0;

	ms_ocall_print_string_t* ms = NULL;
	size_t ocalloc_size = sizeof(ms_ocall_print_string_t);
	void *__tmp = NULL;


	CHECK_ENCLAVE_POINTER(str, _len_str);

	if (ADD_ASSIGN_OVERFLOW(ocalloc_size, (str != NULL) ? _len_str : 0))
		return SGX_ERROR_INVALID_PARAMETER;

	__tmp = sgx_ocalloc(ocalloc_size);
	if (__tmp == NULL) {
		sgx_ocfree();
		return SGX_ERROR_UNEXPECTED;
	}
	ms = (ms_ocall_print_string_t*)__tmp;
	__tmp = (void *)((size_t)__tmp + sizeof(ms_ocall_print_string_t));
	ocalloc_size -= sizeof(ms_ocall_print_string_t);

	if (str != NULL) {
		ms->ms_str = (const char*)__tmp;
		if (_len_str % sizeof(*str) != 0) {
			sgx_ocfree();
			return SGX_ERROR_INVALID_PARAMETER;
		}
		if (memcpy_s(__tmp, ocalloc_size, str, _len_str)) {
			sgx_ocfree();
			return SGX_ERROR_UNEXPECTED;
		}
		__tmp = (void *)((size_t)__tmp + _len_str);
		ocalloc_size -= _len_str;
	} else {
		ms->ms_str = NULL;
	}
	
	status = sgx_ocall(0, ms);

	if (status == SGX_SUCCESS) {
	}
	sgx_ocfree();
	return status;
}

sgx_status_t SGX_CDECL ocall_gettime(double* retval, const char* name, int is_end)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_name = name ? strlen(name) + 1 : 0;

	ms_ocall_gettime_t* ms = NULL;
	size_t ocalloc_size = sizeof(ms_ocall_gettime_t);
	void *__tmp = NULL;


	CHECK_ENCLAVE_POINTER(name, _len_name);

	if (ADD_ASSIGN_OVERFLOW(ocalloc_size, (name != NULL) ? _len_name : 0))
		return SGX_ERROR_INVALID_PARAMETER;

	__tmp = sgx_ocalloc(ocalloc_size);
	if (__tmp == NULL) {
		sgx_ocfree();
		return SGX_ERROR_UNEXPECTED;
	}
	ms = (ms_ocall_gettime_t*)__tmp;
	__tmp = (void *)((size_t)__tmp + sizeof(ms_ocall_gettime_t));
	ocalloc_size -= sizeof(ms_ocall_gettime_t);

	if (name != NULL) {
		ms->ms_name = (const char*)__tmp;
		if (_len_name % sizeof(*name) != 0) {
			sgx_ocfree();
			return SGX_ERROR_INVALID_PARAMETER;
		}
		if (memcpy_s(__tmp, ocalloc_size, name, _len_name)) {
			sgx_ocfree();
			return SGX_ERROR_UNEXPECTED;
		}
		__tmp = (void *)((size_t)__tmp + _len_name);
		ocalloc_size -= _len_name;
	} else {
		ms->ms_name = NULL;
	}
	
	ms->ms_is_end = is_end;
	status = sgx_ocall(1, ms);

	if (status == SGX_SUCCESS) {
		if (retval) *retval = ms->ms_retval;
	}
	sgx_ocfree();
	return status;
}

sgx_status_t SGX_CDECL ocall_encrypt(uint8_t* keys, DATATYPE* dataMat, Vector* compressedData, uint32_t clientNum, uint32_t dim, uint32_t k)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_encrypt_t* ms = NULL;
	size_t ocalloc_size = sizeof(ms_ocall_encrypt_t);
	void *__tmp = NULL;


	__tmp = sgx_ocalloc(ocalloc_size);
	if (__tmp == NULL) {
		sgx_ocfree();
		return SGX_ERROR_UNEXPECTED;
	}
	ms = (ms_ocall_encrypt_t*)__tmp;
	__tmp = (void *)((size_t)__tmp + sizeof(ms_ocall_encrypt_t));
	ocalloc_size -= sizeof(ms_ocall_encrypt_t);

	ms->ms_keys = keys;
	ms->ms_dataMat = dataMat;
	ms->ms_compressedData = compressedData;
	ms->ms_clientNum = clientNum;
	ms->ms_dim = dim;
	ms->ms_k = k;
	status = sgx_ocall(2, ms);

	if (status == SGX_SUCCESS) {
	}
	sgx_ocfree();
	return status;
}

sgx_status_t SGX_CDECL ocall_compress(DATATYPE* dataMat, Vector* compressedData, uint32_t clientNum, uint32_t dim, uint32_t k)
{
	sgx_status_t status = SGX_SUCCESS;

	ms_ocall_compress_t* ms = NULL;
	size_t ocalloc_size = sizeof(ms_ocall_compress_t);
	void *__tmp = NULL;


	__tmp = sgx_ocalloc(ocalloc_size);
	if (__tmp == NULL) {
		sgx_ocfree();
		return SGX_ERROR_UNEXPECTED;
	}
	ms = (ms_ocall_compress_t*)__tmp;
	__tmp = (void *)((size_t)__tmp + sizeof(ms_ocall_compress_t));
	ocalloc_size -= sizeof(ms_ocall_compress_t);

	ms->ms_dataMat = dataMat;
	ms->ms_compressedData = compressedData;
	ms->ms_clientNum = clientNum;
	ms->ms_dim = dim;
	ms->ms_k = k;
	status = sgx_ocall(3, ms);

	if (status == SGX_SUCCESS) {
	}
	sgx_ocfree();
	return status;
}
