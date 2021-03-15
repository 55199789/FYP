#include "Enclave.h"
#include "utils.h"

#include <sgx_tcrypto.h>

sgx_aes_ctr_128bit_key_t key_tags = {0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00};
sgx_aes_ctr_128bit_key_t key_tags_rowmajor = {0x00, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00};
sgx_aes_ctr_128bit_key_t key_dropout = {0x0E, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00};
sgx_aes_ctr_128bit_key_t key_dropout_rowmajor = {0x0D, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00};
sgx_aes_ctr_128bit_key_t key_dropout_rowmajor2 = {0x0A, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00};
sgx_aes_ctr_128bit_key_t key_dissim_1 = {0x0E, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00};
sgx_aes_ctr_128bit_key_t key_dissim_2 = {0x0E, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00};
sgx_aes_ctr_128bit_key_t key_file = {0x0E, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00};


int rdrand64_step(uint64_t *rand) {
	unsigned char ok;
	asm volatile ("rdrand %0; setc %1"
		: "=r" (*rand), "=qm" (ok));
	return (int) ok;
}

unsigned int rdrand_get_n_uints(unsigned int n, uint64_t *dest) {
    unsigned int i;
	uint64_t *lptr= (uint64_t *) dest;
	for (i= 0; i< n; ++i, ++dest) {
		if ( ! rdrand64_step(dest) ) {
			return i;
		}
	}
	return n;
}

void ctr128_inc_for_bytes(unsigned char *ctr, uint64_t len) {

    uint64_t inc_num = (len / 16) + (len % 16 ? 1 : 0);

    // printf("ctr128_inc_for_bytes: byte_len - %lx   inc_num - %lx\n", len, inc_num);
    unsigned char rev_ctr[16];
    for (int i = 0; i < 16; i++) {
        rev_ctr[i] = ctr[15-i];
    }

    uint64_t *lo = (uint64_t *)(rev_ctr);
    uint64_t *hi = (uint64_t *)(rev_ctr + 8);

    uint64_t tmp_lo = *lo;
    *lo = *lo + inc_num;
    if (*lo < tmp_lo)
        *hi = *hi + 1;
    
    for (int i = 0; i < 16; i++) {
        ctr[i] = rev_ctr[15-i];
    }
}


void ctr128_inc_for_count(unsigned char *ctr, uint64_t count) {

    uint64_t inc_num = count;

    unsigned char rev_ctr[16];
    for (int i = 0; i < 16; i++) {
        rev_ctr[i] = ctr[15-i];
    }

    uint64_t *lo = (uint64_t *)(rev_ctr);
    uint64_t *hi = (uint64_t *)(rev_ctr + 8);

    uint64_t tmp_lo = *lo;
    *lo = *lo + inc_num;
    if (*lo < tmp_lo)
        *hi = *hi + 1;
    
    for (int i = 0; i < 16; i++) {
        ctr[i] = rev_ctr[15-i];
    }
}


void plus_log_2(matrix_real *obj, const int size, const int priorTPM) {
    for(int i=0;i<size;i++) 
        obj[i] = std::log(obj[i] * Mi + priorTPM)/log_2;
}

real nzmean(const Vector<matrix_real> &row1, const Vector<bool> &row2) {
    assert(row1.size() == row2.size());
    int cnt = 0;
    real sum = 0.0;
    for(int i=0;i<row1.size(); i++) 
        if(!row2[i] && row1[i]>0) cnt++, sum += row1[i];
    return sum/cnt;
}

Matrix<matrix_real> centre(const Matrix<matrix_real> &D, int n) {
    static Matrix<matrix_real> I = Matrix<matrix_real>::Identity(n, n);
    static Matrix<matrix_real> J = Matrix<matrix_real>::Constant(n, n, matrix_real(1.0)/n);
    Matrix<matrix_real> A = I - J;
    return A*D*A;
}

void cailliez(Matrix<matrix_real> &D_, int n) {
    Matrix<matrix_real> D = D_.array()*D_.array();
    Matrix<matrix_real> W_d = centre(D, n)*(-0.5);
    Matrix<matrix_real> W_d_ = centre(D_, n)*(-0.5);
    Matrix<matrix_real> B(2*n, 2*n); B.setZero();
    B.block(0, n, n, n) = 2*W_d;
    B.block(n, 0, n, n) = -Matrix<matrix_real>::Identity(n, n);
    B.block(n, n, n, n) = -4 * W_d_;
    Eigen::EigenSolver<Matrix<matrix_real> > b(B, false);
    matrix_real c = b.eigenvalues().real().maxCoeff();
    D_.array() += c;
    for(int i=0;i<n;i++)D_(i, i) = 0;
}

void apply_permutation(const int *perm, std::vector<matrix_real>& v, \
                    Matrix<matrix_real> & vs) {
    int n = v.size();
    std::vector<bool> done(n);
    for(int i = 0; i < n; ++i) {
        if(done[i]) continue;
        done[i] = true;
        int prev = i;
        int curr = perm[i];
        while(i != curr) {
            std::swap(v[prev], v[curr]);
            vs.row(prev).swap(vs.row(curr));
            done[curr] = true;
            prev = curr;
            curr = perm[curr];
        }
    }
}
void apply_permutation(const int *perm, std::vector<matrix_real>& v) {
    int n = v.size();
    std::vector<bool> done(n);
    for(int i = 0; i < n; ++i) {
        if(done[i]) continue;
        done[i] = true;
        int prev = i;
        int curr = perm[i];
        while(i != curr) {
            std::swap(v[prev], v[curr]);
            done[curr] = true;
            prev = curr;
            curr = perm[curr];
        }
    }
}

bool TIMING = false;


// optimize sqrt with AVX2

// __m256d _mm256_sqrt_pd(__m256 a)
// ptr should be 32 bytes aligned
void sqrt_double(double *in_ptr, double *out_ptr, size_t count)
{
    const size_t vec_count = 0x4;
    size_t simd_count = count & (~(vec_count - 1));

    __m256d vec;

    for (size_t i = 0; i < simd_count; i += vec_count)
    {
        vec = _mm256_load_pd(in_ptr + i);
        vec = _mm256_sqrt_pd(vec);
        _mm256_store_pd(out_ptr + i, vec);
    }

    // this for loop can be optimized furthers
    for (size_t i = simd_count; i < count; i++)
        *(out_ptr + i) = sqrt(*(in_ptr + i));

    return;
}

// __m256 _mm256_sqrt_ps(__m256 a)
// ptr should be 32 bytes aligned
// Note: not tested
void sqrt_float(float *in_ptr, float *out_ptr, size_t count)
{
    const size_t vec_count = 0x8;
    size_t simd_count = count & (~(vec_count - 1));

    __m256 vec;

    for (size_t i = 0; i < simd_count; i += vec_count)
    {
        vec = _mm256_load_ps(in_ptr + i);
        vec = _mm256_sqrt_ps(vec);
        _mm256_store_ps(out_ptr + i, vec);
    } 

    // this for loop can be optimized furthers
    for (size_t i = simd_count; i < count; i++)
        *(out_ptr + i) = sqrt(*(in_ptr + i));
    
    return;
}

size_t matrix_majorchange_outside_with_del(matrix_real *tag_in, matrix_real *tag_out, bool *dropout_in, bool *dropout_out, size_t rows, size_t cols, matrix_real wThreshold,\
                                            sgx_aes_ctr_128bit_key_t* key_tag_in, sgx_aes_ctr_128bit_key_t* key_tag_out, sgx_aes_ctr_128bit_key_t* key_dropout_in, sgx_aes_ctr_128bit_key_t* key_dropout_out,
                                            unsigned char *tag_base_ctr, unsigned char *dropout_base_ctr, void *cache, size_t cache_size, bool disable_del) {

    size_t work_rows = cols;
    size_t work_cols = rows;

    printf("work_rows: %d  work_cols: %d\n", work_rows, work_cols);

    size_t data_per_block = 16 / sizeof(matrix_real);
    size_t dropout_col_size = work_rows * sizeof(bool);

    int thread_num = 1;
    size_t cache_size_needed = (work_rows * 16 + dropout_col_size) * thread_num;
    if (cache_size < cache_size_needed) {
        matrix_majorchange_outside<matrix_real>(tag_in, tag_out, rows, cols, key_tag_in, key_tag_out, tag_base_ctr, true);
        return rows;
    }

    // single thread
    size_t col_count = UPPER_ALIGN(work_cols * sizeof(matrix_real), 16) / 16;
    size_t row_count = UPPER_ALIGN(work_rows * sizeof(matrix_real), 16) / 16;
    size_t iter_col_count = (work_cols * sizeof(matrix_real)) % 16 ? col_count - 1 : col_count;
    size_t iter_row_count = (work_rows * sizeof(matrix_real)) % 16 ? row_count - 1 : row_count;

    size_t dropout_col_count = UPPER_ALIGN(work_cols * sizeof(bool), 16) / 16;
    size_t dropout_row_count = UPPER_ALIGN(work_rows * sizeof(bool), 16) / 16;

    matrix_real *in_cache = new matrix_real[data_per_block * data_per_block];
    matrix_real *out_cache[data_per_block];
    for (int i = 0; i < data_per_block; i++) {
            out_cache[i] = (matrix_real*)((uint64_t)cache) + work_rows * i;
    }
    bool *dropout_cache = (bool*)( (uint64_t)cache + work_rows * 16 ); 

    size_t cur_out_col = 0;
    for (size_t j = 0; j < col_count; j++) {
        for (size_t i = 0; i < row_count; i++) {                    
            // row iter
            int iter_rows = i < iter_row_count ? data_per_block : (work_rows - iter_row_count * data_per_block);
            int decrypt_cols = j < iter_col_count ? data_per_block : (work_cols - iter_col_count * data_per_block);

            for (int k = 0; k < iter_rows; k++) {
                unsigned char ctr_in[16] = {0};
                if (tag_base_ctr != NULL)
                    memcpy(ctr_in, tag_base_ctr, 16);

                uint64_t in_offset = ((i*data_per_block+k) * work_cols) + j*data_per_block;
                matrix_real *in_data = tag_in + in_offset;

                uint64_t ctr_in_inc = (i*data_per_block+k) *col_count + j; 
                ctr128_inc_for_count(ctr_in,  ctr_in_inc);

                if (sgx_aes_ctr_decrypt(key_tag_in, (uint8_t *)in_data, decrypt_cols * sizeof(matrix_real), ctr_in, 128, (uint8_t *)(in_cache + k * data_per_block)) != SGX_SUCCESS) {
                    printf("\033[33m %s: sgx_aes_ctr_encrypt failed \033[0m\n", __FUNCTION__);
                }
            }

            for (int k1 = 0; k1 < iter_rows; k1++) {
                for (int k2 = 0; k2 < decrypt_cols; k2++) {
                    out_cache[k2][i*data_per_block+k1] = in_cache[k1*data_per_block+k2];
                }
            }
        }


        int iter_cols = j < iter_col_count ? data_per_block : (work_cols - iter_col_count * data_per_block);
        for (int k = 0; k < iter_cols; k++) {
            size_t cur_in_col = j * data_per_block + k;

            // dropout in            
            size_t ctr_dropout_in_offset = cur_in_col * dropout_col_size;
            bool *dropout_in_data = (bool *)((unsigned char *)dropout_in + ctr_dropout_in_offset);
            
            unsigned char ctr_dropout_in[16] = {0};
            if (dropout_base_ctr != NULL)
                memcpy(ctr_dropout_in, dropout_base_ctr, 16);
            size_t ctr_dropout_in_inc = cur_in_col * dropout_row_count;
            ctr128_inc_for_count(ctr_dropout_in, ctr_dropout_in_inc);

            if (sgx_aes_ctr_decrypt(key_dropout_in, (uint8_t *)dropout_in_data, dropout_col_size, ctr_dropout_in, 128, (uint8_t *)(dropout_cache)) != SGX_SUCCESS) {
                printf("\033[33m %s: sgx_aes_ctr_encrypt failed \033[0m\n", __FUNCTION__);
            }
    

            bool is_del = true;
            for (int k1 = 0; k1 < work_rows; k1++) {
                if (out_cache[k][k1] > wThreshold || !dropout_cache[k1]) {
                    is_del = false;
                    break;
                }
            }
            if(is_del & !disable_del)      continue;

            //dropout out
            uint64_t ctr_dropout_out_offset = cur_out_col * dropout_col_size;
            bool *dropout_out_data = (bool *)((unsigned char *)dropout_out + ctr_dropout_out_offset);


            unsigned char ctr_dropout_out[16] = {0};
            if (dropout_base_ctr != NULL)
                memcpy(ctr_dropout_out, dropout_base_ctr, 16);
            size_t ctr_dropout_out_inc = cur_out_col * dropout_row_count;
            ctr128_inc_for_count(ctr_dropout_out, ctr_dropout_out_inc);

            // static bool testing = true;
            // if (testing) {
            //     printf("addr: %p  %p  %d\n", dropout_out_data, key_dropout_out, dropout_col_size);
            //     printf("[%d] ctr_dropout_out: ", cur_out_col);
            //     for (int i = 0; i < 16; i++)
            //         printf("%d  ", ctr_dropout_out[i]);
            //     printf("}}}\n");
                
            //     testing = false;
            // }

            if (sgx_aes_ctr_encrypt(key_dropout_out, (uint8_t *)(dropout_cache), dropout_col_size, ctr_dropout_out, 128, (uint8_t *)dropout_out_data) != SGX_SUCCESS) {
                printf("\033[33m %s: sgx_aes_ctr_encrypt failed \033[0m\n", __FUNCTION__);
            }

            // static bool testing2 = true;
            // if (testing2) {
            //     for (int i = 0; i < 10; i++)
            //         printf("%x ", dropout_out_data[i]);
            //     printf("===\n");
            //     testing2 = false;
            // }

            
            // tag out
            unsigned char ctr_tag_out[16] = {0};
            if (tag_base_ctr != NULL)
                memcpy(ctr_tag_out, tag_base_ctr, 16);
            uint64_t tag_out_offset = cur_out_col * work_rows;
            matrix_real *tag_out_data = tag_out + tag_out_offset;

            uint64_t ctr_tag_out_inc = cur_out_col *row_count;
            ctr128_inc_for_count(ctr_tag_out,  ctr_tag_out_inc);
                        
            if (sgx_aes_ctr_encrypt(key_tag_out, (uint8_t *)(out_cache[k]), work_rows * sizeof(matrix_real), ctr_tag_out, 128, (uint8_t *)tag_out_data) != SGX_SUCCESS) {
                printf("\033[33m %s: sgx_aes_ctr_encrypt failed \033[0m\n", __FUNCTION__);
            }

            // printf("[%d] ctr_tag_out: ", cur_out_col);
            // for (int i = 0; i < 16; i++)
            //     printf("%d  ", ctr_tag_out[i]);
            // printf("}}}\n");

            cur_out_col++;                
        }        
    }

    return cur_out_col;
}



size_t matrix_majorchange_outside_with_del_with_1bit(matrix_real *tag_in, matrix_real *tag_out, bool *dropout_in, uint8_t *dropout_out, size_t rows, size_t cols, matrix_real wThreshold,\
                                            sgx_aes_ctr_128bit_key_t* key_tag_in, sgx_aes_ctr_128bit_key_t* key_tag_out, sgx_aes_ctr_128bit_key_t* key_dropout_in, sgx_aes_ctr_128bit_key_t* key_dropout_out,
                                            unsigned char *tag_base_ctr, unsigned char *dropout_base_ctr, void *cache, size_t cache_size, bool disable_del) {

    size_t work_rows = cols;
    size_t work_cols = rows;

    printf("work_rows: %d  work_cols: %d\n", work_rows, work_cols);

    size_t data_per_block = 16 / sizeof(matrix_real);
    size_t dropout_col_size = work_rows * sizeof(bool);

    int thread_num = 1;
    size_t cache_size_needed = (work_rows * 16 + dropout_col_size * 2) * thread_num;
    if (cache_size < cache_size_needed) {
        matrix_majorchange_outside<matrix_real>(tag_in, tag_out, rows, cols, key_tag_in, key_tag_out, tag_base_ctr, true);
        return rows;
    }

    // single thread
    size_t col_count = UPPER_ALIGN(work_cols * sizeof(matrix_real), 16) / 16;
    size_t row_count = UPPER_ALIGN(work_rows * sizeof(matrix_real), 16) / 16;
    size_t iter_col_count = (work_cols * sizeof(matrix_real)) % 16 ? col_count - 1 : col_count;
    size_t iter_row_count = (work_rows * sizeof(matrix_real)) % 16 ? row_count - 1 : row_count;

    size_t dropout_col_count = UPPER_ALIGN(work_cols * sizeof(bool), 16) / 16;
    size_t dropout_row_count = UPPER_ALIGN(work_rows * sizeof(bool), 16) / 16;

    matrix_real *in_cache = new matrix_real[data_per_block * data_per_block];
    matrix_real *out_cache[data_per_block];
    for (int i = 0; i < data_per_block; i++) {
            out_cache[i] = (matrix_real*)((uint64_t)cache) + work_rows * i;
    }
    bool *dropout_cache = (bool*)( (uint64_t)cache + work_rows * 16 ); 

    size_t cur_out_col = 0;
    for (size_t j = 0; j < col_count; j++) {
        for (size_t i = 0; i < row_count; i++) {                    
            // row iter
            int iter_rows = i < iter_row_count ? data_per_block : (work_rows - iter_row_count * data_per_block);
            int decrypt_cols = j < iter_col_count ? data_per_block : (work_cols - iter_col_count * data_per_block);

            for (int k = 0; k < iter_rows; k++) {
                unsigned char ctr_in[16] = {0};
                if (tag_base_ctr != NULL)
                    memcpy(ctr_in, tag_base_ctr, 16);

                uint64_t in_offset = ((i*data_per_block+k) * work_cols) + j*data_per_block;
                matrix_real *in_data = tag_in + in_offset;

                uint64_t ctr_in_inc = (i*data_per_block+k) *col_count + j; 
                ctr128_inc_for_count(ctr_in,  ctr_in_inc);

                if (sgx_aes_ctr_decrypt(key_tag_in, (uint8_t *)in_data, decrypt_cols * sizeof(matrix_real), ctr_in, 128, (uint8_t *)(in_cache + k * data_per_block)) != SGX_SUCCESS) {
                    printf("\033[33m %s: sgx_aes_ctr_encrypt failed \033[0m\n", __FUNCTION__);
                }
            }

            for (int k1 = 0; k1 < iter_rows; k1++) {
                for (int k2 = 0; k2 < decrypt_cols; k2++) {
                    out_cache[k2][i*data_per_block+k1] = in_cache[k1*data_per_block+k2];
                }
            }
        }


        int iter_cols = j < iter_col_count ? data_per_block : (work_cols - iter_col_count * data_per_block);
        for (int k = 0; k < iter_cols; k++) {
            size_t cur_in_col = j * data_per_block + k;

            // dropout in            
            size_t ctr_dropout_in_offset = cur_in_col * dropout_col_size;
            bool *dropout_in_data = (bool *)((unsigned char *)dropout_in + ctr_dropout_in_offset);
            
            unsigned char ctr_dropout_in[16] = {0};
            if (dropout_base_ctr != NULL)
                memcpy(ctr_dropout_in, dropout_base_ctr, 16);
            size_t ctr_dropout_in_inc = cur_in_col * dropout_row_count;
            ctr128_inc_for_count(ctr_dropout_in, ctr_dropout_in_inc);

            if (sgx_aes_ctr_decrypt(key_dropout_in, (uint8_t *)dropout_in_data, dropout_col_size, ctr_dropout_in, 128, (uint8_t *)(dropout_cache)) != SGX_SUCCESS) {
                printf("\033[33m %s: sgx_aes_ctr_encrypt failed \033[0m\n", __FUNCTION__);
            }
    

            bool is_del = true;
            for (int k1 = 0; k1 < work_rows; k1++) {
                if (out_cache[k][k1] > wThreshold || !dropout_cache[k1]) {
                    is_del = false;
                    break;
                }
            }
            if(is_del && !disable_del)      continue;

            //dropout out
            uint8_t *dropout_out_cache = (uint8_t*)( (uint64_t)cache + work_rows * 16 + dropout_col_size ); 
            for (int k1 = 0; k1 < work_rows; k1++)
                dropout_out_cache[k1] = 0;
            // to 1-bit
            for (int k1 = 0; k1 < work_rows; k1++) {
                // uint8_t tmp_dropout = dropout_cache[k1];
                // uint8_t *tmp_addr = (uint8_t *)&dropout_cache[k1>>3];
                // if (tmp_dropout) {
                //     (*tmp_addr) |= tmp_dropout << (k1 & 0x7);
                // }
                if (dropout_cache[k1]) {
                    dropout_out_cache[k1>>3] |= 1 << (k1 & 0x07);
                }
            }

            // size_t dropout_out_col_size = (work_rows + 0x7) & ~(0x7);
            size_t dropout_out_col_size = (work_rows + 0x7) >> 3;
            uint64_t ctr_dropout_out_offset = cur_out_col * dropout_out_col_size;
            bool *dropout_out_data = (bool *)((unsigned char *)dropout_out + ctr_dropout_out_offset);


            unsigned char ctr_dropout_out[16] = {0};
            if (dropout_base_ctr != NULL)
                memcpy(ctr_dropout_out, dropout_base_ctr, 16);
            size_t dropout_out_row_count = UPPER_ALIGN(dropout_out_col_size, 16) / 16;
            size_t ctr_dropout_out_inc = cur_out_col * dropout_out_row_count;
            ctr128_inc_for_count(ctr_dropout_out, ctr_dropout_out_inc);

            // static bool testing = true;
            // if (testing) {
            //     printf("addr: %p  %p  %d\n", dropout_out_data, key_dropout_out, dropout_col_size);
            //     printf("[%d] ctr_dropout_out: ", cur_out_col);
            //     for (int i = 0; i < 16; i++)
            //         printf("%d  ", ctr_dropout_out[i]);
            //     printf("}}}\n");
                
            //     testing = false;
            // }

            if (sgx_aes_ctr_encrypt(key_dropout_out, (uint8_t *)(dropout_out_cache), dropout_out_col_size, ctr_dropout_out, 128, (uint8_t *)dropout_out_data) != SGX_SUCCESS) {
                printf("\033[33m %s: sgx_aes_ctr_encrypt failed \033[0m\n", __FUNCTION__);
            }

            // static bool testing2 = true;
            // if (testing2) {
            //     for (int i = 0; i < 10; i++)
            //         printf("%x ", dropout_out_data[i]);
            //     printf("===\n");
            //     testing2 = false;
            // }

            
            // tag out
            unsigned char ctr_tag_out[16] = {0};
            if (tag_base_ctr != NULL)
                memcpy(ctr_tag_out, tag_base_ctr, 16);
            uint64_t tag_out_offset = cur_out_col * work_rows;
            matrix_real *tag_out_data = tag_out + tag_out_offset;

            uint64_t ctr_tag_out_inc = cur_out_col *row_count;
            ctr128_inc_for_count(ctr_tag_out,  ctr_tag_out_inc);
                        
            if (sgx_aes_ctr_encrypt(key_tag_out, (uint8_t *)(out_cache[k]), work_rows * sizeof(matrix_real), ctr_tag_out, 128, (uint8_t *)tag_out_data) != SGX_SUCCESS) {
                printf("\033[33m %s: sgx_aes_ctr_encrypt failed \033[0m\n", __FUNCTION__);
            }

            // printf("[%d] ctr_tag_out: ", cur_out_col);
            // for (int i = 0; i < 16; i++)
            //     printf("%d  ", ctr_tag_out[i]);
            // printf("}}}\n");

            cur_out_col++;                
        }        
    }

    return cur_out_col;
}
