// approx <- function(x, y = NULL, xout, method = "linear", n = 50, yleft, yright, rule = 1, f = 0, ties = mean, na.rm = TRUE)
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <cmath>
#include <numeric>
#include "approx.h"

typedef union
{
    double value;
    unsigned int word[2];
} ieee_double;

static const int hw = 1;
static const int lw = 0;

static double R_ValueOfNA(void) {
    /* The gcc shipping with Fedora 9 gets this wrong without
     * the volatile declaration. Thanks to Marc Schwartz. */
    volatile ieee_double x;
    x.word[hw] = 0x7ff00000;
    x.word[lw] = 1954;
    return x.value;
}

int R_IsNA(double x) {
    if (std::isnan(x)) {
	ieee_double y;
	y.value = x;
	return (y.word[lw] == 1954);
    }
    return 0;
}

int R_IsNaN(double x) {
    if (std::isnan(x)) {
	ieee_double y;
	y.value = x;
	return (y.word[lw] != 1954);
    }
    return 0;
}



// typedef double matrix_real;
typedef int R_xlen_t;
typedef struct {
    double ylow;
    double yhigh;
    double f1;
    double f2;
    int kind;
    int na_rm;
} appr_meth;

static double approx1(double v, double *x, double *y, R_xlen_t n,
		      appr_meth *Meth) {
    /* Approximate  y(v),  given (x,y)[i], i = 0,..,n-1 */


    // add by ice
    // if(!n) return R_NaN;
    // if (!n) {
    //     printf("%s: !n, so NaN returned\n", __FUNCTION__);
    //     return nan("");
    // }

    R_xlen_t
	i = 0,
	j = n - 1;
    /* handle out-of-domain points */
    if(v < x[i]) return Meth->ylow;
    if(v > x[j]) return Meth->yhigh;


    /* find the correct interval by bisection */
    while(i < j - 1) { /* x[i] <= v <= x[j] */
	R_xlen_t ij = (i+j) / 2;
	/* i+1 <= ij <= j-1 */
	if(v < x[ij]) j = ij; else i = ij;
	/* still i < j */

    }
    /* provably have i == j-1 */

    /* interpolation */

    if(v == x[j]) return y[j];
    if(v == x[i]) return y[i];
    /* impossible: if(x[j] == x[i]) return y[i]; */

    if(Meth->kind == 1) /* linear */
	return y[i] + (y[j] - y[i]) * ((v - x[i])/(x[j] - x[i]));
    else /* 2 : constant */
	return (Meth->f1 != 0.0 ? y[i] * Meth->f1 : 0.0)
	     + (Meth->f2 != 0.0 ? y[j] * Meth->f2 : 0.0);
}/* approx1() */


static void
R_approxfun(double *x, double *y, R_xlen_t nxy, double *xout, double *yout,
	    R_xlen_t nout, int method, double yleft, double yright, double f, int na_rm) {
    appr_meth M = {0.0, 0.0, 0.0, 0.0, 0}; /* -Wall */

    M.f2 = f;
    M.f1 = 1 - f;
    M.kind = method;
    M.ylow = yleft;
    M.yhigh = yright;
    M.na_rm = na_rm;
#ifdef DEBUG_approx
    REprintf("R_approxfun(x,y, nxy = %.0f, .., nout = %.0f, method = %d, ...)",
	     (double)nxy, (double)nout, Meth->kind);
#endif
    for(R_xlen_t i = 0; i < nout; i++)
	yout[i] = (std::isnan(xout[i])!=0) ? xout[i] : approx1(xout[i], x, y, nxy, &M);
}


void regularize_values (double *x, double *y,size_t len, double **poutx, \
                        double **pouty, size_t *out_len, bool na_rm) {
    na_rm = true;

    size_t ok_len = 0;
    for (int i = 0; i < len; i++)
        if (!R_IsNA(x[i]) && !R_IsNA(y[i]))
            ok_len ++;
    // printf("ok_len: %d\n", ok_len);
    double *ok_x = new double[ok_len];
    double *ok_y = new double[ok_len];

    size_t idx = 0;
    for (int i = 0; i < len; i++) 
        if (!R_IsNA(x[i]) && !R_IsNA(y[i])) {
            ok_x[idx] = x[i];
            ok_y[idx] = y[i];
            idx ++; 
        }

    bool is_unsorted = false;
    for (int i = 0; i < ok_len - 1; i++)
        if (ok_x[i] > ok_x[i+1]) {
            is_unsorted = true;
            break;
        }

    double *order_x = ok_x, *order_y = ok_y;
    size_t order_len = ok_len;
    if (is_unsorted) {
        size_t *sort_list = new size_t[ok_len];
        std::iota(sort_list, sort_list + ok_len, 0);
        std::sort(sort_list, sort_list + ok_len, \
            [&](int idx1, int idx2) {return ok_x[idx1] <= \
                                        ok_x[idx2];});
    
        order_x = new double[order_len];
        order_y = new double[order_len];

        for (int i = 0; i < order_len; i++) {
            order_x[i] = ok_x[sort_list[i]];
            order_y[i] = ok_y[sort_list[i]];
        }

        delete[] ok_x;
        delete[] ok_y;
        delete[] sort_list;
    }

    int out_idx = 0;
    double tmp_sum = 0.0;
    double tmp_x = 0;
    int tmp_cnt = 0;
    for (int i = 0; i < order_len; i++) {
        if (i == 0) {
            tmp_sum = order_y[i];
            tmp_x = order_x[i];
            tmp_cnt = 1;
        } else {
            if (order_x[i] == tmp_x) {
                tmp_sum += order_y[i];
                tmp_cnt ++;
            } else {
                order_y[out_idx] = tmp_sum / (double)tmp_cnt;
                order_x[out_idx] = tmp_x;
                out_idx++;
                tmp_sum = order_y[i];
                tmp_cnt = 1;
                tmp_x = order_x[i];
            }
        }

        if (i == order_len - 1) {
            order_y[out_idx] = tmp_sum / (double)tmp_cnt;
            order_x[out_idx] = tmp_x;
            out_idx++;
        }
    }

    *poutx = order_x;
    *pouty = order_y;
    *out_len = out_idx;

    return;
}


void approx(double *x, double *y, size_t len,  double *xout, double * yout, size_t out_len) {
    // int n = 50; // seems useless here
    double f = 0;
    int ties = 0;
    bool rmNA = true;

    // linear    
    int method = 1;

    double *reg_x, *reg_y;
    size_t reg_len;

    regularize_values(x, y, len, &reg_x, &reg_y, &reg_len, true);

    // printf("reg_len: %d\n", reg_len);

    double yright = R_ValueOfNA();
    double yleft = R_ValueOfNA();
    
    R_approxfun(reg_x, reg_y, reg_len, xout, yout, out_len, method, yleft, yright, f, rmNA);

    delete[] reg_x;
    delete[] reg_y;
}