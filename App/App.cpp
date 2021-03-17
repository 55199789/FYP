#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <time.h> 
#include <stdlib.h>
# include <unistd.h>
# include <pwd.h>
# define MAX_PATH FILENAME_MAX

#include "sgx_trts.h"
#include "sgx_urts.h"

#include "App.h"
#include "Enclave_u.h"

#include <thread>

#include "user_types.h"
#include "threads_conf.h"

// add by ice
const size_t MAXS = 1024 * 1024 *1024;

/* Global EID shared by multiple threads */
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
void ocall_print_string(const char *str)
{
    /* Proxy/Bridge will check the length and null-terminate 
     * the input string to prevent buffer overflow. 
     */
    printf("%s", str);
}


double ocall_gettime(const char *name="\0", int is_end=false)
{
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

void loop(int tid) {
    printf("\033[34m [loop][thread][%d][begin] \033[0m\n", tid);
    ecall_loop(global_eid, tid);
}
static std::atomic<bool> input_load_flag[THREAD_NUM];
static std::function<void()> load_func_ptrs[THREAD_NUM];

std::thread threads[THREAD_NUM];
void threads_init() {
    for (int i = 1; i < THREAD_NUM; i++) {
        load_func_ptrs[i] = NULL;
        input_load_flag[i] = false;
        threads[i] = std::thread(loop, i);
      //  threads[i].detach();
    }
}

void threads_finish() {
  ecall_threads_down(global_eid);

    for (int i = 1; i < THREAD_NUM; i++)
        threads[i].join();
}

void generate_data(DATATYPE *arr, int clientNum, int dim) {
    for(int i=0;i<clientNum;i++)
        for(int j=0;j<dim;j++) arr[i*dim+j] = (DATATYPE)rand()/RAND_MAX;
}

int aggregate(DATATYPE *dataMat, DATATYPE *final_x, \
                int clientNum, int dim) {
    uint32_t ret = SGX_ERROR_UNEXPECTED;
    ocall_gettime();
// Testing code
for(int i=0;i<10;i++) printf("%f ", final_x[i]);
printf("\n");
// Testing end
    ecall_clear_final_x(global_eid, &ret, final_x, dim);
    if (ret != SGX_SUCCESS) {
        print_error_message(ret);
        return -1;
    }
    printf("Clear final x successfully.\n");
// Testing code
for(int i=0;i<10;i++)printf("%f ", final_x[i]);
printf("\n");
// Testing end
    ecall_aggregate(global_eid, &ret, dataMat, final_x, clientNum, dim);
    if (ret != SGX_SUCCESS) {
        print_error_message(ret);
        return -1;
    }
    return 0;
}

/* Application entry */
int SGX_CDECL main(int argc, char *argv[])
{
    (void)(argc);
    (void)(argv);
    if(argc!=3) {
        printf("Usage: ./app CLIENTNUM DIM\n\n");
        return 0;
    }
    double t_enclave_creatation;
    int clientNum = atoi(argv[1]);
    int dim = atoi(argv[2]);
    srand(time(0));
    DATATYPE *dataMat = new DATATYPE[clientNum * dim];
    DATATYPE *final_x = new DATATYPE[dim];
    /* Initialize the enclave */
    printf("init enclave...\n");
    ocall_gettime();
    if(initialize_enclave() < 0) {
        printf("Failed to load enclave.\n");
        goto destroy_enclave;
    }
    t_enclave_creatation = ocall_gettime("Create enclave", 1);
    printf("%s[INFO] Create Enclave: %fms\n",KYEL, t_enclave_creatation*1000);

    // Randomly generate data
    generate_data(dataMat, clientNum, dim);
    printf("\n%s[Info] Sizeof data matrix: %ldMB\n", KYEL, \
                 (sizeof(DATATYPE)*clientNum*dim)>>20);

    if(aggregate(dataMat, final_x, clientNum, dim) < 0) {
        printf("Failed to aggregate.\n");
        goto destroy_enclave;
    }


    /* Destroy the enclave */
    ocall_gettime();
destroy_enclave:
    sgx_destroy_enclave(global_eid);
    ocall_gettime("Destroy enclave", 1);    
    printf("Info: SampleEnclave successfully returned.\n");
    return 0;
}

    // printf("\033[34m eigen init inside enclave \033[0m\n");

    // printf("\033[34m threads_init \033[0m\n");
    // threads_init();


    // printf("\033[34m threads_finish \033[0m\n");
    // threads_finish();
