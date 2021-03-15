// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2010 Gael Guennebaud <gael.guennebaud@inria.fr>
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef EIGEN_PARALLELIZER_H
#define EIGEN_PARALLELIZER_H

#include <threads_conf.h>


// #include <atomic>
// #include <condition_variable>
// #include <thread>
// #include <functional>
// #include <mutex>

// extern struct productinfo gbl_productinfo;
// extern std::atomic<bool> is_done[THREAD_NUM];
// extern std::mutex mtx[THREAD_NUM];
// extern std::condition_variable cv[THREAD_NUM];
// // extern std::atomic<bool> mtx[THREAD_NUM];
// extern std::function<void()> fun_ptrs[THREAD_NUM];


namespace Eigen {

namespace internal {

/** \internal */
inline void manage_multi_threading(Action action, int* v)
{
  static EIGEN_UNUSED int m_maxThreads = -1;

  if(action==SetAction)
  {
    eigen_internal_assert(v!=0);
    m_maxThreads = *v;
    printf("[ICE][manage_multi_threading][%d]\n", m_maxThreads);
  }
  else if(action==GetAction)
  {
    // printf("[ICE][manage_multi_threading][%d]\n", m_maxThreads);

    eigen_internal_assert(v!=0);

    // add by ice
    #if defined (EIGEN_HAS_OPENMP) || defined (ICE_MULTI_THREAD)
    if(m_maxThreads>0) {
      *v = m_maxThreads;
    }
    else {
      printf("[ERR][manage_multi_threading]\n");
      while(1)
        ;
    }
    
    #else
    *v = 1;
    #endif
  }
  else
  {
    eigen_internal_assert(false);
  }
}

}

/** Must be call first when calling Eigen from multiple threads */
inline void initParallel()
{
  int nbt;
  internal::manage_multi_threading(GetAction, &nbt);
  std::ptrdiff_t l1, l2, l3;
  internal::manage_caching_sizes(GetAction, &l1, &l2, &l3);
}

/** \returns the max number of threads reserved for Eigen
  * \sa setNbThreads */
inline int nbThreads()
{
  int ret;
  internal::manage_multi_threading(GetAction, &ret);
  return ret;
}

/** Sets the max number of threads reserved for Eigen
  * \sa nbThreads */
inline void setNbThreads(int v)
{
  internal::manage_multi_threading(SetAction, &v);
}

namespace internal {

template<typename Index> struct GemmParallelInfo
{
  GemmParallelInfo() : sync(-1), users(0), lhs_start(0), lhs_length(0) {}

  Index volatile sync;
//  int volatile users;
  std::atomic<int> users;

  Index lhs_start;
  Index lhs_length;
};


template<bool Condition, typename Functor, typename Index>
void parallelize_gemm(const Functor& func, Index rows, Index cols, Index depth, bool transpose)
{

      //printf("[parallelize_gemm] SINGLE-THREAD\n");
  // printf("[parallelize_gemm]rows=%02ld[]cols=%02ld[]transpose=%02d[]\n", rows, cols, transpose ? 1 : 0);
  // TODO when EIGEN_USE_BLAS is defined,
  // we should still enable OMP for other scalar types
// #if !(defined (EIGEN_HAS_OPENMP)) || defined (EIGEN_USE_BLAS)
// add by ice
#if (!(defined (EIGEN_HAS_OPENMP)) && !(defined (ICE_MULTI_THREAD))) || defined (EIGEN_USE_BLAS)

  // FIXME the transpose variable is only needed to properly split
  // the matrix product when multithreading is enabled. This is a temporary
  // fix to support row-major destination matrices. This whole
  // parallelizer mechanism has to be redisigned anyway.
  EIGEN_UNUSED_VARIABLE(depth);
  EIGEN_UNUSED_VARIABLE(transpose);

  //add by ice
//  static int count = 0;
  // printf("[parallelize_gemm](single thread)[%d][%d][%s]\n", rows, cols, transpose ? "T" : "F");

  func(0,rows, 0,cols);
  // printf("[ICE][%d][%d][%s]\n", rows, cols, transpose ? "T" : "F");

#else

  // Dynamically check whether we should enable or disable OpenMP.
  // The conditions are:
  // - the max number of threads we can create is greater than 1
  // - we are not already in a parallel code
  // - the sizes are large enough

  // compute the maximal number of threads from the size of the product:
  // This first heuristic takes into account that the product kernel is fully optimized when working with nr columns at once.
  Index size = transpose ? rows : cols;
  Index pb_max_threads = std::max<Index>(1,size / Functor::Traits::nr);

  // compute the maximal number of threads from the total amount of work:
  double work = static_cast<double>(rows) * static_cast<double>(cols) *
      static_cast<double>(depth);
  double kMinTaskSize = 50000;  // FIXME improve this heuristic.
  pb_max_threads = std::max<Index>(1, std::min<Index>(pb_max_threads, work / kMinTaskSize));


  // compute the number of threads we are going to use
  Index threads = std::min<Index>(nbThreads(), pb_max_threads);

  // if multi-threading is explicitely disabled, not useful, or if we already are in a parallel session,
  // then abort multi-threading
  // FIXME omp_get_num_threads()>1 only works for openmp, what if the user does not use openmp?
  //if((!Condition) || (threads==1) || (omp_get_num_threads()>1))
  //  return func(0,rows, 0,cols);

  // printf("[parallelize_gemm]threads: %d  %d   %d\n", threads, nbThreads(), pb_max_threads);

  if((!Condition) || (threads==1))
    return func(0,rows, 0,cols);

  Eigen::initParallel();
  func.initParallelSession(threads);

  if(transpose)
    std::swap(rows,cols);

  ei_declare_aligned_stack_constructed_variable(GemmParallelInfo<Index>,info,threads,0);

  // add by ice
  // add for eigen's matrix multiplication
  // add by ice
  struct productinfo {
      int thread_num;
      int rows;
      int cols;
      bool transpose;
      productinfo() {};
      productinfo(int thread_num, int rows, int cols, bool transpose)
                : thread_num(thread_num), rows(rows), cols(cols), transpose(transpose) {};
      ~productinfo() {};
  };
  struct productinfo gbl_productinfo(THREAD_NUM, rows, cols, transpose);
  // struct productinfo gbl_productinfo;
  // gbl_productinfo.rows = rows;
  // gbl_productinfo.cols = cols;
  // gbl_productinfo.transpose = transpose;
  // gbl_productinfo.thread_num = THREAD_NUM;
  // printf("[gbl_productinfo][row][%d][col][%d][transpose][%d][func][%p]\n", gbl_productinfo.rows, gbl_productinfo.cols, gbl_productinfo.transpose, &func);

  if (threads > gbl_productinfo.thread_num) {
    printf("[ERR]threads[%d][>][thread_num][%d]\n", threads, gbl_productinfo.thread_num);
    while (1)
      ;
    
  }

  for (int i = 0; i < threads; i++) {
    fun_ptrs[i] = [i, threads, &gbl_productinfo, &func, &info]() {
      // printf("--------[%d][%p][%p]-----------\n",i, &func, (void *)info);
      long blockCols = gbl_productinfo.cols / threads &  ~long(0x3);
      long blockRows = gbl_productinfo.rows / threads;
      blockRows = (blockRows/Functor::Traits::mr)*Functor::Traits::mr;

      long r0 = i*blockRows;
      long actualBlockRows = (i+1== threads) ? gbl_productinfo.rows - r0 : blockRows;

      long c0 = i*blockCols;
      long actualBlockCols = (i+1== threads) ? gbl_productinfo.cols -c0 : blockCols;
    
      info[i].lhs_start = r0;
      info[i].lhs_length = actualBlockRows;


      if(gbl_productinfo.transpose) 
        func(i, threads, c0, actualBlockCols, 0, gbl_productinfo.rows, info);
      else          
        func(i, threads, 0, gbl_productinfo.rows, c0, actualBlockCols, info); 
    };
  }

  task_notify(threads);
  fun_ptrs[0]();
  task_wait_done(threads);


  // for (int i = 1; i < threads; i++) {
  //   cv[i].notify_one();
  // }

    // for (int i = 1; i < threads; i++) {
    //   mtx[i] = true;
    // }

    // Index tid = 0;

    // Index blockCols = (cols / threads) & ~Index(0x3);
    // Index blockRows = (rows / threads);
    // blockRows = (blockRows/Functor::Traits::mr)*Functor::Traits::mr;

    // Index r0 = tid*blockRows;
    // Index actualBlockRows = (tid+1==threads) ? rows-r0 : blockRows;

    // Index c0 = tid*blockCols;
    // Index actualBlockCols = (tid+1==threads) ? cols-c0 : blockCols;

    // info[tid].lhs_start = r0;
    // info[tid].lhs_length = actualBlockRows;

    // if(transpose) func(tid, threads, c0, actualBlockCols, 0, rows, info);
    // else          func(tid, threads, 0, rows, c0, actualBlockCols, info);


  // for(int i = 1; i < threads; i++) {
  //   while (is_done[i] == false)
  //     ;
  // }

  // for (int i = 1; i < threads; i++) {
  //   is_done[i] = false;
  // }
  // #pragma omp parallel num_threads(threads)
  // {
  //   Index i = omp_get_thread_num(); //get thread id
  //   // Note that the actual number of threads might be lower than the number of request ones.
  //   Index actual_threads = omp_get_num_threads(); //get running thread

  //   Index blockCols = (cols / actual_threads) & ~Index(0x3);
  //   Index blockRows = (rows / actual_threads);
  //   blockRows = (blockRows/Functor::Traits::mr)*Functor::Traits::mr;

  //   Index r0 = i*blockRows;
  //   Index actualBlockRows = (i+1==actual_threads) ? rows-r0 : blockRows;

  //   Index c0 = i*blockCols;
  //   Index actualBlockCols = (i+1==actual_threads) ? cols-c0 : blockCols;

  //   info[i].lhs_start = r0;
  //   info[i].lhs_length = actualBlockRows;

  //   if(transpose) func(c0, actualBlockCols, 0, rows, info);
  //   else          func(0, rows, c0, actualBlockCols, info);
  // }
#endif
}

} // end namespace internal

} // end namespace Eigen

#endif // EIGEN_PARALLELIZER_H
