#include "Enclave.h"
#include "Enclave_t.h" /* print_string */
#include <stdarg.h>
#include <stdio.h> /* vsnprintf */
#include <string.h>

#include <Eigen/Dense>
#include <Eigen/Core>
#include "scData.h"
#include "threads_conf.h"
#include "utils.h"


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
extern std::atomic<bool> sync_flag_[THREAD_NUM];
extern std::function<void()> fun_ptrs[THREAD_NUM];


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

//used to set cache size in Eigen
int cacheL1 = 0, cacheL2 = 0, cacheL3 = 0;
void ecall_cache_size(int l1, int l2, int l3) {
    //printf("[ecall_cache_size][%d][%d][%d]\n", l1, l2, l3);
    cacheL1 = l1;
    cacheL2 = l2;
    cacheL3 = l3;
}
void cache_size_hook(int &l1, int &l2, int &l3) {
//    printf("cache_size_hook\n");
    l1 = cacheL1;
    l2 = cacheL2;
    l3 = cacheL3;
}

void ecall_eigen_init(int l1, int l2, int l3, int threads)
{
    ecall_cache_size(l1, l2, l3);
    Eigen::setNbThreads(threads);    
}



void test_matrix_majorchange_outside(scData *obj)
{
 
// tags
printf("\033[34m tags majorchange testing begin... \033[0m\n");
matrix_real *test_outside;
ocall_aligned_malloc((uint64_t *)&test_outside, sizeof(matrix_real) * obj->featureSize * obj->sampleSize);
printf("test_outside: %p\n", test_outside);

size_t new_tag_rows = matrix_majorchange_outside_wrapper<matrix_real>((matrix_real *)(obj->tags_outside), (matrix_real *)test_outside, obj->tags_outside_rows, 
                                obj->tags_outside_cols, &key_tags, &key_tags_rowmajor, NULL, true, obj->enclave_cache, obj->enclave_cache_size);
printf("new_tag_rows: %d\n", new_tag_rows);

uint8_t ctr_tag[16] = {0};
matrix_real *tags_cache = new matrix_real[obj->sampleSize];
for (int i = 0; i < obj->featureSize; i++) {
    // printf("\033[34m row  %d begin... \033[0m\n", i);
    matrix_real *out_test_row = &test_outside[i * obj->sampleSize];

    if (sgx_aes_ctr_decrypt(&key_tags_rowmajor, (uint8_t *)out_test_row, obj->sampleSize * sizeof(matrix_real), ctr_tag, 128, (uint8_t *)(tags_cache)) != SGX_SUCCESS) {
        printf("\033[33m %s: sgx_aes_ctr_encrypt failed - %d -\033[0m\n", __FUNCTION__, i);
        while (1)
            ;        
    }
    for (int j = 0; j < obj->sampleSize; j++) {
        if (tags_cache[j] != (*(obj->tags))(i, j)) {
            printf("\033[33m not equal in (%d, %d) \033[0m\n", i, j);
            while (1)
                ;            
        }
    }

    // printf("\033[34m row  %d done... \033[0m\n", i);
}


//dropout
printf("\033[35m dropout majorchange testing begin... \033[0m\n");
bool *test_dropout_outside;
ocall_aligned_malloc((uint64_t *)&test_dropout_outside, sizeof(bool) * obj->featureSize * obj->sampleSize);
printf("test_dropout_outside: %p\n", test_dropout_outside);

size_t new_dropout_rows = matrix_majorchange_outside_wrapper<bool>((bool *)(obj->dropoutCandidates_outside), (bool *)test_dropout_outside, obj->tags_outside_rows, \
                                obj->tags_outside_cols, &key_dropout, &key_dropout_rowmajor, NULL, true, obj->enclave_cache, obj->enclave_cache_size);
printf("new_dropout_rows: %d\n", new_dropout_rows);

uint8_t ctr_dropout[16] = {0};
bool *dropout_cache = new bool[obj->sampleSize];
for (int i = 0; i < obj->featureSize; i++) {
    printf("\033[35m row  %d begin... \033[0m\n", i);
    bool *out_test_row = &test_dropout_outside[i * obj->sampleSize];

    if (sgx_aes_ctr_decrypt(&key_dropout_rowmajor, (uint8_t *)out_test_row, obj->sampleSize * sizeof(bool), ctr_dropout, 128, (uint8_t *)(dropout_cache)) != SGX_SUCCESS) {
        printf("\033[35m %s: sgx_aes_ctr_encrypt failed - %d -\033[0m\n", __FUNCTION__, i);
        while (1)
            ;        
    } 

    for (int j = 0; j < obj->sampleSize; j++) {
        if (dropout_cache[j] != obj->dropoutCandidates(i, j)) {
            printf("\033[35m not equal in (%d, %d) \033[0m\n", i, j);
            while (1)
                ;            
        }
    }

    printf("\033[35m row  %d done... \033[0m\n", i);
}

}


void test_matrix_majorchange_outside_with_del(scData *obj)
{
        std::vector<int> remain_row;

        int drop_rows = 0, data_rows = 0, inter_rows = 0;
        for (int i = 0; i < obj->featureSize; i++) {
            size_t dropout_count = 0;
            size_t data_count = 0;
            for (int j = 0; j < obj->sampleSize; j++) {
                if (obj->dropoutCandidates(i, j))
                    dropout_count ++;
                if ((*(obj->tags))(i, j) < obj->wThresholdVal)
                    data_count ++;
            }

            if (dropout_count == obj->sampleSize)
                drop_rows ++;
            if (data_count == obj->sampleSize)
                data_rows ++;
            if (dropout_count == obj->sampleSize && data_count == obj->sampleSize)
                inter_rows ++;
            else
                remain_row.push_back(i);
        }
        printf("\033[36m drop_rows: %d  data_rows: %d  inter_rows: %d  remain_rows: %d \033[0m\n", drop_rows, data_rows, inter_rows, remain_row.size());


    //dropout
    printf("\033[35m dropout majorchange testing begin... \033[0m\n");
    ocall_gettime(0);
    bool *test_dropout_outside;
    ocall_aligned_malloc((uint64_t *)&test_dropout_outside, sizeof(bool) * obj->featureSize * obj->sampleSize);
    printf("test_dropout_outside: %p\n", test_dropout_outside);

    size_t new_dropout_rows = matrix_majorchange_outside_wrapper<bool>((bool *)(obj->dropoutCandidates_outside), (bool *)test_dropout_outside, obj->tags_outside_rows, \
                                    obj->tags_outside_cols, &key_dropout, &key_dropout_rowmajor, NULL, true, obj->enclave_cache, obj->enclave_cache_size);
    printf("new_dropout_rows: %d\n", new_dropout_rows);

    uint8_t ctr_dropout[16] = {0};
    bool *dropout_cache = new bool[obj->sampleSize];
    for (int i = 0; i < obj->featureSize; i++) {
        printf("\033[35m row  %d begin... \033[0m\n", i);
        bool *out_test_row = &test_dropout_outside[i * obj->sampleSize];

        if (sgx_aes_ctr_decrypt(&key_dropout_rowmajor, (uint8_t *)out_test_row, obj->sampleSize * sizeof(bool), ctr_dropout, 128, (uint8_t *)(dropout_cache)) != SGX_SUCCESS) {
            printf("\033[35m %s: sgx_aes_ctr_encrypt failed - %d -\033[0m\n", __FUNCTION__, i);
            while (1)
                ;        
        } 

        for (int j = 0; j < obj->sampleSize; j++) {
            if (dropout_cache[j] != obj->dropoutCandidates(i, j)) {
                printf("\033[35m not equal in (%d, %d) \033[0m\n", i, j);
                while (1)
                    ;            
            }
        }

        printf("\033[35m row  %d done... \033[0m\n", i);
    }
    ocall_gettime(1);

    printf("\033[34m tag majorchange testing with del begin... \033[0m\n");
    ocall_gettime(0);
    matrix_real *test_outside;
    ocall_aligned_malloc((uint64_t *)&test_outside, sizeof(matrix_real) * obj->featureSize * obj->sampleSize);
    printf("test_outside: %p\n", test_outside);
    size_t new_tag_rows = matrix_majorchange_outside_with_del((matrix_real *)(obj->tags_outside), (matrix_real *)test_outside, test_dropout_outside, test_dropout_outside, obj->tags_outside_rows, 
                                                                obj->tags_outside_cols, obj->wThresholdVal, &key_tags, &key_tags_rowmajor, &key_dropout_rowmajor, &key_dropout_rowmajor2, \
                                                                NULL, NULL, obj->enclave_cache, obj->enclave_cache_size, false);

    printf("new_tag_rows: %d\n", new_tag_rows);

    matrix_real *tags_cache = new matrix_real[obj->sampleSize];
    uint8_t ctr_tag[16] = {0};
    for (int i = 0; i < remain_row.size(); i++) {
        printf("\033[34m row  %d  -  %d begin... \033[0m\n", i, remain_row[i]);
        matrix_real *out_test_row = &test_outside[i * obj->sampleSize];

        if (sgx_aes_ctr_decrypt(&key_tags_rowmajor, (uint8_t *)out_test_row, obj->sampleSize * sizeof(matrix_real), ctr_tag, 128, (uint8_t *)(tags_cache)) != SGX_SUCCESS) {
            printf("\033[33m %s: sgx_aes_ctr_encrypt failed - %d -\033[0m\n", __FUNCTION__, i);
            while (1)
                ;        
        }
        
        // printf("ctr_tag: ");
        // for (int i = 0; i < 16; i++)
        //     printf("%d  ", ctr_tag[i]);
        // printf("}}}\n");
        
        for (int j = 0; j < obj->sampleSize; j++) {
            if (tags_cache[j] != (*(obj->tags))(remain_row[i], j)) {
                printf("\033[33m not equal in (%d, %d): %lf  %lf \033[0m\n", i, j, tags_cache[j], (*(obj->tags))(remain_row[i], j));
                while (1)
                    ;            
            }
        }

        printf("\033[34m row  %d done... \033[0m\n", i);
    }


    printf("\033[34m dropout... \033[0m\n");
    uint8_t ctr_dropout2[16] = {0};
    bool *dropout_cache2 = new bool[obj->sampleSize];
    for (int i = 0; i < remain_row.size(); i++) {
        printf("\033[34m row  %d begin... \033[0m\n", i);
        bool *out_test_row = &test_dropout_outside[i * obj->sampleSize];

        // printf("addr: %p  %p  %d\n", out_test_row, &key_dropout_rowmajor2, obj->sampleSize * sizeof(bool));
        // printf("ctr_dropout2: ");
        // for (int i = 0; i < 16; i++)
        //     printf("%d  ", ctr_dropout2[i]);
        // printf("}}}\n");

        if (sgx_aes_ctr_decrypt(&key_dropout_rowmajor2, (uint8_t *)out_test_row, obj->sampleSize * sizeof(bool), ctr_dropout2, 128, (uint8_t *)(dropout_cache2)) != SGX_SUCCESS) {
            printf("\033[35m %s: sgx_aes_ctr_encrypt failed - %d -\033[0m\n", __FUNCTION__, i);
            while (1)
                ;        
        }

        // for (int i = 0; i < 10; i++)
        //     printf("%x ", out_test_row[i]);
        // printf("===\n");
        
        
        for (int j = 0; j < obj->sampleSize; j++) {
            if (dropout_cache2[j] != obj->dropoutCandidates(remain_row[i], j)) {
                printf("\033[33m not equal in (%d, %d): %d  %d \033[0m\n", i, j, dropout_cache2[j], obj->dropoutCandidates(remain_row[i], j));
                return;
                while (1)
                    ;            
            }
        }

        printf("\033[34m row  %d done... \033[0m\n", i);
    }


}


void test_matrix_majorchange_outside_with_del_with_1bit(scData *obj)
{
        std::vector<int> remain_row;

        int drop_rows = 0, data_rows = 0, inter_rows = 0;
        for (int i = 0; i < obj->featureSize; i++) {
            size_t dropout_count = 0;
            size_t data_count = 0;
            for (int j = 0; j < obj->sampleSize; j++) {
                if (obj->dropoutCandidates(i, j))
                    dropout_count ++;
                if ((*(obj->tags))(i, j) < obj->wThresholdVal)
                    data_count ++;
            }

            if (dropout_count == obj->sampleSize)
                drop_rows ++;
            if (data_count == obj->sampleSize)
                data_rows ++;
            if (dropout_count == obj->sampleSize && data_count == obj->sampleSize)
                inter_rows ++;
            else
                remain_row.push_back(i);
        }
        printf("\033[36m drop_rows: %d  data_rows: %d  inter_rows: %d  remain_rows: %d \033[0m\n", drop_rows, data_rows, inter_rows, remain_row.size());


    //dropout
    printf("\033[35m dropout majorchange testing begin... \033[0m\n");
    ocall_gettime(0);
    bool *test_dropout_outside;
    ocall_aligned_malloc((uint64_t *)&test_dropout_outside, sizeof(bool) * obj->featureSize * obj->sampleSize);
    printf("test_dropout_outside: %p\n", test_dropout_outside);

    size_t new_dropout_rows = matrix_majorchange_outside_wrapper<bool>((bool *)(obj->dropoutCandidates_outside), (bool *)test_dropout_outside, obj->tags_outside_rows, \
                                    obj->tags_outside_cols, &key_dropout, &key_dropout_rowmajor, NULL, true, obj->enclave_cache, obj->enclave_cache_size);
    printf("new_dropout_rows: %d\n", new_dropout_rows);

    uint8_t ctr_dropout[16] = {0};
    bool *dropout_cache = new bool[obj->sampleSize];
    for (int i = 0; i < obj->featureSize; i++) {
        printf("\033[35m row  %d begin... \033[0m\n", i);
        bool *out_test_row = &test_dropout_outside[i * obj->sampleSize];

        if (sgx_aes_ctr_decrypt(&key_dropout_rowmajor, (uint8_t *)out_test_row, obj->sampleSize * sizeof(bool), ctr_dropout, 128, (uint8_t *)(dropout_cache)) != SGX_SUCCESS) {
            printf("\033[35m %s: sgx_aes_ctr_encrypt failed - %d -\033[0m\n", __FUNCTION__, i);
            while (1)
                ;        
        } 

        for (int j = 0; j < obj->sampleSize; j++) {
            if (dropout_cache[j] != obj->dropoutCandidates(i, j)) {
                printf("\033[35m not equal in (%d, %d) \033[0m\n", i, j);
                while (1)
                    ;            
            }
        }

        printf("\033[35m row  %d done... \033[0m\n", i);
    }
    ocall_gettime(1);

    printf("\033[34m tag majorchange testing with del begin... \033[0m\n");
    ocall_gettime(0);
    matrix_real *test_outside;
    ocall_aligned_malloc((uint64_t *)&test_outside, sizeof(matrix_real) * obj->featureSize * obj->sampleSize);
    printf("test_outside: %p\n", test_outside);
    size_t new_tag_rows = matrix_majorchange_outside_with_del_with_1bit((matrix_real *)(obj->tags_outside), (matrix_real *)test_outside, test_dropout_outside, (uint8_t*)test_dropout_outside, \
                                obj->tags_outside_rows, obj->tags_outside_cols, obj->wThresholdVal, &key_tags, &key_tags_rowmajor, &key_dropout_rowmajor, &key_dropout_rowmajor2, NULL, NULL, \
                                obj->enclave_cache, obj->enclave_cache_size, false);

    printf("new_tag_rows: %d\n", new_tag_rows);

    matrix_real *tags_cache = new matrix_real[obj->sampleSize];
    uint8_t ctr_tag[16] = {0};
    for (int i = 0; i < remain_row.size(); i++) {
        printf("\033[34m row  %d  -  %d begin... \033[0m\n", i, remain_row[i]);
        matrix_real *out_test_row = &test_outside[i * obj->sampleSize];

        if (sgx_aes_ctr_decrypt(&key_tags_rowmajor, (uint8_t *)out_test_row, obj->sampleSize * sizeof(matrix_real), ctr_tag, 128, (uint8_t *)(tags_cache)) != SGX_SUCCESS) {
            printf("\033[33m %s: sgx_aes_ctr_encrypt failed - %d -\033[0m\n", __FUNCTION__, i);
            while (1)
                ;        
        }
        
        // printf("ctr_tag: ");
        // for (int i = 0; i < 16; i++)
        //     printf("%d  ", ctr_tag[i]);
        // printf("}}}\n");
        
        for (int j = 0; j < obj->sampleSize; j++) {
            if (tags_cache[j] != (*(obj->tags))(remain_row[i], j)) {
                printf("\033[33m not equal in (%d, %d): %lf  %lf \033[0m\n", i, j, tags_cache[j], (*(obj->tags))(remain_row[i], j));
                while (1)
                    ;            
            }
        }

        printf("\033[34m row  %d done... \033[0m\n", i);
    }


    printf("\033[34m dropout... \033[0m\n");
    uint8_t ctr_dropout2[16] = {0};
    uint8_t *dropout_cache2 = new uint8_t[obj->sampleSize];
    // size_t row_bytes = (obj->sampleSize + 0x7) & ~0x7;
    size_t row_bytes = (obj->sampleSize + 0x7) >> 3;
    for (int i = 0; i < remain_row.size(); i++) {
        printf("\033[34m row  %d begin... \033[0m\n", i);
        // bool *out_test_row = &test_dropout_outside[i * obj->sampleSize];
        size_t offset = row_bytes * i;
        uint8_t *out_test_row = (uint8_t*)test_dropout_outside + offset;

        //testing
        // printf("addr: %p  %p  %d\n", out_test_row, &key_dropout_rowmajor2, obj->sampleSize * sizeof(bool));
        // printf("ctr_dropout2: ");
        // for (int i = 0; i < 16; i++)
        //     printf("%d  ", ctr_dropout2[i]);
        // printf("}}}\n");

        if (sgx_aes_ctr_decrypt(&key_dropout_rowmajor2, (uint8_t *)out_test_row, row_bytes, ctr_dropout2, 128, (uint8_t *)(dropout_cache2)) != SGX_SUCCESS) {
            printf("\033[35m %s: sgx_aes_ctr_encrypt failed - %d -\033[0m\n", __FUNCTION__, i);
            while (1)
                ;        
        }

        // for (int i = 0; i < 10; i++)
        //     printf("%x ", out_test_row[i]);
        // printf("===\n");
        
        
        for (int j = 0; j < obj->sampleSize; j++) {
            bool tmp_val = ( (dropout_cache2[j>>3] >> (j&0x07)) & 0x1) ? true : false;
            if (tmp_val != obj->dropoutCandidates(remain_row[i], j)) {
                printf("\033[33m not equal in (%d, %d): %d  %d \033[0m\n", i, j, dropout_cache2[j], obj->dropoutCandidates(remain_row[i], j));
                while (1)
                    ;            
            }
        }

        printf("\033[34m row  %d done... \033[0m\n", i);
    }

    printf("dropout row_bytes: %d\n", row_bytes);
}




//ICE
void input_plain(char *buf, uint64_t &raw_data_outside_addr, uint64_t &raw_data_addr, \
        size_t &loaded_rows, size_t &loaded_cols, size_t input_rows, size_t input_cols) {
    int rows = -1, cols = 0;
    char *st = buf;
    // Skip the name row and the feature column
    while(*st!='\n')++st;
    for(char *i=st;*i;i++)
        if(*i=='\n')++rows;
    for(char *i=st+1;*i!='\n';++i)
        if(*i==',')++cols;
    
    matrix_real *raw_data_outside, *tmp_raw_data_outside;
    ocall_aligned_malloc((uint64_t *)&tmp_raw_data_outside, sizeof(matrix_real) * input_rows * cols);
    ocall_aligned_malloc((uint64_t *)&raw_data_outside, sizeof(matrix_real) * input_rows * cols);
    matrix_real *raw_data = (matrix_real *)Eigen::internal::aligned_malloc(sizeof(matrix_real) * input_rows * cols);

    printf("raw_data: %p   raw_data_outside: %p  tmp_raw_data_outside: %p \n", raw_data, raw_data_outside, tmp_raw_data_outside);

    Vector<matrix_real> cur_row = Vector<matrix_real>::Zero(cols);
    int i = -1, j = 0;
    char *prev = 0;
    for(;*st;st++) 
        if(*st=='\n') {
            if(i==-1)i++;
            else {
                *st=0;
                cur_row(j++) = atof(prev);
                if(cur_row.sum()>0) {
                    for (int col = 0; col < cols; col++) {
                        tmp_raw_data_outside[col * input_rows + i] = cur_row(col);
                        raw_data[col * input_rows + i] = cur_row(col);
                    }
                    i++;
                }
            }
            j=0;
            if(rows--==0)break;
            while(*st!=',')++st; // skip the column name
            prev = st+1;
        } else if(*st==',') {
            *st=0;
            cur_row(j++) = atof(prev);
            prev = st+1;
        }

    printf("Data format:\n\
            # rows = %d, # cols = %d\n", i, cols);
    loaded_rows = i;
    loaded_cols = cols;


    //testing
    for (int k1 = 0; k1 < loaded_cols; k1++) {
        for (int k2 = 0; k2 < loaded_rows; k2++) {
            if (tmp_raw_data_outside[k1 * loaded_rows + k2] != raw_data[k1 * loaded_rows + k2]) {
                printf("%d: (%d, %d) not eqaul!\n", __LINE__, k2, k1);
                while (1)
                    ;
            }
        }
    }

    uint8_t ctr[16] = {0};
    matrix_real *tags_data = tmp_raw_data_outside;
    matrix_real *out_tags_data = raw_data_outside;
    
    for (int col = 0; col < loaded_cols; col++) {
        // printf("col %d\n", col);
        matrix_real *col_data = &tags_data[col * loaded_rows];
        matrix_real *out_col_data = &out_tags_data[col * loaded_rows];
        // note: ctr is updated
        if (sgx_aes_ctr_encrypt(&key_tags, (uint8_t *)col_data, loaded_rows * sizeof(matrix_real), ctr, 128, (uint8_t *)out_col_data) != SGX_SUCCESS) {
            printf("\033[33m scData: sgx_aes_ctr_encrypt failed \033[0m\n");
        }
    }

    //testing
    uint8_t test_ctr[16] = {0};
    matrix_real *test_cache = new matrix_real[loaded_rows];
    for (int col = 0; col < loaded_cols; col++) {
        matrix_real *out_col_data = &out_tags_data[col * loaded_rows];
        
        if (sgx_aes_ctr_decrypt(&key_tags, (uint8_t *)out_col_data, loaded_rows * sizeof(matrix_real), test_ctr, 128, (uint8_t *)test_cache) != SGX_SUCCESS) {
            printf("\033[33m scData: sgx_aes_ctr_encrypt failed \033[0m\n");
        }

        for (int row = 0; row < loaded_rows; row++) {
            if (test_cache[row] != raw_data[col * loaded_rows + row]) {
                printf("\033[33m %d: (%d, %d) not equal \033[0m\n", __LINE__, row, col);
                while (1)
                    ;                
            }
        }
    }

    raw_data_outside_addr = (uint64_t)raw_data_outside;
    raw_data_addr = (uint64_t)raw_data;
    
    // TODO: free tmp_raw_data_outside
    // TODO: free buf
}

void real_eval(matrix_real *raw_data_outside, matrix_real *raw_data, size_t rows, size_t cols) {
    printf("Constructing scData Object\n");
    ocall_gettime(0);
        // scData *obj = new scData(&data);
        scData *obj = new scData(raw_data_outside, raw_data, rows, cols);
    ocall_gettime(1);

    // for (int i = 0; i < 20; i++) {
    //     for (int j = 0; j < 20; j++) {
    //         printf("%f ", obj->tags->operator()(i, j));
    //     }
    //     printf("\n");
    // }
    printf("----------------------------------\n");

    printf("Determining Dropout Candidates\n");
    ocall_gettime(0);
        obj->determineDropoutCandidates();
    ocall_gettime(1);


    printf("Computing Threshold w\n");
    ocall_gettime(0);
        obj->wThreshold();
    ocall_gettime(1);
    printf("wThresholdVal = %.15f, pDropoutCoefA = %.3f, \
            pDropoutCoefB = %.3f\n", obj->wThresholdVal, obj->pDropoutCoefA, obj->pDropoutCoefB);

    printf("Changing Major of dropoutCandidates and tags\n");
    ocall_gettime(0);
        obj->changeMajor();
        // obj->changeMajor_1bit();
    ocall_gettime(1);

    printf("Computing Dissimilarity Matrix\n");
    ocall_gettime(0);
    // For testing
    // obj->wThresholdVal = 13.70391;
        obj->scDissim();
    ocall_gettime(1);

    // for (int i = 0; i < 10; i++) {
    //     for (int j = 0; j < 10; j++) {
    //         printf("%lf ", obj->dissim(i, j));
    //     }
    //     printf("\n");
    // }
    // printf("----------------------------------\n");
    // for (int i = 10; i < 20; i++) {
    //     for (int j = 10; j < 20; j++) {
    //         printf("%lf ", obj->dissim(i, j));
    //     }
    //     printf("\n");
    // }
    // printf("----------------------------------\n");
    // for (int i = 3000-10; i < 3000; i++) {
    //     for (int j = 3000-10; j < 3000; j++) {
    //         printf("%lf ", obj->dissim(i, j));
    //     }
    //     printf("\n");
    // }
    // printf("----------------------------------\n");

    real tmp_sum = 0;
    for (int i = 0; i < obj->sampleSize; i++) {
        for (int j = 0; j < obj->sampleSize; j++) {
            tmp_sum += obj->dissim(i, j);
        }
    }
    printf("sum of dissim: %.16f\n", tmp_sum);    


    printf("Performing Princliple Coordinate Analysis\n");
    ocall_gettime(0);
        obj->scPCoA();
    ocall_gettime(1);
    // for(int i=0;i<10;i++)
    //     printf("%f ", obj->PC(0, i));
    // printf("Determining the number of Principle Coordinate A be used\n");
    // ocall_gettime(0);
    //      obj->nPC();
    // ocall_gettime(1);

    // printf("# PC: %d\n", obj->nPCVal);
    real sum = 0;
    for(int j=0;j<obj->nPCVal;j++) {
        sum+=obj->variation[j];
        printf("%f ", obj->variation[j]);
    }
    printf("\nsum of var: %f\n", sum);

    printf("Performing Hierarchical Clustering\n");
    ocall_gettime(0);
        obj->scCluster();
    ocall_gettime(1);
    printf("# clusters: %d", obj->nCluster);
    printf("to File\n");
    ocall_gettime(0);
        obj->toFile("./output.enc");
    ocall_gettime(1);


    printf("Finihsed\n");
}


//ICE
void ecall_eval_plain(char *buf, size_t input_rows, size_t input_cols) {
    matrix_real *raw_data_outside, *raw_data;
    uint64_t raw_data_outside_addr, raw_data_addr;
    size_t rows, cols;
    input_plain(buf, raw_data_outside_addr, raw_data_addr, rows, cols, input_rows, input_cols);
    raw_data_outside = (matrix_real *)raw_data_outside_addr;
    raw_data = (matrix_real *)raw_data_addr;

    printf("%s: row = %d col = %d\n", __FUNCTION__, rows, cols);
    printf("%s: raw_data - %p  raw_data_outside - %p\n", __FUNCTION__, raw_data, raw_data_outside);

    real_eval(raw_data_outside, raw_data, rows, cols);
}



//ICE
void ecall_eval_cipher(char *buf) {

    size_t rows = ((size_t*)buf)[0];
    size_t cols = ((size_t*)buf)[1];

    matrix_real *raw_data_outside = (matrix_real *)( (uint64_t)buf + sizeof(size_t) + sizeof(size_t) );

    // matrix_real *raw_data = (matrix_real *)Eigen::internal::aligned_malloc(sizeof(matrix_real) * rows * cols);
    // matrix_real *out_tags_data = (matrix_real *)raw_data_outside;
    // matrix_real *in_tags_data = raw_data;
    // uint8_t ctr[16] = {0};
    // for (int i = 0; i < cols; i++) {
    //     matrix_real *out_tags_col = &out_tags_data[i * rows];
    //     matrix_real *in_tags_col = &in_tags_data[i * rows];
        
    //     if (sgx_aes_ctr_decrypt(&key_tags, (uint8_t *)out_tags_col, rows * sizeof(matrix_real), ctr, 128, (uint8_t *)(in_tags_col)) != SGX_SUCCESS) {
    //             printf("\033[33m %s: sgx_aes_ctr_encrypt failed \033[0m\n", __FUNCTION__);
    //     }
    // }

    printf("%s: row = %d col = %d\n", __FUNCTION__, rows, cols);
    // printf("%s: raw_data - %p  raw_data_outside - %p\n", __FUNCTION__, raw_data, raw_data_outside);

    real_eval(raw_data_outside, NULL, rows, cols);
}