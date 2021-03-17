#include "Enclave.h"
#include "Enclave_t.h"

#include <string.h>
#include "sgx_dh.h"
#include "KeyExchange.h"

uint32_t generate_key(double &totTime,sgx_key_128bit_t &dh_aek) {
    double t_session_creation, t_msg1, t_msg2, t_msg3;
    ocall_gettime(&t_session_creation, "\0", 0);
    sgx_dh_msg1_t dh_msg1;            //Diffie-Hellman Message 1
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
if(TIMEPRINT) {
    printf("Shared Key: \n");
    for(int i=0;i<16;i++) {
    printf("%02X", dh_aek[i]);
    }
    printf("\n\n");
}
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
    totTime += t_session_creation + t_msg1 + t_msg3;
    return SGX_SUCCESS;
}