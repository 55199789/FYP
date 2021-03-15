#include "Enclave.h"
#include "Enclave_t.h"

#include "dissim.h"
#include "threads_conf.h"

#include "utils.h"

std::atomic<bool> sync_flag_[THREAD_NUM];
std::function<void()> fun_ptrs[THREAD_NUM];

template <typename T1, typename T2>
void adjust(T1 a, T1 b, T2 &count1, T2 &count2){
    T1 p = f(a, b, count2);
    count1 = p*count2 + (1.0-p)*count1;
}


#if 1

// 1 bit dropout version
void calcDissim(bool useStepFunction, matrix_real* dis, \
        matrix_real* tags, bool* dropout, matrix_real *dissim_cache[THREAD_NUM], int rows, int cols, \
        real wThreshold, real a, real b, bool first){

    const int thread_num = THREAD_NUM;

    if(useStepFunction){
        int eds[thread_num];
        for(int i=0;i<THREAD_NUM-1;i++)
            eds[i] = cols * std::sqrt(1.0 * (i + 1) / THREAD_NUM);
//     // For balance each thread as it's O(n^2) instead of O(n)
        eds[thread_num-1]=cols;
        for (int tid = 0; tid < thread_num; tid++) {
            const int ed = eds[tid];
            const int st = tid?eds[tid-1]:0;
            matrix_real * cur_dissim_cache = dissim_cache[tid];

            fun_ptrs[tid] = [st, ed, tid, rows, cols, dis, \
                            tags, dropout, wThreshold, cur_dissim_cache, first]() {
                
                // matrix_real *dissimI = new matrix_real[cols];
                matrix_real *dissimI = cur_dissim_cache;
                uint8_t ctr_de[16] = {0};
                uint8_t ctr_en[16] = {0};
                int count = ed - st;

                size_t ctr_row_count = (sizeof(matrix_real) * cols + 15) / 16;
                // printf("ctr_row_count: %d\n", ctr_row_count);
                // printf("this is thread %d with count %d\n", tid, count);
                // printf("%d - start: %d end: %d\n", tid, st, ed);
                // ctr128_inc_for_bytes(ctr_de, sizeof(matrix_real) * cols * st);
                // ctr128_inc_for_bytes(ctr_en, sizeof(matrix_real) * cols * st);
                ctr128_inc_for_count(ctr_de, ctr_row_count * st);
                ctr128_inc_for_count(ctr_en, ctr_row_count * st);

                matrix_real *dissim = dis + cols * st;
                // if(first) 
                //     for(int i=0;i<10;i++)printf("%f ", dissimI[i]);
                
                for(int i=st; i<ed; i++, dissim += cols) {
                    // if((i-st)%150==0)printf("%d - %d\n", tid, i-st);
                    // load dissim_data i
                    if (!first){
                        if(sgx_aes_ctr_decrypt(&key_dissim_1, (uint8_t*)dissim, \
                            cols*sizeof(matrix_real), ctr_de, 128, \
                            (uint8_t*)dissimI) != SGX_SUCCESS) {
                            printf("\033[33m %s: sgx_aes_ctr_encrypt failed \
                                    \033[0m\n", __FUNCTION__);
                        }
                    }else memset(dissimI, 0, sizeof(matrix_real)*cols);
                    // else printf("%f ", dissimI[0]);
                    #if 0
                    for(int j=0;j<i;j++) {
                        matrix_real dist = 0;
                        matrix_real *dataCol1 = tags + i;
                        bool *dropCol1 = dropout + i;
                        matrix_real *dataCol2 = tags + j;
                        bool *dropCol2 = dropout + j;
                        for(int k=0;k<rows;k++, dataCol1+=cols, dropCol1+=cols, \
                            dataCol2+=cols, dropCol2+=cols) 
                            if(*dataCol1 > wThreshold || *dataCol2 > wThreshold || \
                                !*dropCol1 && !*dropCol2)
                                dist += pow(*dataCol1 - *dataCol2, 2);
                        dissimI[j] -= dist / 2;
                    }
                    #else 
                    for (int k = 0; k < rows; k++) {               
                        size_t row_offset = k * cols;         
                        size_t offset1 = row_offset + i;
                        matrix_real *data1 = tags + offset1;
                        bool *drop1 = dropout + offset1;

                        // for (int j = 0; j < i; j++) {
                        //     size_t offset2 = row_offset + j;
                        //     matrix_real *data2 = tags + offset2;
                        //     bool *drop2 = dropout + offset2;
                        
                        //     if (*data1 > wThreshold || *data2 > wThreshold || !*drop1 && !*drop2) {
                        //         dissimI[j] -= (*data1 - *data2) * (*data1 - *data2) / 2.0;
                        //     }
                        // }
                        // matrix_real *data2 = tags + row_offset;
                        // bool *drop2 = dropout + row_offset;
                        if (*data1 > wThreshold) {
                            // for (int j = 0; j < i; ++j, ++data2) {
                            for (int j = 0; j < i; ++j) {
                                size_t offset2 = row_offset + j;
                                matrix_real *data2 = tags + offset2;
                                // bool *drop2 = dropout + offset2;

                                dissimI[j] -= (*data1 - *data2) * (*data1 - *data2) / 2.0;
                            }
                        } else if (!*drop1) {
                            // for (int j = 0; j < i; ++j, ++data2, ++drop2) {
                            for (int j = 0; j < i; ++j) {
                                size_t offset2 = row_offset + j;
                                matrix_real *data2 = tags + offset2;
                                bool *drop2 = dropout + offset2;

                                if ( *data2 > wThreshold || !*drop2)
                                    dissimI[j] -= (*data1 - *data2) * (*data1 - *data2) / 2.0;
                            }
                        } else {
                            // for (int j = 0; j < i; ++j, ++data2) {
                            for (int j = 0; j < i; ++j) {
                                size_t offset2 = row_offset + j;
                                matrix_real *data2 = tags + offset2;
                                // bool *drop2 = dropout + offset2;

                                if ( *data2 > wThreshold) 
                                    dissimI[j] -= (*data1 - *data2) * (*data1 - *data2) / 2.0;
                            }
                        }
                    }
                    #endif

                    if (sgx_aes_ctr_encrypt(&key_dissim_2, (uint8_t*)dissimI, \
                        cols*sizeof(matrix_real), ctr_en, 128, \
                        (uint8_t*)dissim) != SGX_SUCCESS) {
                         printf("\033[33m %s: sgx_aes_ctr_encrypt failed \
                                \033[0m\n", __FUNCTION__);
                    }
                }
                // delete[] dissimI;
            };
        }
    } else {
        printf("!!!!!!!!! useStepFunction == false is not implemented\n");
        while (1)
            ;
    }
    task_notify(thread_num);
    fun_ptrs[0]();
    task_wait_done(thread_num);
}

# else

void calcDissim(bool useStepFunction, matrix_real* dis, \
        matrix_real* tags, bool* dropout, matrix_real *dissim_cache[THREAD_NUM], int rows, int cols, \
        real wThreshold, real a, real b){

    const int thread_num = THREAD_NUM;

    if(useStepFunction){
        int eds[thread_num];
        for(int i=0;i<THREAD_NUM-1;i++)
            eds[i] = cols * std::sqrt(1.0 * (i + 1) / THREAD_NUM);
//     // For balance each thread as it's O(n^2) instead of O(n)
        eds[thread_num-1]=cols;
        for (int tid = 0; tid < thread_num; tid++) {
            const int ed = eds[tid];
            const int st = tid?eds[tid-1]:0;
            matrix_real * cur_dissim_cache = dissim_cache[tid];

            fun_ptrs[tid] = [st, ed, tid, thread_num, rows, cols, dis, \
                            tags, dropout, wThreshold, cur_dissim_cache]() {
                // // add for testing
                // size_t test_count = 0, test_count2 = 0;
                // matrix_real test_sum = 0.0;
                
                // matrix_real *dissimI = new matrix_real[cols];
                matrix_real *dissimI = cur_dissim_cache;
                uint8_t ctr_de[16] = {0};
                uint8_t ctr_en[16] = {0};
                int count = ed - st;
                // int actual_count = (tid + 1 == thread_num) ? (n - count * tid) : count;

                printf("this is thread %d with count %d\n", tid, count);
                printf("%d - start: %d end: %d\n", tid, st, ed);
                ctr128_inc_for_bytes(ctr_de, sizeof(matrix_real) * cols * st);
                ctr128_inc_for_bytes(ctr_en, sizeof(matrix_real) * cols * st);
                matrix_real *dissim = dis + cols * st;
                for(int i=st; i<ed; i++, dissim += cols) {
                    if((i-st)%150==0)printf("%d - %d\n", tid, i-st);
                    // load dissim_data i
                    if (sgx_aes_ctr_decrypt(&key_dissim_1, (uint8_t*)dissim, \
                        cols*sizeof(matrix_real), ctr_de, 128, \
                        (uint8_t*)dissimI) != SGX_SUCCESS) {
                        printf("\033[33m %s: sgx_aes_ctr_encrypt failed \
                                \033[0m\n", __FUNCTION__);
                    }
                    for(int j=0;j<i;j++) {
                        matrix_real dist = 0;
                        matrix_real *dataCol1 = tags + i;
                        bool *dropCol1 = dropout + i;
                        matrix_real *dataCol2 = tags + j;
                        bool *dropCol2 = dropout + j;
                        for(int k=0;k<rows;k++, dataCol1+=cols, dropCol1+=cols, \
                            dataCol2+=cols, dropCol2+=cols) 
                            if(*dataCol1 > wThreshold || *dataCol2 > wThreshold || \
                                !*dropCol1 && !*dropCol2)
                                dist += pow(*dataCol1 - *dataCol2, 2);
                        dissimI[j] -= dist / 2;
                    }
                    if (sgx_aes_ctr_encrypt(&key_dissim_2, (uint8_t*)dissimI, \
                        cols*sizeof(matrix_real), ctr_en, 128, \
                        (uint8_t*)dissim) != SGX_SUCCESS) {
                         printf("\033[33m %s: sgx_aes_ctr_encrypt failed \
                                \033[0m\n", __FUNCTION__);
                    }
                }
                // delete[] dissimI;
            };
        }
    } else {
        printf("!!!!!!!!! useStepFunction == false is not implemented\n");
        while (1)
            ;
    }
    task_notify(thread_num);
    fun_ptrs[0]();
    task_wait_done(thread_num);
}

#endif

void cp2dissim(matrix_real* src, matrix_real* dst, const int st, \
                const int numRows, const int numCols) {
    const int ed = st+numRows;
    for(int i=st;i<ed;i++, dst+=numCols, src+=i) {
        memcpy(dst, src, i*sizeof(matrix_real));
        dst[i]=0;
    }
}

