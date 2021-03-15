#include "Enclave.h"
#include "Enclave_t.h" /* print_string */
#include <stdarg.h>
#include <stdio.h> /* vsnprintf */
#include <string.h>

#include "threads_conf.h"

//#include <sgx_intrin.h>


//#include "xmmintrin.h"
//#include "immintrin.h"


/*
 * printf:
 *   Invokes OCALL to display the enclave buffer to the terminal.
 */


extern "C"
int puts(const char *str)
{
    printf(str);
    printf("\n");
}

extern "C"
int printf(const char* fmt, ...)
{
    char buf[BUFSIZ] = { '\0' };
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, BUFSIZ, fmt, ap);
    va_end(ap);
    ocall_print_string(buf);
    return (int)strnlen(buf, BUFSIZ - 1) + 1;
}


// multi-threading
bool volatile is_exit = false;
std::atomic<bool> sync_flag_[THREAD_NUM];
std::function<void()> fun_ptrs[THREAD_NUM];


void task_notify(int threads) {
    for (int i = 1; i < threads; i++) {
        sync_flag_[i] = true;
    }
}

void ecall_loop(int tid) {
    printf("[ecall_loop][thread][%d][begin]\n", tid);

    while(true) {
        {

            while(sync_flag_[tid] == false)
                 ;

            if (is_exit == true)
                return;

            if (fun_ptrs[tid] == NULL)
                printf("[ecall_loop][%d][func is null]\n", tid);
            
            fun_ptrs[tid]();

            fun_ptrs[tid] = NULL;
            sync_flag_[tid] = false;
            // is_done[tid] = true;
        }
    }
}

void task_wait_done(int threads) {
    for (int i = 1; i < threads; i++) {
        while(sync_flag_[i] == true)
            ;
    }    
}



// void test_threads() {
//     for (int i = 0; i < 4; i++) {
//         fun_ptrs[i] = [i]() {
//             printf("this is tese thread %d\n", i);
//             return;
//         };
//     }

//     task_notify(4);
//     fun_ptrs[0]();
//     task_wait_done(4);
// }

void ecall_threads_down() {
    // test_threads();
    // printf("after test_threads...\n");

    is_exit = true;
    for (int i = 1; i < THREAD_NUM; i++)
        sync_flag_[i] = true;
}

void threads_init() {
    is_exit = false;

    for (int i = 0; i < THREAD_NUM; i++) {
        fun_ptrs[i] = NULL;
        sync_flag_[i] =false;
    }
}
