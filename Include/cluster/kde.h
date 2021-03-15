#ifndef KDE_H
#define KDE_H
#include <math.h>
#include <vector>
#include "utils.h"
namespace KDE {
    const real PI = acos((real)(-1));
    typedef real (*Kernel)(real u);
    typedef real (*BwOption)(const Vector<matrix_real>  &);
    inline real Uniform(real u) {
        return 0.5;
    }
    inline real Triangular(real u) {
        return 1.0 - abs(u);
    }
    inline real Epanechnikov(real u) {
        // if(std::abs(u)>=1)return 0;
        return 0.75*(1-u*u);
    }
    inline real Quatric(real u) {
        return 15.0/16.0 * (1-u*u) * (1-u*u);
    }
    inline real Cosine(real u) {
        return PI*0.25*cos(PI*0.5*u);
    }

    inline real Guassian(real u) {
        return 1.0/sqrt(2.0*PI)*exp(-0.5*u*u);
    }
    inline real Logistic(real u) {
        return 1.0 / (exp(u)+2+exp(-u));
    }
    inline real Sigmoid(real u) {
        return 2.0 / (PI*(exp(u)+exp(-u)));
    }
    real bw_default(const Vector<matrix_real>  &);

    real density(matrix_real *vec_data, size_t vec_size, Kernel kernel, \
                int n = 512, bool max = true, BwOption bw = bw_default, \
                real adjust = 1, real from = -1, real to = -1);
}

#endif