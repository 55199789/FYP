#include <stdio.h>
#include <string.h>
#include <assert.h>

# include <unistd.h>
# include <pwd.h>
# define MAX_PATH FILENAME_MAX

#include "sgx_urts.h"
#include "App.h"
#include "Enclave_u.h"

// add by ice
#include <Eigen/Dense>
#include <thread>
#include <Eigen/Core>
#include "scData.h"

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

    ret = sgx_create_enclave(ENCLAVE_FILENAME, SGX_DEBUG_FLAG, NULL, NULL, &global_eid, &misc_attr);
    if (ret != SGX_SUCCESS) {
        print_error_message(ret);
        return -1;
    }

    printf("[SGX_ATTR]%lx[ICE]\n", misc_attr.secs_attr.xfrm);

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


void ocall_gettime( const char *name="\0", int is_end=false)
{
    static std::chrono::time_point<std::chrono::high_resolution_clock> begin_time, end_time;
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
    }

    return;
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

void ocall_toFile_plain(const char *filename, uint16_t *types, size_t types_len) {

    printf("ocall_toFile......\n");

    if(freopen(filename, "w", stdout)==NULL) {
        printf("File %s open failed", filename);
        return;
    }
    for(int i=0 ; i < (int)types_len - 1; i++)
        printf("%d,", types[i]);
    printf("%d\n", types[types_len-1]);
    fclose(stdout);

    printf("ocall_toFile end......\n");
} 

//ICE
#include <sys/stat.h>
char* file_input2(const char * FileName)
{
    FILE *pFile;
    if((pFile = fopen(FileName, "rb"))==NULL) {
        printf("\033[33m Open file %s failed. \033[0m\n", FileName);
        exit(-1);
    }

    struct stat file_sate;
    fstat(fileno(pFile), &file_sate);
    size_t file_size = file_sate.st_size;
    printf("%s: File Size: %u\n", __FUNCTION__, file_size);
    char *buf = new char[file_size + 10];
    size_t len = fread(buf, 1, file_size, pFile);
    fclose(pFile);
    printf("%u bytes are readed from file...\n", len);
    buf[len] = 0;

    return buf;
}



//ICE
void eval_plain(int argc, char *argv[])
{
    if (argc != 3) {
        printf("\033[33m input arguments for rows and cols is not specified! \033[0m\n");
        printf("\033[33m usage: ./app row_val col_val \033[0m");
        printf("\033[33m note: row_val means the counts for non-zero rows \033[0m\n");
        exit(-1);
    }

    size_t input_rows = atoi(argv[1]);
    size_t input_cols = atoi(argv[2]);

    printf("Loading Data\n");

    // char *buf = file_input2("../../GSE131907_raw_UMI_matrix_3kcell.csv");
    char *buf = file_input2("../../3000-1.csv");
printf("Here\n");
    if (buf == NULL) {
        printf("file_input fail...\n");
        exit(1);
    }
    else {           
        printf("file_input success...\n");
    }
    
    // ecall_input(global_eid, buf);
    // input(data, "./input/GSE131907_raw_UMI_matrix_3kcell.csv");
    // printf("Size of data: %fMB\n", sizeof(matrix_real)*data.size()*1.0/1024/1024);
    
    ecall_eval_plain(global_eid, buf, input_rows, input_cols);
    printf("ecall eval_plain returns...\n");
}


//ICE
void eval_cipher(int argc, char *argv[])
{
    printf("Loading Data\n");

    char *buf = file_input2("./input.enc");
    if (buf == NULL) {
        printf("file_input fail...\n");
        exit(1);
    }
    else {           
        printf("file_input success...\n");
    }
    
    ecall_eval_cipher(global_eid, buf);
    
    // ecall_eval_plain(global_eid, buf, input_rows, input_cols);
    printf("ecall_eval_cipher returns...\n");
}

/* Application entry */
int SGX_CDECL main(int argc, char *argv[])
{
    (void)(argc);
    (void)(argv);

    /* Initialize the enclave */
    printf("init enclave...\n");
    ocall_gettime(0);
    if(initialize_enclave() < 0){
        printf("Enter a character before exit ...\n");
        getchar();
        return -1; 
    }
    ocall_gettime(1);

    // printf("\033[34m handle_cache_size \033[0m\n");
    // //needed
    // handle_cache_size();
    printf("\033[34m eigen init inside enclave \033[0m\n");
    eigen_init();

    printf("\033[34m threads_init \033[0m\n");
    threads_init();

    // eval_plain(argc, argv);
    eval_cipher(argc, argv);

    printf("\033[34m threads_finish \033[0m\n");
    threads_finish();

    /* Destroy the enclave */
    sgx_destroy_enclave(global_eid);

    
    
    printf("Info: SampleEnclave successfully returned.\n");

    printf("Enter a character before exit ...\n");

    return 0;
}
