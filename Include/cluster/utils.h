#ifndef UTILS_HPP
#define UTILS_HPP

#include <math.h>
#include <chrono>
#include <vector>
#include <Eigen/Core>
#include <Eigen/Eigenvalues>

#include "immintrin.h"

#include <sgx_tcrypto.h>

#include "threads_conf.h"

extern sgx_aes_ctr_128bit_key_t key_tags;
extern sgx_aes_ctr_128bit_key_t key_tags_rowmajor;
extern sgx_aes_ctr_128bit_key_t key_dropout;
extern sgx_aes_ctr_128bit_key_t key_dropout_rowmajor;
extern sgx_aes_ctr_128bit_key_t key_dropout_rowmajor2;
extern sgx_aes_ctr_128bit_key_t key_dissim_1;
extern sgx_aes_ctr_128bit_key_t key_dissim_2;
extern sgx_aes_ctr_128bit_key_t key_file;

typedef double real;
typedef double matrix_real;

// #define  ALIGN_BYTES  32
// #define  ALIGN_MALLOC(bytes) malloc((bytes) + ALIGN_BYTES);
// #define  ALIGN_ADDR(addr) (((uint64_t)addr + ALIGN_BYTES) & ~(ALIGN_BYTES - 1))

enum Tag {
    RAW,
    CPM
};

enum Method {
    WARD_D2,
    SINGLE,
    COMPLETE,
    AVERAGE
};

typedef real (*Linkage)(const real, const real, const real, \
                        const int, const int, const int);

constexpr int Mi = 1000000;
const matrix_real log_2 = std::log(2);

template<class T>
using Matrix = typename Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic, Eigen::ColMajor>;

template<class T>
using Vector = typename Eigen::Matrix<T, 1, Eigen::Dynamic>;

template<class T>
using ColVector = typename Eigen::Matrix<matrix_real, Eigen::Dynamic, 1>;

template<class T>
using ColArray = typename Eigen::Array<matrix_real, Eigen::Dynamic, 1>;


template <typename T>
using MatrixMap = typename Eigen::Map<Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic, Eigen::ColMajor>>;

template <typename T>
using VectorMap = typename Eigen::Map<Eigen::Matrix<T, 1, Eigen::Dynamic>>;

template<class T>
inline T f(T a, T b, T x) {
        return T(1)/(T(1)+exp(a*(x-b)));
}

template<class T>
inline matrix_real l2_dist(const T& x, const T& y, int n){
    matrix_real dist = 0;
    for(int i=0;i<n;i++)dist += (x[i]-y[i])*(x[i]-y[i]);
    return dist;
}

template<class T>
void plus_log_2(T *obj, const int priorTPM) {
    obj->array() = (obj->array() * Mi + priorTPM).log()/log_2;
}

template<class T>
matrix_real quantile(T x, const real q) {
    assert(0.0<=q && q<=1.0+1e-10);
    const int n  = x.size();
    const real id = (n - 1) * q;
    const int lo = floor(id);
    const int hi = ceil(id);
    std::nth_element(x.data(), x.data()+lo, x.data() + n);
    const matrix_real qs = x[lo];
    std::nth_element(x.data() ,x.data()+hi, x.data() + n);
    const matrix_real ps = x[hi];
    const real h  = (id - lo);
    return (1.0 - h) * qs + h * ps;
}

template<class T>
matrix_real quantile(T* dat, const int n, const real q) {  
    assert(0.0<=q && q<=1.0+1e-10);
    T *x = new T[n];
    memcpy(x, dat, n * sizeof(T));
    const real id = (n - 1) * q;
    const int lo = floor(id);
    const int hi = ceil(id);
    std::nth_element(x, x + lo, x + n);
    const matrix_real qs = x[lo];
    std::nth_element(x ,x + hi, x + n);
    const matrix_real ps = x[hi];
    const real h  = (id - lo);
    delete[] x;
    return (real(1.0) - h) * qs + h * ps;
}

template<class T>
matrix_real mean(const T &x) {
    matrix_real ret = 0.0;
    const int n = x.size();
    for(int i=0;i<n;i++) ret += x[i];
    return ret/x.size();
}
void plus_log_2(matrix_real *obj, const int size, const int priorTPM);

// Non-zero mean
real nzmean(const Vector<matrix_real> &row1, const Vector<bool> &row2);

Matrix<matrix_real> centre(const Matrix<matrix_real> &D, int n);

void cailliez(Matrix<matrix_real> &D_, int n);

void apply_permutation(const int *perm, std::vector<matrix_real>& v, \
                    Matrix<matrix_real> & vs);
void apply_permutation(const int *perm, std::vector<matrix_real>& v);
inline real single(const real c_i_j, const real c_i_k, const real c_j_k, \
            const int i, const int j, const int k) {
                return std::min(c_i_k, c_j_k);
            }

inline real single(const real c_i_j, const real c_i_k, const real c_j_k, \
            const int i, const int j, const int k);

inline real complete(const real c_i_j, const real c_i_k, const real c_j_k, \
            const int i, const int j, const int k) {
                return std::max(c_i_k, c_j_k);
            }
inline real average(const real c_i_j, const real c_i_k, const real c_j_k, \
            const int i, const int j, const int k) {
                return (c_i_k*i+c_j_k*j)/(i+j);
            }
inline real ward_d2(const real c_i_j, const real c_i_k, const real c_j_k, \
            const int i, const int j, const int k) {
        const int sum = i+j+k;
        return (c_i_k*(i+k)+ c_j_k*(j+k)-c_i_j*k)/(real)sum;
    }

inline void ctr128_inc(unsigned char *counter)
{
	unsigned int n = 16, c = 1;

	do {
		--n;
		c += counter[n];
		counter[n] = (unsigned char)c;
		c >>= 8;
	} while (n);
}

void ctr128_inc_for_bytes(unsigned char *ctr, size_t len);
void ctr128_inc_for_count(unsigned char *ctr, uint64_t count);

void sqrt_double(double *in_ptr, double *out_ptr, size_t count);
void sqrt_float(float *in_ptr, float *out_ptr, size_t count);

size_t matrix_majorchange_outside_with_del(matrix_real *tag_in, matrix_real *tag_out, bool *dropout_in, bool *dropout_out, size_t rows, size_t cols, matrix_real wThreshold,\
                                            sgx_aes_ctr_128bit_key_t* key_tag_in, sgx_aes_ctr_128bit_key_t* key_tag_out, sgx_aes_ctr_128bit_key_t* key_dropout_in, sgx_aes_ctr_128bit_key_t* key_dropout_out,
                                            unsigned char *tag_base_ctr, unsigned char *dropout_base_ctr, void *cache, size_t cache_size, bool disable_del = false);
                                            
size_t matrix_majorchange_outside_with_del_with_1bit(matrix_real *tag_in, matrix_real *tag_out, bool *dropout_in, uint8_t *dropout_out, size_t rows, size_t cols, matrix_real wThreshold,\
                                            sgx_aes_ctr_128bit_key_t* key_tag_in, sgx_aes_ctr_128bit_key_t* key_tag_out, sgx_aes_ctr_128bit_key_t* key_dropout_in, sgx_aes_ctr_128bit_key_t* key_dropout_out,
                                            unsigned char *tag_base_ctr, unsigned char *dropout_base_ctr, void *cache, size_t cache_size, bool disable_del = false);

// void matrix_majorchange_outside(matrix_real *in, matrix_real *out, size_t rows, size_t cols, \
//                                     sgx_aes_ctr_128bit_key_t* key_in, sgx_aes_ctr_128bit_key_t* key_out, unsigned char *ctr, bool toRowMajor);

#define  UPPER_ALIGN(data, align) ( (data + (align - 1)) & ~(align - 1) )

int rdrand64_step(uint64_t *rand);

unsigned int rdrand_get_n_uints(unsigned int n, uint64_t *dest);


template <typename T>
void matrix_majorchange_outside(T *in, T *out, size_t rows, size_t cols, \
                                    sgx_aes_ctr_128bit_key_t* key_in, \
                                    sgx_aes_ctr_128bit_key_t* key_out, \
                                    unsigned char *base_ctr, bool toRowMajor) {

    size_t work_rows = cols;
    size_t work_cols = rows;
    if (toRowMajor == false) {
        work_rows = rows;
        work_cols = cols;
    }

    // printf("work_rows: %d  work_cols: %d\n", work_rows, work_cols);
    // printf("row_count: %d  col_count: %d  data_per_block: %d\n", row_count, col_count, data_per_block);
    // printf("iter_row_count: %d  iter_col_count: %d\n", iter_row_count, iter_col_count, data_per_block);
    int thread_num = THREAD_NUM;
    for (int tid = 0; tid < thread_num; tid++) {
        fun_ptrs[tid] = [tid, thread_num, in, out, work_rows, work_cols, key_in, key_out, base_ctr]() {

        size_t col_count = UPPER_ALIGN(work_cols * sizeof(T), 16) / 16;
        size_t row_count = UPPER_ALIGN(work_rows * sizeof(T), 16) / 16;
        size_t data_per_block = 16 / sizeof(T);
        T *in_cache = new T[data_per_block * data_per_block];
        T *out_cache = new T[data_per_block * data_per_block];

        size_t iter_col_count = (work_cols * sizeof(T)) % 16 ? col_count - 1 : col_count;
        size_t iter_row_count = (work_rows * sizeof(T)) % 16 ? row_count - 1 : row_count;

            size_t count = col_count / thread_num;
            size_t actual_count = (tid + 1 == thread_num) ? (col_count - count * tid) : count;
            size_t st = count * tid;
            size_t ed = st + actual_count;

            printf("thread %d: %d %d\n", tid, st, ed);

            for (size_t j = st; j < ed; j++) {
                for (size_t i = 0; i < row_count; i++) {        
                    
                    // row iter
                    int iter_rows = i < iter_row_count ? data_per_block : (work_rows - iter_row_count * data_per_block);
                    int decrypt_cols = j < iter_col_count ? data_per_block : (work_cols - iter_col_count * data_per_block);

                    for (int k = 0; k < iter_rows; k++) {

                        unsigned char ctr_in[16] = {0};
                        if (base_ctr != NULL)
                            memcpy(ctr_in, base_ctr, 16);

                        uint64_t in_offset = ((i*data_per_block+k) * work_cols) + j*data_per_block;
                        T *in_data = in + in_offset;

                        uint64_t ctr_in_inc = (i*data_per_block+k) *col_count + j; 
                        ctr128_inc_for_count(ctr_in,  ctr_in_inc);

                        if (sgx_aes_ctr_decrypt(key_in, (uint8_t *)in_data, decrypt_cols * sizeof(T), ctr_in, 128, (uint8_t *)(in_cache + k * data_per_block)) != SGX_SUCCESS) {
                            printf("\033[33m %s: sgx_aes_ctr_encrypt failed \033[0m\n", __FUNCTION__);
                        }
                    }

                    for (int k1 = 0; k1 < iter_rows; k1++) {
                        for (int k2 = 0; k2 < decrypt_cols; k2++) {
                            out_cache[k2 * data_per_block + k1] = in_cache[k1 * data_per_block + k2];
                        }
                    }

                    // col iter
                    int iter_cols = j < iter_col_count ? data_per_block : (work_cols - iter_col_count * data_per_block);
                    int encrypt_rows = i < iter_row_count ? data_per_block : (work_rows - iter_row_count * data_per_block);

                    for (int k = 0; k < iter_cols; k++) {
                        unsigned char ctr_out[16] = {0};
                        if (base_ctr != NULL)
                            memcpy(ctr_out, base_ctr, 16);

                        uint64_t out_offset = ((j*data_per_block+k) * work_rows) + i*data_per_block;
                        T *out_data = out + out_offset;

                        uint64_t ctr_out_inc = (j*data_per_block+k) *row_count + i;
                        ctr128_inc_for_count(ctr_out,  ctr_out_inc);

                        if (sgx_aes_ctr_encrypt(key_out, (uint8_t *)(out_cache + k * data_per_block), encrypt_rows * sizeof(T), ctr_out, 128, (uint8_t *)out_data) != SGX_SUCCESS) {
                            printf("\033[33m %s: sgx_aes_ctr_encrypt failed \033[0m\n", __FUNCTION__);
                        }
                    }
                }
            }
            
            delete[] in_cache;
            delete[] out_cache;

        };
    }
    task_notify(thread_num);
    fun_ptrs[0]();
    task_wait_done(thread_num);
}



// if the whole row satisfy the fp function, the row is deleted
template <typename T>
size_t matrix_majorchange_outside_wrapper(T *in, T *out, size_t rows, size_t cols, \
                                    sgx_aes_ctr_128bit_key_t* key_in, sgx_aes_ctr_128bit_key_t* key_out, unsigned char *base_ctr, bool toRowMajor, void *cache, size_t cache_size) {

    size_t work_rows = cols;
    size_t work_cols = rows;
    if (toRowMajor == false) {
        work_rows = rows;
        work_cols = cols;
    }

    printf("work_rows: %d  work_cols: %d\n", work_rows, work_cols);

    size_t data_per_block = 16 / sizeof(T);
    int thread_num = THREAD_NUM;
    size_t cache_size_needed = work_cols * 16 * thread_num;
    if (cache_size < cache_size_needed) {
        matrix_majorchange_outside<T>(in, out, rows, cols, key_in, key_out, base_ctr, toRowMajor);
        return rows;
    }

#if 1
    // single thread
    size_t col_count = UPPER_ALIGN(work_cols * sizeof(T), 16) / 16;
    size_t row_count = UPPER_ALIGN(work_rows * sizeof(T), 16) / 16;

    size_t iter_col_count = (work_cols * sizeof(T)) % 16 ? col_count - 1 : col_count;
    size_t iter_row_count = (work_rows * sizeof(T)) % 16 ? row_count - 1 : row_count;

    T *in_cache = new T[data_per_block * data_per_block];
    T *out_cache[data_per_block];
    for (int i = 0; i < data_per_block; i++) {
            out_cache[i] = (T*)( (uint64_t)cache) + work_rows * i;
    }

    size_t cur_out_col = 0;
    for (size_t j = 0; j < col_count; j++) {

        for (size_t i = 0; i < row_count; i++) {                    
            // row iter
            int iter_rows = i < iter_row_count ? data_per_block : (work_rows - iter_row_count * data_per_block);
            int decrypt_cols = j < iter_col_count ? data_per_block : (work_cols - iter_col_count * data_per_block);

            for (int k = 0; k < iter_rows; k++) {
                unsigned char ctr_in[16] = {0};
                if (base_ctr != NULL)
                    memcpy(ctr_in, base_ctr, 16);

                uint64_t in_offset = ((i*data_per_block+k) * work_cols) + j*data_per_block;
                T *in_data = in + in_offset;

                uint64_t ctr_in_inc = (i*data_per_block+k) *col_count + j; 
                ctr128_inc_for_count(ctr_in,  ctr_in_inc);

                if (sgx_aes_ctr_decrypt(key_in, (uint8_t *)in_data, decrypt_cols * sizeof(T), ctr_in, 128, (uint8_t *)(in_cache + k * data_per_block)) != SGX_SUCCESS) {
                    printf("\033[33m %s: sgx_aes_ctr_encrypt failed \033[0m\n", __FUNCTION__);
                }
            }

            for (int k1 = 0; k1 < iter_rows; k1++) {
                for (int k2 = 0; k2 < decrypt_cols; k2++) {
                    T tmp_val = in_cache[k1*data_per_block+k2];
                    out_cache[k2][i*data_per_block+k1] = tmp_val;
                }
            }
        }

        int iter_cols = j < iter_col_count ? data_per_block : (work_cols - iter_col_count * data_per_block);
        for (int k = 0; k < iter_cols; k++) {

            unsigned char ctr_out[16] = {0};
            if (base_ctr != NULL)
                memcpy(ctr_out, base_ctr, 16);

                // uint64_t out_offset = ((j*data_per_block+k) * work_rows);
                uint64_t out_offset = (cur_out_col++) * work_rows;
                T *out_data = out + out_offset;

                uint64_t ctr_out_inc = (j*data_per_block+k) *row_count;
                ctr128_inc_for_count(ctr_out,  ctr_out_inc);
                        
                if (sgx_aes_ctr_encrypt(key_out, (uint8_t *)(out_cache[k]), work_rows * sizeof(T), ctr_out, 128, (uint8_t *)out_data) != SGX_SUCCESS) {
                    printf("\033[33m %s: sgx_aes_ctr_encrypt failed \033[0m\n", __FUNCTION__);
                }                
        }        
    }

    return cur_out_col;

#else
    // multiple thread
    for (int tid = 0; tid < thread_num; tid++) {
        fun_ptrs[tid] = [tid, thread_num, in, out, work_rows, work_cols, key_in, key_out, base_ctr, cache]() {

            size_t col_count = UPPER_ALIGN(work_cols * sizeof(T), 16) / 16;
            size_t row_count = UPPER_ALIGN(work_rows * sizeof(T), 16) / 16;
            const size_t data_per_block = 16 / sizeof(T);
            T *in_cache = new T[data_per_block * data_per_block];
            T *out_cache[data_per_block];
            for (int i = 0; i < data_per_block; i++) {
                out_cache[i] = (T*)( (uint64_t)cache + work_rows*16*tid ) + work_rows * i;
            }

            size_t iter_col_count = (work_cols * sizeof(T)) % 16 ? col_count - 1 : col_count;
            size_t iter_row_count = (work_rows * sizeof(T)) % 16 ? row_count - 1 : row_count;

            size_t count = col_count / thread_num;
            size_t actual_count = (tid + 1 == thread_num) ? (col_count - count * tid) : count;
            size_t st = count * tid;
            size_t ed = st + actual_count;

            printf("thread %d: %d %d\n", tid, st, ed);

            for (size_t j = st; j < ed; j++) {
                for (size_t i = 0; i < row_count; i++) {        
                    
                    // row iter
                    int iter_rows = i < iter_row_count ? data_per_block : (work_rows - iter_row_count * data_per_block);
                    int decrypt_cols = j < iter_col_count ? data_per_block : (work_cols - iter_col_count * data_per_block);

                    for (int k = 0; k < iter_rows; k++) {

                        unsigned char ctr_in[16] = {0};
                        if (base_ctr != NULL)
                            memcpy(ctr_in, base_ctr, 16);

                        uint64_t in_offset = ((i*data_per_block+k) * work_cols) + j*data_per_block;
                        T *in_data = in + in_offset;

                        uint64_t ctr_in_inc = (i*data_per_block+k) *col_count + j; 
                        ctr128_inc_for_count(ctr_in,  ctr_in_inc);

                        if (sgx_aes_ctr_decrypt(key_in, (uint8_t *)in_data, decrypt_cols * sizeof(T), ctr_in, 128, (uint8_t *)(in_cache + k * data_per_block)) != SGX_SUCCESS) {
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
                    unsigned char ctr_out[16] = {0};
                    if (base_ctr != NULL)
                        memcpy(ctr_out, base_ctr, 16);

                    uint64_t out_offset = ((j*data_per_block+k) * work_rows);
                    T *out_data = out + out_offset;
                    uint64_t ctr_out_inc = (j*data_per_block+k) *row_count;
                    ctr128_inc_for_count(ctr_out,  ctr_out_inc);
                        
                    if (sgx_aes_ctr_encrypt(key_out, (uint8_t *)(out_cache[k]), work_rows * sizeof(T), ctr_out, 128, (uint8_t *)out_data) != SGX_SUCCESS) {
                        printf("\033[33m %s: sgx_aes_ctr_encrypt failed \033[0m\n", __FUNCTION__);
                    }                
                }
            }
            
            delete[] in_cache;
        };
    }
    task_notify(thread_num);
    fun_ptrs[0]();
    task_wait_done(thread_num);

    return rows;
#endif
}


#endif