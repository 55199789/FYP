#include "Enclave.h"
#include "Enclave_t.h" /* print_string */
#include <stdarg.h>
#include <stdio.h> /* vsnprintf */
#include <string.h>
#include "sgx_ecp_types.h"
#include "sgx_dh.h"
#include "threads_conf.h"
#include "sgx_tcrypto.h"
#include "KeyExchange.h"
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
    return 0;
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

uint32_t ecall_key_exchange(int clientNum) {
    double t_enter;
    ocall_gettime(&t_enter, "Enter enclave", 1);

    for(int i=0;i<clientNum;i++) {

    }
    // Message 1
    double t_session_creation, t_msg1, t_msg2, t_msg3;
    ocall_gettime(&t_session_creation, "\0", 0);
    sgx_dh_msg1_t dh_msg1;            //Diffie-Hellman Message 1
    sgx_key_128bit_t dh_aek;        // Session Key
    sgx_dh_msg2_t dh_msg2;            //Diffie-Hellman Message 2
    sgx_dh_msg3_t dh_msg3;            //Diffie-Hellman Message 3
    sgx_dh_session_t sgx_dh_session_client, sgx_dh_session_server;
    sgx_status_t status = SGX_SUCCESS;
	sgx_dh_session_enclave_identity_t initiator_identity, \
                responder_identity;
    memset(&dh_aek,0, sizeof(sgx_key_128bit_t));
    memset(&dh_msg1, 0, sizeof(sgx_dh_msg1_t));
    memset(&dh_msg2, 0, sizeof(sgx_dh_msg2_t));
    memset(&dh_msg3, 0, sizeof(sgx_dh_msg3_t));
    status = sgx_dh_init_session(SGX_DH_SESSION_RESPONDER, 
                    &sgx_dh_session_server);
    if (status != SGX_SUCCESS) {
        printf("Initialize Sever session faied.\n");
        return status;
    }
    ocall_gettime(&t_session_creation, \
            "[ECALL] Create the server session", 1);
    
    // From server to client
    ocall_gettime(&t_msg1, "\0", 0);
    status = sgx_dh_responder_gen_msg1((sgx_dh_msg1_t*)&dh_msg1, \
                    &sgx_dh_session_server);
    if (status != SGX_SUCCESS) {
        printf("Server generates msg1 failed.\n");
        return status;
    }
    ocall_gettime(&t_msg1, \
            "[ECALL] Server generates message 1", 1);

    // From client to server
    ocall_gettime(&t_msg2, "\0", 0);
    status = sgx_dh_init_session(SGX_DH_SESSION_INITIATOR, \
                        &sgx_dh_session_client);
    if (status != SGX_SUCCESS) {
        printf("Initialize Client session faied.\n");
        return status;
    }
    // printf("[ECALL] Create the client session successfully.\n");
    status = sgx_dh_initiator_proc_msg1(&dh_msg1, &dh_msg2, \
                        &sgx_dh_session_client);
    if (status != SGX_SUCCESS) {
        printf("Client processes msg1 failed.\n");
        return status;
    }
    // printf("[ECALL] Client processes message 1 successfully.\n");
    ocall_gettime(&t_msg2, "[ECALL] Client genreates msg2", 1);

    // From server to client
    ocall_gettime(&t_msg3, "\0", 0);
    status = sgx_dh_responder_proc_msg2(&dh_msg2, &dh_msg3, \
                    &sgx_dh_session_server, &dh_aek,
                    &initiator_identity);
    if (status != SGX_SUCCESS) {
        printf("Server generates msg3 failed.\n");
        return status;
    }
    ocall_gettime(&t_msg3, "[ECALL] Server generates message 3", 1);

printf("Shared Key: \n");
for(int i=0;i<16;i++) {
    printf("%02X", dh_aek[i]);
}
printf("\n\n");

    // Client generates aek - not necessarily
    // status = sgx_dh_initiator_proc_msg3(&dh_msg3, 
    //                 &sgx_dh_session_client, 
    //                 &dh_aek, 
    //                 &responder_identity);
    // if (status != SGX_SUCCESS) {
    //     printf("Client processes msg3 failed.\n");
    //     return status;
    // }
    // printf("[ECALL] Client processes message 3 successfully.\n");
    printf("%sTotal DHKE time in server: %fs\n\n\n", KRED, \
        t_session_creation + t_msg1 + t_msg3 + t_enter);
    return SGX_SUCCESS;
}