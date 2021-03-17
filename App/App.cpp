#include <stdio.h>
#include <string.h>
#include <assert.h>

# include <unistd.h>
# include <pwd.h>
# define MAX_PATH FILENAME_MAX

#include "sgx_trts.h"
#include "sgx_urts.h"
#include "sgx_ecp_types.h"
#include "sgx_dh.h"
#include "sgx_tcrypto.h"

#include "App.h"
#include "Enclave_u.h"

#include <thread>

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
void print_error_message(sgx_status_t ret)
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


double ocall_gettime( const char *name="\0", int is_end=false)
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
        printf("%s Time elapsed: %fs\n\n", name, elapsed.count());
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

/* Application entry */
int SGX_CDECL main(int argc, char *argv[])
{
    (void)(argc);
    (void)(argv);
    double t_enclave_creatation, t_session_creation;
    /* Initialize the enclave */
    printf("init enclave...\n");
    ocall_gettime();
    if(initialize_enclave() < 0) {
        printf("Failed to load enclave.\n");
        goto destroy_enclave;
    }
    {
    t_enclave_creatation = ocall_gettime("Create enclave", 1);
    
    // Message 1
    ocall_gettime();
    sgx_dh_msg1_t dh_msg1;            //Diffie-Hellman Message 1
    sgx_key_128bit_t dh_aek;        // Session Key
    sgx_dh_msg2_t dh_msg2;            //Diffie-Hellman Message 2
    sgx_dh_msg3_t dh_msg3;            //Diffie-Hellman Message 3
    sgx_dh_session_t sgx_dh_session;
    // sgx_dh_msg1_t dh_msg1_client, dh_msg1_server;
    // sgx_key_128bit_t dh_aek_client, dh_aek_server;
    // sgx_dh_msg2_t dh_msg2_client, dh_msg2_server;
    // sgx_dh_msg3_t dh_msg3_client, dh_msg3_server;
    // sgx_dh_session_t sgx_dh_session_client, sgx_dh_session_server;
    sgx_status_t status = SGX_SUCCESS;
    memset(&dh_aek,0, sizeof(sgx_key_128bit_t));
    memset(&dh_msg1, 0, sizeof(sgx_dh_msg1_t));
    memset(&dh_msg2, 0, sizeof(sgx_dh_msg2_t));
    memset(&dh_msg3, 0, sizeof(sgx_dh_msg3_t));
    status = sgx_dh_init_session(SGX_DH_SESSION_RESPONDER, 
                    &sgx_dh_session);
    if (status != SGX_SUCCESS) {
        print_error_message(status);
        goto destroy_enclave;
    }
    t_session_creation = ocall_gettime("Create session", 1);
    
    // From server to client
    ocall_gettime();
    status = sgx_dh_responder_gen_msg1((sgx_dh_msg1_t*)&dh_msg1, \
                    &sgx_dh_session);
    if (status != SGX_SUCCESS) {
        print_error_message(status);
        goto destroy_enclave;
    }
    ocall_gettime("Generate message 1");
    /* Destroy the enclave */
    ocall_gettime();
    }
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
