#include "lmFunctor.h"

int LMFunctor::operator()(const ColVector<matrix_real> &par, \
                          ColVector<matrix_real> &fvec) const {
    const matrix_real& a = par(0);
    const matrix_real& b = par(1);
    for(int i=0;i<m;i++) 
        fvec(i) = yval[i] - f(a, b, xval[i]);
    return 0;
}

int LMFunctor::df(const ColVector<matrix_real> &par,\
                     Eigen::Matrix<matrix_real, -1, -1, 0>& fjac) const {

    // Numeric way: 
    // real epsilon;
    // epsilon = 1e-5f;

    // for (int i = 0; i < par.size(); i++) {
    //     ColVector<matrix_real> xPlus(par);
    //     xPlus(i) += epsilon;
    //     ColVector<matrix_real> xMinus(par);
    //     xMinus(i) -= epsilon;

    //     ColVector<matrix_real>  fvecPlus(values());
    //     operator()(xPlus, fvecPlus);

    //     ColVector<matrix_real>  fvecMinus(values());
    //     operator()(xMinus, fvecMinus);

    //     ColVector<matrix_real> fvecDiff(values());
    //     fvecDiff = (fvecPlus - fvecMinus) / (2.0f * epsilon);

    //     fjac.block(0, i, values(), 1) = fvecDiff;
    // }
    // Use the formula
    const matrix_real& a = par(0);
    const matrix_real& b = par(1);
    // ColArray<matrix_real> y_hat(m);
    for(int i=0;i<m;i++) y_hat[i] = f(a, b, xval[i]);
    // matrix_real *y_hat = new matrix_real[m];
    // y_hat = (y_hat - 1)*y_hat;
    // fjac.col(1) = y_hat * a;
    // fjac.col(0) = y_hat * (b-xval);
    const matrix_real *y_hat_end = y_hat + m;
     for(matrix_real *cur=y_hat; cur!=y_hat_end;cur++) 
        *cur = (*cur - 1) * *cur;
    for(matrix_real *cur = y_hat, *col_1 = fjac.col(1).data(), \
            *col_0 = fjac.col(0).data(), *x = xval; \
            cur!=y_hat_end; cur++, col_1++, x++, col_0++) {
        *col_1 = *cur * a;
        *col_0 = *cur * (b- *x);
    }
    return 0;
}