// #include <iostream>

#include "Enclave.h"
#include "Enclave_t.h" /* print_string */

#include <math.h>
#include <stdio.h>
#include <algorithm>
#include <numeric>
#include <vector>
#include <unsupported/Eigen/NonLinearOptimization>

#include "dissim.h"
#include "eigenvectors.h"
#include "hClustering.h"
#include "kde.h"
#include "lmFunctor.h"
#include "scData.h"
#include "utils.h"

#include <iostream>

const matrix_real epsilon = std::numeric_limits<matrix_real>::epsilon();

template <typename T1, typename T2>
void adjust(T1 a, T1 b, T2 &count1, T2 &count2){
    T1 p = f(a, b, count2);
    count1 = p*count2 + (1.0-p)*count1;
}


//ICE
scData::scData(matrix_real *raw_data_outside, matrix_real *raw_data, size_t rows, size_t cols, Tag t)
                : sampleSize(cols), featureSize(rows), priorTPM(1.0), tagType(t) {

    printf("%s: featureSize - %d  sampleSize - %d\n", __FUNCTION__, featureSize, sampleSize);

    enclave_cache_size = 70 * 1024 * 1024;
    enclave_cache = (void *)Eigen::internal::aligned_malloc(enclave_cache_size);
    if (enclave_cache == NULL) {
        printf("\033[33m scData: alloc mem for enclave_cache failed \033[0m\n");
    }

    libSize = (matrix_real *) Eigen::internal::aligned_malloc(sizeof(matrix_real) * sampleSize);
    if (libSize == NULL) {
        printf("\033[31m ERR: allocate memory for libSize failed  \033[0m\n");
    }

    matrix_real *tags_cache = (matrix_real *)enclave_cache;
    uint8_t ctr_in_tag[16] = {0};
    uint8_t ctr_out_tag[16] = {0};
    matrix_real *out_tags_data = (matrix_real *)raw_data_outside;
    if (tagType == CPM) {
        for (int i = 0; i < sampleSize; i++) {
            libSize[i] = Mi;
        }

        for (int i = 0; i < sampleSize; i++) {
            matrix_real *out_tags_col = &out_tags_data[i * featureSize];

            if (sgx_aes_ctr_decrypt(&key_tags, (uint8_t *)out_tags_col, featureSize * sizeof(matrix_real), ctr_in_tag, 128, (uint8_t *)(tags_cache)) != SGX_SUCCESS) {
                printf("\033[33m %s: sgx_aes_ctr_encrypt failed \033[0m\n", __FUNCTION__);
            }

            for (int j = 0; j < sampleSize; j++) {
                tags_cache[j] = std::log2(tags_cache[j] + priorTPM);
            }

            if (sgx_aes_ctr_encrypt(&key_tags, (uint8_t *)tags_cache, featureSize * sizeof(matrix_real), ctr_out_tag, 128, (uint8_t *)(out_tags_col)) != SGX_SUCCESS) {
                printf("\033[33m %s: sgx_aes_ctr_encrypt failed \033[0m\n", __FUNCTION__);
            }
        }
    
    } else {
        for (int i = 0; i < sampleSize; i++) {
            matrix_real *out_tags_col = &out_tags_data[i * featureSize];

            if (sgx_aes_ctr_decrypt(&key_tags, (uint8_t *)out_tags_col, featureSize * sizeof(matrix_real), ctr_in_tag, 128, (uint8_t *)(tags_cache)) != SGX_SUCCESS) {
                printf("\033[33m %s: sgx_aes_ctr_encrypt failed \033[0m\n", __FUNCTION__);
            }

            libSize[i] = 0;
            for (int j = 0; j < featureSize; j++) {
                libSize[i] += tags_cache[j];
            }

            for (int j = 0; j < featureSize; j++) {
                // testing
                // if (tags_cache[j] != raw_data[i * featureSize + j]) {
                //     printf("%d: (%d, %d) not equal!\n", __LINE__, j, i);
                // }

                tags_cache[j] = std::log2(tags_cache[j] * Mi / libSize[i] + priorTPM);
            }

            if (sgx_aes_ctr_encrypt(&key_tags, (uint8_t *)tags_cache, featureSize * sizeof(matrix_real), ctr_out_tag, 128, (uint8_t *)(out_tags_col)) != SGX_SUCCESS) {
                printf("\033[33m %s: sgx_aes_ctr_encrypt failed \033[0m\n", __FUNCTION__);
            }
        }

    }

    this->tags_outside = raw_data_outside;
    this->tags_outside_rows = featureSize;
    this->tags_outside_cols = sampleSize;

    if (raw_data != NULL) {
    // deal with raw_data
        new (&tags_struct) MatrixMap<matrix_real>(raw_data, featureSize, sampleSize);
        tags = &tags_struct;

        if (tagType == CPM) {
            for (int i = 0; i < sampleSize; i++) {
                for (int j = 0; j < featureSize; j++) {
                    (*tags)(j, i) = std::log2((*tags)(j, i) + priorTPM);
                }
            }
        } else {
            for (int i = 0; i < sampleSize; i++) {
                for (int j = 0; j < featureSize; j++) {
                    (*tags)(j, i) = std::log2((*tags)(j, i) / libSize[i] * Mi + priorTPM);
                }
            }
        }
    }
}


scData::scData(Matrix<matrix_real> *_tags, Tag t)\
        :sampleSize(_tags->cols()), featureSize(_tags->rows()), \
        priorTPM(1.0+1e-8), tags(_tags), tagType(t) {

    libSize = (matrix_real *) Eigen::internal::aligned_malloc(sizeof(matrix_real) * sampleSize);
    if (libSize == NULL) {
        printf("\033[31m ERR: allocate memory for libSize failed  \033[0m\n");
    }
    
    // maybe this can be integrated into ecall_input for further optimization
    if (tagType == CPM) {
        for (int i = 0; i < sampleSize; i++) {
            libSize[i] = 1;
        }
    } else {
        for (int i = 0; i < sampleSize; i++) {
            libSize[i] = 0;
            for (int j = 0; j < featureSize; j++) {
                libSize[i] += (*tags)(j, i);
            }

            for (int k = 0; k < featureSize; k++) {
                (*tags)(k, i) /= libSize[i]; 
            }
        }

    }

    plus_log_2(tags, priorTPM);
}

scData::scData(Matrix<matrix_real> *_tags,  void *buf_outside, Tag t)\
        :sampleSize(_tags->cols()), featureSize(_tags->rows()), \
        priorTPM(1.0+1e-8), tags(_tags), tagType(t) {

    libSize = (matrix_real *) Eigen::internal::aligned_malloc(sizeof(matrix_real) * sampleSize);
    if (libSize == NULL) {
        printf("\033[31m ERR: allocate memory for libSize failed  \033[0m\n");
    }
    
    // maybe this can be integrated into ecall_input for further optimization
    if (tagType == CPM) {
        for (int i = 0; i < sampleSize; i++) {
            libSize[i] = 1;
        }
    } else {
        for (int i = 0; i < sampleSize; i++) {
            libSize[i] = 0;
            for (int j = 0; j < featureSize; j++) {
                libSize[i] += (*tags)(j, i);
            }

            for (int k = 0; k < featureSize; k++) {
                (*tags)(k, i) /= libSize[i]; 
            }
        }

    }

    plus_log_2(tags, priorTPM);

    this->tags_outside = buf_outside;
    this->tags_outside_cols = sampleSize;
    this->tags_outside_rows = featureSize;
    
    uint8_t ctr[16] = {0};
    matrix_real *tags_data = tags->data();
    matrix_real *out_tags_data = (matrix_real *)tags_outside;
    for (int col = 0; col < this->tags_outside_cols; col++) {
        matrix_real *col_data = &tags_data[col * featureSize];
        matrix_real *out_col_data = &out_tags_data[col * featureSize];
        // note: ctr is updated
        if (sgx_aes_ctr_encrypt(&key_tags, (uint8_t *)col_data, featureSize * sizeof(matrix_real), ctr, 128, (uint8_t *)out_col_data) != SGX_SUCCESS) {
            printf("\033[33m scData: sgx_aes_ctr_encrypt failed \033[0m\n");
        }
    }

    printf("tags_outside: %p\n", tags_outside);

    // 70 MB
    enclave_cache_size = 70 * 1024 * 1024;
    enclave_cache = (void *)Eigen::internal::aligned_malloc(enclave_cache_size);
    if (enclave_cache == NULL) {
        printf("\033[33m scData: alloc mem for enclave_cache failed \033[0m\n");
    }
}


void scData::determineDropoutCandidates(real min1, \
             real min2, int N, real alpha, bool fast, \
             bool zerosOnly, real bw_adjust) {

    if (zerosOnly) {
        printf("\033[33m determineDropoutCandidates: zerosOnly not implemented \033[0m\n");
        return;
    }

    int *topLibraries = new int[sampleSize];
    std::iota(topLibraries, topLibraries+sampleSize, 0);
    auto& tmp = libSize;
    if (fast && sampleSize > N && tagType == RAW) {
        std::sort(topLibraries, topLibraries + sampleSize, \
            [&](int idx1, int idx2) {return libSize[idx1] > \
                                        libSize[idx2];});
    } else 
        N = sampleSize;
    std::vector<real> dTs(N, 0);
    matrix_real *LT1 = new matrix_real[N];
    matrix_real *LT2 = new matrix_real[N];
    for(int i=0;i<N;i++) {
        LT1[i] = min1/libSize[topLibraries[i]];
        LT2[i] = min2/libSize[topLibraries[i]];
    }
    plus_log_2(LT1, N, priorTPM);
    plus_log_2(LT2, N, priorTPM);

#if 0
    //single thread
    matrix_real *tmp_tags_cache = (matrix_real *)enclave_cache;
    matrix_real *out_tags_data = (matrix_real *)tags_outside;
    size_t bytes_per_col = featureSize * sizeof(matrix_real);
    size_t count_per_col = (bytes_per_col / 16) + ( (bytes_per_col % 16) ? 1 : 0 );

    printf("bytes_per_col:  %d   count_per_col: %d\n", bytes_per_col, count_per_col); 
    for (int i = 0; i < N; i++) {

        uint8_t ctr[16] = {0};
        matrix_real *out_col_data = &out_tags_data[topLibraries[i] * featureSize];
        // ctr128_inc_for_bytes(ctr, ((featureSize * sizeof(matrix_real)) * topLibraries[i + j]);
        ctr128_inc_for_count(ctr, count_per_col * topLibraries[i]);

        if (sgx_aes_ctr_decrypt(&key_tags, (uint8_t *)out_col_data, bytes_per_col, ctr, 128, (uint8_t *)(tmp_tags_cache)) != SGX_SUCCESS) {
            printf("\033[33m determineDropoutCandidates: sgx_aes_ctr_encrypt failed \033[0m\n");
        }

        matrix_real *col_data = tmp_tags_cache;
        size_t col_data_size = featureSize;
        real dfn_max = KDE::density(col_data, col_data_size, \
                    KDE::Epanechnikov, 1024, true, KDE::bw_default, bw_adjust, LT2[i]);
        dTs[i] = KDE::density(col_data, col_data_size, KDE::Epanechnikov, \
                    1024, false, KDE::bw_default, bw_adjust, LT1[i], dfn_max);
    }
#else
    //multiple threads
    matrix_real *out_tags_data = (matrix_real *)tags_outside;

    // matrix_real *tmp_tags_cache = (matrix_real *)enclave_cache;
    matrix_real *tmp_tags_cache_arr[THREAD_NUM];
    int thread_num = THREAD_NUM;
    for (int i = 0; i < thread_num; i++) {
        tmp_tags_cache_arr[i] = (matrix_real *) enclave_cache + featureSize * i;
    }
    
    int col_size = this->featureSize;
    for (int tid = 0; tid < thread_num; tid++) {
        fun_ptrs[tid] = [tid, thread_num, out_tags_data, tmp_tags_cache_arr, topLibraries, LT1, LT2, &dTs, col_size, N, bw_adjust]() {
            int count = N / thread_num;
            int actual_count = (tid + 1 == thread_num) ? (N - count * tid) : count;
            int st = count * tid;
            int ed = st + actual_count;

            size_t bytes_per_col = col_size * sizeof(matrix_real);
            size_t count_per_col = (bytes_per_col / 16) + ( (bytes_per_col % 16) ? 1 : 0 );

            matrix_real *tmp_tags_cache = tmp_tags_cache_arr[tid];
            for (int i = st; i < ed; i++) {
                uint8_t ctr[16] = {0};
                
                matrix_real *out_col_data = &out_tags_data[topLibraries[i] * col_size];
                ctr128_inc_for_count(ctr, count_per_col * topLibraries[i]);

                if (sgx_aes_ctr_decrypt(&key_tags, (uint8_t *)out_col_data, bytes_per_col, ctr, 128, (uint8_t *)(tmp_tags_cache)) != SGX_SUCCESS) {
                    printf("\033[33m determineDropoutCandidates: sgx_aes_ctr_encrypt failed \033[0m\n");
                }

                matrix_real *col_data = tmp_tags_cache;
                size_t col_data_size = col_size;
                real dfn_max = KDE::density(col_data, col_data_size, \
                            KDE::Epanechnikov, 1024, true, KDE::bw_default, bw_adjust, LT2[i]);
                dTs[i] = KDE::density(col_data, col_data_size, KDE::Epanechnikov, \
                            1024, false, KDE::bw_default, bw_adjust, LT1[i], dfn_max);
            }
        };
    }

    task_notify(thread_num);
    fun_ptrs[0]();
    task_wait_done(thread_num);
#endif

    delete[] topLibraries;
    delete[] LT1; 
    delete[] LT2;

    if (fast && sampleSize>N && tagType == RAW) {
        matrix_real dThreshold = quantile(dTs, 0.5);

        dTs.resize(sampleSize);
        for (int i = 0; i < sampleSize; i++)
            dTs[i] = dThreshold;
    } else {
        real limit1 = quantile(dTs, alpha); 
        real limit2 = quantile(dTs, 1.0-alpha);
        for(int i=0;i<sampleSize;i++)
            if(dTs[i]<limit1) dTs[i] = limit1;
            else if(dTs[i]>limit2) dTs[i] = limit2;
    }

    // printf("calc dropoutCandidate...\n");

    ocall_aligned_malloc((uint64_t *)&dropoutCandidates_outside, sizeof(bool) * featureSize * sampleSize);
    // printf("dropoutCandidates_outside: %p\n", dropoutCandidates_outside);
    dropoutCandidates_outside_rows = featureSize;
    dropoutCandidates_outside_cols = sampleSize;

    // printf("return from ocall_aligned_malloc\n");

    matrix_real *tags_cache = (matrix_real *)enclave_cache;
    bool *dropout_cache = (bool *) ((uint64_t)enclave_cache + sizeof(matrix_real) * featureSize * 2);

    // matrix_real *out_tags_data = (matrix_real *)tags_outside;
    bool *out_dropout_data = (bool *)dropoutCandidates_outside;
    uint8_t ctr_tag[16] = {0};
    uint8_t ctr_dropout[16] = {0};
    for (int i = 0; i < sampleSize; i ++) {
        matrix_real *out_tags_col = &out_tags_data[i * featureSize];
        bool *out_dropout_col = &out_dropout_data[i * featureSize];

        if (sgx_aes_ctr_decrypt(&key_tags, (uint8_t *)out_tags_col, featureSize * sizeof(matrix_real), ctr_tag, 128, (uint8_t *)(tags_cache)) != SGX_SUCCESS) {
                printf("\033[33m %s: sgx_aes_ctr_encrypt failed \033[0m\n", __FUNCTION__);
        }

        for (int j = 0; j < featureSize; j++) {
            dropout_cache[j] = tags_cache[j] < dTs[i];
        }

        if (sgx_aes_ctr_encrypt(&key_dropout, (uint8_t *)dropout_cache, featureSize * sizeof(bool), ctr_dropout, 128, (uint8_t *)(out_dropout_col)) != SGX_SUCCESS) {
                printf("\033[33m %s: sgx_aes_ctr_encrypt failed \033[0m\n", __FUNCTION__);
        }
    }
}



void scData::wThreshold(real cutoff) {

    matrix_real *dropoutRates_data = new matrix_real[featureSize];
    matrix_real *averLcpm_data = new matrix_real[featureSize];
    int *dropoutCandidates_row_count = new int[featureSize];
    int *tmp_cnt = new int[featureSize];
    real *sum = new real[featureSize];

    for (int i = 0; i < featureSize; i++) {
        dropoutCandidates_row_count[i] = 0;
        tmp_cnt[i] = 0;
        sum[i] = 0.0;
    }

    matrix_real *tags_cache = (matrix_real *)enclave_cache;
    bool *dropout_cache = (bool *) ((uint64_t) enclave_cache + sizeof(matrix_real) * featureSize * 2);
    matrix_real *out_tags_data = (matrix_real *)tags_outside;
    bool *out_dropout_data = (bool *)dropoutCandidates_outside;
    uint8_t ctr_tag[16] = {0};
    uint8_t ctr_dropout[16] = {0};

    for (int i = 0; i < sampleSize; i++) {
        matrix_real *out_tags_col = &out_tags_data[i * featureSize];
        bool *out_dropout_col = &out_dropout_data[i * featureSize];

        if (sgx_aes_ctr_decrypt(&key_tags, (uint8_t *)out_tags_col, featureSize * sizeof(matrix_real), ctr_tag, 128, (uint8_t *)(tags_cache)) != SGX_SUCCESS) {
                printf("\033[33m %s: sgx_aes_ctr_encrypt failed \033[0m\n", __FUNCTION__);
        }

        if (sgx_aes_ctr_decrypt(&key_dropout, (uint8_t *)out_dropout_col, featureSize * sizeof(bool), ctr_dropout, 128, (uint8_t *)(dropout_cache)) != SGX_SUCCESS) {
                printf("\033[33m %s: sgx_aes_ctr_encrypt failed \033[0m\n", __FUNCTION__);
        }


        for (int j = 0; j < featureSize; j++) {
            if (dropout_cache[j]) {
                dropoutCandidates_row_count[j] ++;
            } else if (tags_cache[j] > 0) {
                tmp_cnt[j] ++;
                sum[j] += tags_cache[j];
            }
        }
    }

    size_t cnt = 0;
    for (int i = 0; i < featureSize; i++) {
        if (dropoutCandidates_row_count[i] == sampleSize)
            continue;
        
        dropoutRates_data[cnt] = (real)dropoutCandidates_row_count[i] / (real)sampleSize;
        averLcpm_data[cnt] = sum[i] / tmp_cnt[i];
        cnt++;
    }

    delete[] dropoutCandidates_row_count;
    delete[] tmp_cnt;
    delete[] sum;

    // x -> averLcpm, y -> dropoutRates, function -> y = 1/(1+e^(ax-b))
    // more elegent implementation needed here
    Vector<matrix_real> dropoutRates(cnt);
    ColArray<matrix_real> averLcpm(cnt);
    for (int i = 0; i < cnt; i++) {
        dropoutRates[i] = dropoutRates_data[i];
        averLcpm[i] = averLcpm_data[i];
    }

    // printf("wThreshold: cnt - %d\n", cnt);

    ColVector<matrix_real> x(2);
    x(0) = 1; x(1) = round(quantile(averLcpm_data, cnt, 0.5));
    LMFunctor functor(averLcpm_data, dropoutRates_data, cnt);

    Eigen::LevenbergMarquardt<LMFunctor, matrix_real> lm(functor);
    lm.minimize(x);
    pDropoutCoefA = x(0);
    pDropoutCoefB = x(1);
    wThresholdVal = matrix_real(1)/pDropoutCoefA * log(1.0/cutoff - 1.0) + pDropoutCoefB;
    
    delete[] dropoutRates_data;
    delete[] averLcpm_data;
}


# if 0
void scData::changeMajor() {
    ocall_aligned_malloc((uint64_t *)&tags_row, sizeof(matrix_real) * \
            featureSize * sampleSize);
    size_t new_tag_rows = matrix_majorchange_outside_wrapper<matrix_real>( \
                (matrix_real*)(tags_outside), (matrix_real*)(tags_row), \
                tags_outside_rows, tags_outside_cols, &key_tags, \
                &key_tags_rowmajor, NULL, true, enclave_cache, \
                enclave_cache_size);
    ocall_aligned_malloc((uint64_t *)&dropout_row, sizeof(bool) * \
            featureSize * sampleSize);
    size_t new_drop_rows = matrix_majorchange_outside_wrapper<bool>( \
                (bool*)(dropoutCandidates_outside), \
                (bool*)(dropout_row), \
                dropoutCandidates_outside_rows, \
                dropoutCandidates_outside_cols, &key_dropout, \
                &key_dropout_rowmajor, NULL, true, enclave_cache, \
                enclave_cache_size);
}

#else
// can del row
void scData::changeMajor() {

    ocall_aligned_malloc((uint64_t *)&dropout_row, sizeof(bool) * \
            featureSize * sampleSize);
    size_t new_drop_rows = matrix_majorchange_outside_wrapper<bool>( \
                (bool*)(dropoutCandidates_outside), \
                (bool*)(dropout_row), \
                dropoutCandidates_outside_rows, \
                dropoutCandidates_outside_cols, &key_dropout, \
                &key_dropout_rowmajor, NULL, true, enclave_cache, \
                enclave_cache_size);
ocall_gettime(1);
    ocall_aligned_malloc((uint64_t *)&tags_row, sizeof(matrix_real) * \
            featureSize * sampleSize);
    size_t new_tag_rows = matrix_majorchange_outside_with_del((matrix_real *)(tags_outside), \
                (matrix_real *)(tags_row), (bool *)dropout_row, (bool *)dropout_row, tags_outside_rows, tags_outside_cols, \
                wThresholdVal, &key_tags, &key_tags_rowmajor, &key_dropout_rowmajor, &key_dropout_rowmajor2, \
                NULL, NULL, enclave_cache, enclave_cache_size, false);

    printf("new_tag_rows: %d\n", new_tag_rows);

    featureSize = new_tag_rows;
    for (int i = 0; i < 16; i++)
        key_dropout_rowmajor[i] = key_dropout_rowmajor2[i];
    // printf("key_dropout_major: ");
    // for (int i = 0; i < 16; i++)
    //     printf("%d ", key_dropout_rowmajor[i]);
    // printf("]]]\n");
    
}
#endif

void scData::changeMajor_1bit() {

    ocall_aligned_malloc((uint64_t *)&dropout_row, sizeof(bool) * \
            featureSize * sampleSize);
    size_t new_drop_rows = matrix_majorchange_outside_wrapper<bool>( \
                (bool*)(dropoutCandidates_outside), \
                (bool*)(dropout_row), \
                dropoutCandidates_outside_rows, \
                dropoutCandidates_outside_cols, &key_dropout, \
                &key_dropout_rowmajor, NULL, true, enclave_cache, \
                enclave_cache_size);

    ocall_aligned_malloc((uint64_t *)&tags_row, sizeof(matrix_real) * \
            featureSize * sampleSize);
    size_t new_tag_rows = matrix_majorchange_outside_with_del_with_1bit((matrix_real *)(tags_outside), \
                (matrix_real *)(tags_row), (bool *)dropout_row, (uint8_t *)dropout_row, tags_outside_rows, tags_outside_cols, \
                wThresholdVal, &key_tags, &key_tags_rowmajor, &key_dropout_rowmajor, &key_dropout_rowmajor2, \
                NULL, NULL, enclave_cache, enclave_cache_size, true);

    printf("new_tag_rows: %d\n", new_tag_rows);

    featureSize = new_tag_rows;
    for (int i = 0; i < 16; i++)
        key_dropout_rowmajor[i] = key_dropout_rowmajor2[i];

    // testing
#if 0
    printf("\033[34m dropout... \033[0m\n");
    // std::vector<int> remain_row;
    // for (int k = 0; k < featureSize; k++)
    //     remain_row.push_back(k);

    uint8_t ctr_dropout2[16] = {0};
    uint8_t *dropout_cache2 = new uint8_t[sampleSize];
    // size_t row_bytes = (obj->sampleSize + 0x7) & ~0x7;
    size_t row_bytes = (sampleSize + 0x7) >> 3;
    for (int i = 0; i < featureSize; i++) {
        printf("\033[34m row  %d begin... \033[0m\n", i);
        // bool *out_test_row = &test_dropout_outside[i * obj->sampleSize];
        size_t offset = row_bytes * i;
        uint8_t *out_test_row = (uint8_t*)dropout_row + offset;


        if (sgx_aes_ctr_decrypt(&key_dropout_rowmajor2, (uint8_t *)out_test_row, row_bytes, ctr_dropout2, 128, (uint8_t *)(dropout_cache2)) != SGX_SUCCESS) {
            printf("\033[35m %s: sgx_aes_ctr_encrypt failed - %d -\033[0m\n", __FUNCTION__, i);
            while (1)
                ;        
        }
 
        for (int j = 0; j < sampleSize; j++) {
            bool tmp_val = ( (dropout_cache2[j>>3] >> (j&0x07)) & 0x1) ? true : false;
            if (tmp_val != dropoutCandidates(i, j)) {
                printf("\033[33m not equal in (%d, %d): %d  %d \033[0m\n", i, j, dropout_cache2[j], dropoutCandidates(i, j));
                while (1)
                    ;            
            }
        }

        printf("\033[34m row  %d done... \033[0m\n", i);
    }
#endif
}


void scData::scDissim(bool correction, int threads, bool useStepFunction) {

    ocall_aligned_malloc((uint64_t *)&dissim_outside, sizeof(matrix_real) * \
            sampleSize * sampleSize);
    
    if (enclave_cache_size < sizeof(matrix_real) * sampleSize) {
        printf("\033[33m %s: encalve_cache_size < the size of one col of dissim matrix \033[0m\n", __FUNCTION__);
        while (1)
            ;        
    }
    uint8_t ctr_dissim[16] = {0};
    // cache should be 32 bytes aligned because of SIMD
    
    //suppose enclave_cache is larger than dissim_cache_size
    size_t dissim_cache_size_per_thread = ( (sampleSize * sizeof(matrix_real) + 31) & ~31 );
    size_t dissim_cache_size = dissim_cache_size_per_thread * THREAD_NUM;
    matrix_real *dissim_cache[THREAD_NUM];
    // printf("dissim_cache_size_per_thread: %d\n", dissim_cache_size_per_thread);
    // memset(enclave_cache, 0, dissim_cache_size);
    for (int k1 = 0; k1 < THREAD_NUM; k1++) {
        dissim_cache[k1] = (matrix_real *)((uint64_t)enclave_cache + dissim_cache_size_per_thread * k1);
        // printf("dissim_cache[%d]: %p\n", k1, dissim_cache[k1]);
    }
    size_t tags_cache_size_per_row = ( (sampleSize * sizeof(matrix_real) + 31) & ~31 );
    size_t dropout_cache_size_per_row =  ( (sampleSize * sizeof(bool) + 31) & ~31 );
    const int ROW = (enclave_cache_size - dissim_cache_size) / \
                (tags_cache_size_per_row + dropout_cache_size_per_row);

    matrix_real *tags_data = (matrix_real *)((uint64_t)enclave_cache + dissim_cache_size);
    bool *dropout_data = (bool *)((uint64_t)enclave_cache + dissim_cache_size + tags_cache_size_per_row * ROW);    

    printf("%s: ROW = %d\n", __FUNCTION__, ROW);
    // printf("tags_data: %p   dropout_data: %p\n", tags_data, dropout_data);

    matrix_real *tags_st = (matrix_real*)tags_row;
    bool *dropout_st = (bool*)dropout_row;
    uint8_t ctr_tag[16] = {0};
    uint8_t ctr_dropout[16] = {0};
    for(int i=0;i<featureSize;i+=ROW) { 
        const int ed = std::min(featureSize, i+ROW);
        matrix_real *tags_data_st = tags_data;
        bool *dropout_data_st = dropout_data;
        for(int j=i;j<ed;j++, tags_st+=sampleSize, dropout_st+=sampleSize, \
                        tags_data_st+=sampleSize, dropout_data_st+=sampleSize) {
           if (sgx_aes_ctr_decrypt(&key_tags_rowmajor, (uint8_t *)tags_st, \
                sampleSize * sizeof(matrix_real), ctr_tag, 128, \
                (uint8_t *)(tags_data_st)) != SGX_SUCCESS) {
                printf("\033[33m %s: sgx_aes_ctr_encrypt failed - \
                        %d -\033[0m\n", __FUNCTION__, i);
                while (1);        
            }
            if (sgx_aes_ctr_decrypt(&key_dropout_rowmajor, \
                (uint8_t *)dropout_st, \
                sampleSize * sizeof(bool), ctr_dropout, 128, \
                (uint8_t *)(dropout_data_st)) != SGX_SUCCESS) {
                printf("\033[33m %s: sgx_aes_ctr_encrypt failed - \
                        %d -\033[0m\n", __FUNCTION__, i);
                while (1);        
            }
        }
        printf("Starting row = %d, Processing row = %d\n", i, ed-i);
        memcpy(&key_dissim_1, &key_dissim_2, 16);
        int ret;
        if ((ret = rdrand_get_n_uints(2, (uint64_t*)&key_dissim_2))!=2) {
            printf("Key generation failed on %d\n", ret);
            break;
        }
        calcDissim(useStepFunction, dissim_outside, tags_data, dropout_data, dissim_cache, \
            std::min(ROW, featureSize-i), sampleSize, wThresholdVal, \
            pDropoutCoefA, pDropoutCoefB, i==0);
// ocall_gettime(1);
    }

    // load the whole dissim matrix into enclave
    dissim_data = (matrix_real*)Eigen::internal::aligned_malloc( \
                sizeof(matrix_real) * sampleSize * sampleSize);     
    memset(ctr_dissim, 0, sizeof(ctr_dissim));

    matrix_real* diss_col = dissim_data;
    matrix_real* diss_col_out = dissim_outside;
    // diss_col = dissim_data;
    // diss_col_out = dissim_outside;
    for(int i=0;i<sampleSize;i++, diss_col+=sampleSize, diss_col_out+=sampleSize) {
        if (sgx_aes_ctr_decrypt(&key_dissim_2, (uint8_t *)diss_col_out, \
                sampleSize * sizeof(matrix_real), ctr_dissim, 128, \
                (uint8_t *)diss_col) != SGX_SUCCESS) {
            printf("\033[33m scData: sgx_aes_ctr_decrypt failed \033[0m\n");
        }
    }
    new (&dissim) MatrixMap<matrix_real>(dissim_data, sampleSize, sampleSize);

    Eigen::internal::aligned_free(enclave_cache);
}



void scData::scPCoA() {
    Matrix<matrix_real> ret = centre(dissim.selfadjointView<Eigen::Upper>(), \
            sampleSize);

#if 0
    Eigen::SelfAdjointEigenSolver<Matrix<matrix_real> > sol(ret, false);
#else
    Matrix<float> ret2 = ret.cast<float>();
    Eigen::SelfAdjointEigenSolver<Matrix<float> > sol(ret2, false);
#endif
 
    // Eigen::SelfAdjointEigenSolver<Matrix<matrix_real> > sol(ret);
    const auto &eigenvalues = sol.eigenvalues();
    // const auto &eigenvectors = sol.eigenvectors();
printf("Max eigen: %f, Min eigen: %f\n", eigenvalues.maxCoeff(), eigenvalues.minCoeff());
    int n = eigenvalues.size(), k = 0;
    matrix_real sum = 0;
    variation.resize(n);
    for(int i=0;i<n;i++) 
        if(eigenvalues(i)>epsilon)
            sum += (variation[k++] = eigenvalues(i));
    variation.resize(k);
    int *index = new int[k];
    std::iota(index, index + k, 0);
    std::vector<matrix_real> &tmp = variation;
    std::sort(index, index+k, [&tmp](int i, int j){return tmp[i]>tmp[j];});
    // PC.resize(k, n); 
    // int k_ = 0;
    // for(int i=0;i<n;i++) 
    //     if(eigenvalues(i)>epsilon)
    //         PC.row(k_++) = eigenvectors.col(i);
    apply_permutation(index, variation);
    delete[] index;
    // seems unnecessary
    // const std::vector<matrix_real> var_copy(variation);
    // for(std::vector<matrix_real>::iterator i=variation.begin(); \
    //                 i!=variation.end(); i++) *i /= sum;
    nPC();
printf("# PC: %d\n", nPCVal);
ocall_gettime(1);
    calcEigenvectors(PC, variation, ret, nPCVal);
}


void scData::nPC(int nPC_, real cutoff_divisor) {
    if(nPC_>0 && nPC_<= (int)variation.size()) {
        nPCVal=nPC_;
        return;
    }
    const int NPC_DEFAULT = 4;
    int l = variation.size();
    l = (double)l * 0.9;


    if(l==0) l = variation.size();
    matrix_real max_d_1 = 0, max_d_2 = 0, max_d_3 = 0, mean = 0;
    for(int i=0;i<l-1;i++) {
        matrix_real d = variation[i] - variation[i+1];
        mean += d;

        if (d > max_d_1) {
            max_d_3 = max_d_2;
            max_d_2 = max_d_1;
            max_d_1 = d;
        } else if (d > max_d_2) {
            max_d_3 = max_d_2;
            max_d_2 = d;
        } else if (d > max_d_3) {
            max_d_3 = d;
        }

    }
    mean /= (l - 1);
    matrix_real spread = 300.0*mean/(max_d_1+max_d_2+max_d_3);

    if(spread > 15.0) {
        nPCVal = NPC_DEFAULT;
        return;
    } else if(spread > 10.0) cutoff_divisor = 5;
    real cutoff = max_d_1 / cutoff_divisor;
    std::vector<int> cur_group, pre_group;
    int group_index = 0; cur_group.push_back(0);
    int len_cur_group = 1;

    for(int i=1;i<l;i++) {
        if(variation[i-1] - variation[i] < cutoff) {
            cur_group.push_back(i); ++len_cur_group;
            if(len_cur_group > 7 || (i>=10 && len_cur_group >3)) {
                if(group_index==0){
                    nPCVal = NPC_DEFAULT;
                    return;
                }

                nPCVal = *pre_group.rbegin() + 1;
                if(nPCVal == 1) nPCVal = NPC_DEFAULT;
                return;
            }
        }else {
            group_index++; len_cur_group = 1;
            pre_group = cur_group; 
            cur_group[0] = i; cur_group.resize(1);
        }
    }
    nPCVal = NPC_DEFAULT;
}

void scData::scCluster(int n, Method cMethod) {
    variation.resize(nPCVal);
    PC.conservativeResize(nPCVal, sampleSize);
    if(n<=1) n = nPCVal * 3 + 3;
    int cur_cluster = sampleSize;
    HierarchicalClustering cluster(PC, nPCVal, cMethod);
    std::vector<std::vector<uint16_t > > records(n);
    std::vector<real> indexes(n-1);
    types.resize(sampleSize);
    while(cur_cluster>2) {
        cluster.merge();
        cur_cluster--;
        if(cur_cluster<=n) {
            records[cur_cluster - 2] = cluster.getClusters();
            indexes[cur_cluster - 2] = cluster.getIndex();
        }
    }
    std::vector<real> a(n-3);
    for(int i=0;i<n-3;i++) a[i] = indexes[i] + indexes[i+2] - \
                                2 * indexes[i+1]; 
    real min_b = a[0];
    int b = 0;
    for(int i=1;i<n-3;i++)
        if(min_b>a[i]) {
            b = i;
            min_b = a[i];
        }
    real min_c = a[b+1];
    int c = b + 1;
    for(int i=b+2;i<n-3;i++) 
        if(min_c>a[i]) {
            c = i;
            min_c = a[i];
        }
    if(3*a[c]<a[b])
        nCluster = c + 3;
    else nCluster = b + 3;
    types = records[nCluster - 2];
}

#if 1
void scData::toFile(const char *filename) const {
    printf("Writing encrypted results to the file %s...\n", filename);
    uint8_t ctr[16] = {0};
    uint16_t *output = new uint16_t[sampleSize];
    if (sgx_aes_ctr_encrypt(&key_file, (uint8_t *)types.data(), \
            sampleSize * sizeof(uint16_t), ctr, 128, \
            (uint8_t *)output) != SGX_SUCCESS) {
        printf("\033[33m %s: sgx_aes_ctr_encrypt failed \033[0m\n", \
            __FUNCTION__);
    }
    ocall_toFile(filename, output, sampleSize);
    delete[] output;
}
#else
void scData::toFile(const char *filename) const {
    ocall_toFile_plain(filename, (uint16_t *)types.data(), types.size());
}
#endif

