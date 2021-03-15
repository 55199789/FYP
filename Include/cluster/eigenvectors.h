#ifndef CALC_EIGENVECTORS
#define CALC_EIGENVECTORS
#include "utils.h"

void calcEigenvectors(Matrix<matrix_real> &, \
                const std::vector<matrix_real> &, \
                Matrix<matrix_real> &, const int nPC, \
                const int maxIter = 100);

#endif