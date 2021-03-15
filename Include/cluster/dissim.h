#ifndef DISSIM_H
#define DISSIM_H

#include "utils.h"
// void partialCalcStep(const int , const int, Matrix<matrix_real>&, \
//         const Matrix<matrix_real>&, const Matrix<bool>&, const real);
// void partialCalc(const int , const int, Matrix<matrix_real>&, \
//         const Matrix<matrix_real>&, const Matrix<bool>&, \
//         const real, const real);
void calcDissim(bool, Matrix<matrix_real>&, \
        Matrix<matrix_real>&, Matrix<bool>&, \
        real, real, real);
// void calcDissim(bool, matrix_real* , matrix_real *, \
//         bool *, int, int, real, real, real);
void calcDissim(bool useStepFunction, matrix_real* dis, \
        matrix_real* tags, bool* dropout, matrix_real *dissim_cache[THREAD_NUM], int rows, int cols, \
        real wThreshold, real a, real b, bool first);
void cp2dissim(matrix_real*, matrix_real*, const int, \
                const int, const int);
#endif