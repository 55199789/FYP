#include <assert.h>
#include <math.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h> 
#include <unistd.h>
#define MAX_PATH FILENAME_MAX
#include <algorithm>
#include <chrono>
#include "sgx_trts.h"
#include "sgx_urts.h"

#include "App.h"
#include "Enclave_u.h"

#include "openssl/aes.h"
#include "openssl/evp.h"
#include "openssl/err.h"

#define SGXSSL_CTR_BITS	128
#define SHIFT_BYTE	8

#include "user_types.h"

/* Global EID */
sgx_enclave_id_t global_eid = 0;

typedef struct _sgx_errlist_t {
    sgx_status_t err;
    const char *msg;
    const char *sug; /* Suggestion */
} sgx_errlist_t;

/* Error code returned by sgx_create_enclave */
static sgx_errlist_t sgx_errlist[] = {
    {
        SGX_ERROR_UNEXPECTED,
        "Unexpected error occurred.",
        NULL
    },
    {
        SGX_ERROR_INVALID_PARAMETER,
        "Invalid parameter.",
        NULL
    },
    {
        SGX_ERROR_OUT_OF_MEMORY,
        "Out of memory.",
        NULL
    },
    {
        SGX_ERROR_ENCLAVE_LOST,
        "Power transition occurred.",
        "Please refer to the sample \"PowerTransition\" for details."
    },
    {
        SGX_ERROR_INVALID_ENCLAVE,
        "Invalid enclave image.",
        NULL
    },
    {
        SGX_ERROR_INVALID_ENCLAVE_ID,
        "Invalid enclave identification.",
        NULL
    },
    {
        SGX_ERROR_INVALID_SIGNATURE,
        "Invalid enclave signature.",
        NULL
    },
    {
        SGX_ERROR_OUT_OF_EPC,
        "Out of EPC memory.",
        NULL
    },
    {
        SGX_ERROR_NO_DEVICE,
        "Invalid SGX device.",
        "Please make sure SGX module is enabled in the BIOS, and install SGX driver afterwards."
    },
    {
        SGX_ERROR_MEMORY_MAP_CONFLICT,
        "Memory map conflicted.",
        NULL
    },
    {
        SGX_ERROR_INVALID_METADATA,
        "Invalid enclave metadata.",
        NULL
    },
    {
        SGX_ERROR_DEVICE_BUSY,
        "SGX device was busy.",
        NULL
    },
    {
        SGX_ERROR_INVALID_VERSION,
        "Enclave version was invalid.",
        NULL
    },
    {
        SGX_ERROR_INVALID_ATTRIBUTE,
        "Enclave was not authorized.",
        NULL
    },
    {
        SGX_ERROR_ENCLAVE_FILE_ACCESS,
        "Can't open enclave file.",
        NULL
    },
};

/* Check error conditions for loading enclave */
template<class T>
void print_error_message(T ret)
{
    size_t idx = 0;
    size_t ttl = sizeof sgx_errlist/sizeof sgx_errlist[0];

    for (idx = 0; idx < ttl; idx++) {
        if(ret == sgx_errlist[idx].err) {
            if(NULL != sgx_errlist[idx].sug)
                printf("Info: %s\n", sgx_errlist[idx].sug);
            printf("Error: %s\n", sgx_errlist[idx].msg);
            break;
        }
    }
    
    if (idx == ttl)
    	printf("Error code is 0x%X. Please refer to the \"Intel SGX SDK Developer Reference\" for more details.\n", ret);
}

/* Initialize the enclave:
 *   Call sgx_create_enclave to initialize an enclave instance
 */
int initialize_enclave(void)
{
    sgx_status_t ret = SGX_ERROR_UNEXPECTED;
    
    /* Call sgx_create_enclave to initialize an enclave instance */
    /* Debug Support: set 2nd parameter to 1 */
    sgx_misc_attribute_t misc_attr;

    ret = sgx_create_enclave(ENCLAVE_FILENAME, SGX_DEBUG_FLAG, \
                NULL, NULL, &global_eid, &misc_attr);
    if (ret != SGX_SUCCESS) {
        print_error_message(ret);
        return -1;
    }

    // printf("[SGX_ATTR]%lx[ICE]\n", misc_attr.secs_attr.xfrm);

    return 0;
}

/* OCall functions */
void ocall_print_string(const char *str) {
    /* Proxy/Bridge will check the length and null-terminate 
     * the input string to prevent buffer overflow. 
     */
    printf("%s", str);
}

double ocall_gettime(const char *name="\0", int is_end=false) {
    static std::chrono::time_point<std::chrono::high_resolution_clock> \
                            begin_time, end_time;
    static bool begin_done = false;

    if (!is_end) {
        begin_done = true;
        begin_time = std::chrono::high_resolution_clock::now();
    } else if (!begin_done) {
        printf("NOTE: begin time is not set, so elapsed time is not printed!\n");
    } else {
        end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end_time - begin_time;
        if(TIMEPRINT) {
            printf("%s%s Time elapsed: %fs\n\n", KYEL, name, elapsed.count());
            printf("%s", KNRM);
        }
        return elapsed.count();
    }

    return 0;
}

void generate_data(DATATYPE *arr, int clientNum, int dim) {
    for(int i=0;i<clientNum;i++)
        // for(int j=0;j<dim;j++) arr[i*dim+j] = (DATATYPE)rand()/RAND_MAX;
        for(int j=0;j<dim;j++) arr[i*dim+j] = (DATATYPE)rand();
}

void aggregate_test(DATATYPE *dataMat, uint32_t clientNum, \
                uint32_t dim, DATATYPE *test_x) {
    memset(test_x, 0, sizeof(DATATYPE)*dim);
    for(int i=0;i<clientNum;i++) {
        DATATYPE *vec = dataMat + i * dim;
        for(int j=0;j<dim;j++) test_x[j] += vec[j];
    }
    for(int i=0;i<dim;i++)test_x[i]/=clientNum;
}


static void ctr128_inc(uint8_t *counter) {
	unsigned int n = 16, c = 1;
	do {
		--n;
		c += counter[n];
		counter[n] = (uint8_t)c;
		c >>= SHIFT_BYTE;
	} while (n);
}

sgx_status_t aes_ctr_encrypt(const uint8_t *p_key, \
                        const uint8_t *p_src, \
                        const uint32_t src_len, uint8_t *p_ctr, \
                        const uint32_t ctr_inc_bits, uint8_t *p_dst) {
    if ((src_len > INT_MAX) || (p_key == NULL) || \
            (p_src == NULL) || (p_ctr == NULL) || (p_dst == NULL))
		return SGX_ERROR_INVALID_PARAMETER;
    sgx_status_t ret = SGX_ERROR_UNEXPECTED;
	int len = 0;
	EVP_CIPHER_CTX* ptr_ctx = NULL;
    if (ctr_inc_bits != SGXSSL_CTR_BITS)
		return SGX_ERROR_INVALID_PARAMETER;
    do {
		if (!(ptr_ctx = EVP_CIPHER_CTX_new())) {
			ret = SGX_ERROR_OUT_OF_MEMORY;
			break;
		}
		if (1 != EVP_EncryptInit_ex(ptr_ctx, EVP_aes_128_ctr(), NULL, (unsigned char*)p_key, p_ctr)) {
			break;
		}
		if (1 != EVP_EncryptUpdate(ptr_ctx, p_dst, &len, p_src, src_len)) {
			break;
		}
		if (1 != EVP_EncryptFinal_ex(ptr_ctx, p_dst + len, &len)) {
			break;
		}
		len = src_len;
		while (len >= 0) {
			ctr128_inc(p_ctr);
			len -= 16;
		}
		ret = SGX_SUCCESS;
	} while (0);

	if (ptr_ctx) {
		EVP_CIPHER_CTX_free(ptr_ctx);
	}
	return ret;
}

void ocall_compress(DATATYPE *dataMat, \
        const uint32_t clientNum, const uint32_t dim, \
        const uint32_t k) {
    // Compress dataMat
    if (k<=0 || k>=dim) 
        return;
    DATATYPE *tmp = new DATATYPE[dim];
    Element *ele = new Element[k];
    double t_comp[clientNum];
    for(int i=0;i<clientNum;i++) { 
        ocall_gettime();
        int st = i*dim;
        memcpy(tmp, dataMat+st, sizeof(DATATYPE)*dim);
        DATATYPE *kth = tmp + k - 1;
        std::nth_element(tmp, kth, tmp+dim, \
                [](const DATATYPE &a, const DATATYPE &b)->bool {
                    return abs(a)>abs(b);
                });
        DATATYPE beta = 0;
        int cnt = 0;
        for(int j=0;j<dim;j++) {
            if(dataMat[st+j]>=*kth) {
                ele[cnt++] = {
                    j, dataMat[st+j]>0
                };
            }
            beta += dataMat[st+j]*dataMat[st+j];
        }
        beta = sqrt(beta)/sqrt(k);
        // Re-use dataMat for convenience
        memcpy(dataMat+st, ele, sizeof(Element)*k);
        memcpy(dataMat+st+sizeof(Element)*k, &beta, \
                sizeof(beta));
        t_comp[i] = ocall_gettime("\0", 1);
    }

    double t_en_avg = 0;
    for(auto i:t_comp) t_en_avg += i;
    t_en_avg /= clientNum;
    printf("%sTime of data compression per client: %fms%s\n", \
        KRED, t_en_avg*1000, KNRM);
    delete[] tmp;
    delete[] ele;
}

void ocall_encrypt(uint8_t *keys, DATATYPE *dataMat, \
        const uint32_t clientNum, const uint32_t dim, \
        const uint32_t k) {
    // Encrypt dataMat
    uint8_t *tmp = NULL;
    if (k<dim && k>0)
        tmp = new uint8_t[k*sizeof(Element)];
    else
        tmp = new uint8_t[dim*sizeof(DATATYPE)];
    double t_en[clientNum];
    for(int i=0;i<clientNum;i++) {
        ocall_gettime();
        uint8_t ctr[16] = {0};
        if (k<dim && k>0) 
            memcpy(tmp, dataMat+i*dim, sizeof(Element)*k);
        else 
            memcpy(tmp, dataMat+i*dim, sizeof(DATATYPE)*dim);
        // As we re-use the same memory, 
        // we do not need to encrypt the whole space (if compressed)
        if(aes_ctr_encrypt(keys+16*i, \
                (uint8_t*)tmp, \
                k<dim&&k>0?sizeof(Element)*k: \
                            sizeof(DATATYPE)*dim, \
                ctr, 128, \
                (uint8_t *)(dataMat + i*dim)) != SGX_SUCCESS)
            printf("\033[33m Encrypt dataMat: sgx_aes_ctr_encrypt failed \033[0m\n");
        t_en[i] = ocall_gettime("\0", 1);
    }
    double t_en_avg = 0;
    for(auto i:t_en) t_en_avg += i;
    t_en_avg /= clientNum;
    printf("%sTime of data encryption per client: %fms%s\n", \
        KRED, t_en_avg*1000, KNRM);
    delete[] tmp;
}

int aggregate(DATATYPE *dataMat, DATATYPE *final_x, \
                uint8_t *keys, \
                int clientNum, int dim, \
                int k) {
    uint32_t ret = SGX_ERROR_UNEXPECTED;
    ocall_gettime();
    ecall_clear_final_x(global_eid, &ret, final_x, dim);
    if (ret != SGX_SUCCESS) {
        print_error_message(ret);
        return -1;
    }
    printf("Clear final x successfully.\n");
    ecall_aggregate(global_eid, &ret, dataMat, final_x,\
            keys, clientNum, dim, k);
    if (ret != SGX_SUCCESS) {
        print_error_message(ret);
        return -1;
    }
    return 0;
}

void test_aggregate_results(DATATYPE *test_x, \
                DATATYPE *final_x, uint32_t dim) {
    for(int i=0;i<dim;i++)
        if(abs(final_x[i] - test_x[i])>1e-6) {
            printf("pos: %d, std x: %f, final x: %f failed!\n", i, \
                test_x[i], final_x[i]);
            return;
        }
    printf("Testing passed!\n");
}

/* Application entry */
int SGX_CDECL main(int argc, char *argv[])
{
    (void)(argc);
    (void)(argv);
    if(argc!=4 && argc!=3) {
        printf("Usage: ./app CLIENTNUM DIM [k]\n\n");
        return 0;
    }
    double t_enclave_creatation;
    int clientNum = atoi(argv[1]);
    int dim = atoi(argv[2]);
    int k = dim;
    if(argc==4) {
        k = atoi(argv[3]);
        if(k<=1) {
            if(atof(argv[3]>0.5)) {
                printf("k should be less than 0.5 or greater than 1.\n");
                return;
            }
            k = atof(argv[3])*dim;
        }
    }
    srand(time(0));
    DATATYPE *dataMat = new DATATYPE[clientNum * dim];
    DATATYPE *final_x = new DATATYPE[dim];
    DATATYPE *test_x = new DATATYPE[dim];
    uint8_t *clientKeys = new uint8_t[clientNum*16];
    /* Initialize the enclave */
    printf("init enclave...\n");
    ocall_gettime();
    if(initialize_enclave() < 0) {
        printf("Failed to load enclave.\n");
        goto destroy_enclave;
    }
    t_enclave_creatation = ocall_gettime("Create enclave", 1);
    printf("%s[INFO] Create Enclave: %fms%s\n",KYEL, \
            t_enclave_creatation*1000, KNRM);

    // printf("\033[34m threads_init \033[0m\n");
    // threads_init();

    // Randomly generate data
    generate_data(dataMat, clientNum, dim);

    ocall_gettime();
    aggregate_test(dataMat, clientNum, dim, test_x);
    printf("Test aggregate time: %fms\n", \
            ocall_gettime("Aggregate", 1));

    printf("\n%s[Info] Sizeof data matrix: %ldMB\n%s", KYEL, \
                 (sizeof(DATATYPE)*clientNum*dim)>>20, KNRM);

    if(aggregate(dataMat, final_x, clientKeys,\
             clientNum, dim, k) < 0) {
        printf("Failed to aggregate.\n");
        goto destroy_enclave;
    }
    printf("Secure aggregation completed!\n");

    // Test whether the results are equal
    test_aggregate_results(test_x, final_x, dim);

    /* Destroy the enclave */
    ocall_gettime();
destroy_enclave:
    double t_destroy = sgx_destroy_enclave(global_eid);
    ocall_gettime("Destroy enclave", 1);    
    delete[] dataMat;
    delete[] clientKeys; 
    delete[] final_x;
    delete[] test_x;
    printf("[Info] Destroy enclave: %fms.\n", t_destroy);
    
    // printf("\033[34m threads_finish \033[0m\n");
    // threads_finish();

    return 0;
}
