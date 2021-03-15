#include "Enclave.h"
#include "Enclave_t.h"

#include "eigenvectors.h"
#include "threads_conf.h"

void calcEigenvectors(Matrix<matrix_real> &PC, \
                const std::vector<matrix_real> &var, \
                Matrix<matrix_real> &ret, const int nPC, const int maxIter) {
    // Unparallel
    // const int thread_num = 3;
    const int n = ret.rows();
    PC.resize(nPC, n);
    // const Matrix<matrix_real> I = \
    //             Matrix<matrix_real>::Identity(n, n);
    // int eds[thread_num];
    // for(int i=0;i<thread_num;i++)
    //     eds[i] = nPC/thread_num;
    // for(int i=0;i<nPC%thread_num;i++)
    //     eds[i]++;
    // for(int i=1;i<thread_num;i++)eds[i] +=eds[i-1];
    // for (int tid = 0;tid<thread_num; tid++) {
    //     const int ed = eds[tid];
    //     const int st = tid?eds[tid-1]:0;
    //     fun_ptrs[tid] = [st, ed, tid, &PC, &I, maxIter, &ret, &var, n]() {
    //         for(int i=st;i<ed;i++) {
    //             const Matrix<matrix_real> inv = 
    //                 (ret-I*var[i]).householderQr().solve(I);
    //             ColVector<matrix_real> v0 = ColVector<matrix_real>::Random(n);
    //             for(int j=0;j<maxIter;j++) {
    //                 v0 = (inv * v0).normalized();
    //                 matrix_real error = std::abs(var[i] - v0.transpose()*ret*v0);
    //                 if (error<1e-9)break;
    //                 // printf("Thread %d, iter %d, error = %f\n", tid, j, error);
    //             }
    //             PC.row(i) = v0.transpose() * sqrt(var[i]);
    //         }
    //     };
    // }
    // task_notify(thread_num);
    // fun_ptrs[0]();
    // task_wait_done(thread_num);
    for(int i=0;i<nPC;i++) {
        ret.diagonal().array() -= var[i];
        const Matrix<matrix_real> inv = ret.inverse();
        ColVector<matrix_real> v0 = ColVector<matrix_real>::Random(n), v1(n);
        for(int j=0;j<maxIter;j++) {
            v1 = (inv * v0).normalized();
            matrix_real error = std::abs((v0-v1).sum());
// if(j<10)printf("iter #%d, error: %.10f\n", j, error);
            if (error<1e-6)break;
            v0 = v1;
        }
        PC.row(i) = v0.transpose() * sqrt(var[i]);
        ret.diagonal().array() += var[i];
    }
}