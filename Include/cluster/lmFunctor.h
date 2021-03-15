#ifndef LM_FUNCTOR
#define LM_FUNCTOR
#include "utils.h"
class LMFunctor {
public:
    const int m;
//     ColArray<matrix_real>* xval;
    matrix_real *xval, *y_hat;
    const matrix_real* yval;
    int operator()(const ColVector<matrix_real> &par, \
                    ColVector<matrix_real> &fvec) const;
    int df(const ColVector<matrix_real> &par, \
            Eigen::Matrix<matrix_real, -1, -1, 0>& fjac) const;
    int values() const { return m;}
    int inputs() {return 2;}
    LMFunctor(matrix_real *x, \
               const matrix_real *y, \
               const int m_):m(m_), xval(x), yval(y){
        y_hat = new matrix_real[m];
        // xval.resize(m);
        // new (&xval) Eigen::Map<ColArray<matrix_real> >(x, m);
    };
    ~LMFunctor() {
            yval = xval = 0;
            delete[] y_hat;
    }
};

#endif