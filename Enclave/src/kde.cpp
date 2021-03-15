#include "approx.h"
#include "fft.h"
#include "kde.h"
#include <algorithm>

real* uniform(real from, real to, int len) {
    real *dest = new real[len];
    real by = (to - from)/(len-1);
    dest[0] = from;
    for(int i=1;i<len;i++)dest[i] = from + i*by;
    dest[len-1] = to;
    return dest;
}

real* binDist(Vector<matrix_real>& x, real wi, real xlo, real xhi, int n) {
    real* y = new real[2*n];
    const int ixmin = 0, ixmax = n - 1;
    real xdelta = (xhi-xlo)/(n-1);
    memset(y, 0, sizeof(real)*2*n);
    int N = x.size();
    for(int i=0;i<N;i++) {
        real xpos = (x[i]-xlo)/xdelta;
        if(xpos>INT_MAX || xpos<INT_MIN)continue;
        int ix = floor(xpos);
        real fx = xpos - ix;
        if(ixmin<=ix && ix<ixmax) {
            y[ix] += (1-fx)*wi;
            y[ix+1] += fx*wi;
        }else if(ix==-1)y[0] +=fx*wi;
        else if(ix==ixmax) y[ix]+=(1-fx)*wi;
        
    }
    return y;
}

real KDE::bw_default(const Vector<matrix_real> & vec) {
    const int n = vec.size();
    real sum2 = vec.sum(); sum2*=sum2;
    real var = (vec.array().square().sum() - sum2/n)/(n-1);
    real ret1 = sqrt(var);
    real ret2 = (quantile(vec, 0.75) - quantile(vec, 0.25))/real(1.34);
    real tmp_res = std::min(ret1, ret2);
    if (tmp_res == 0)   tmp_res = ret1;
    if (tmp_res == 0)   tmp_res = std::abs(vec(0));
    if (tmp_res == 0)   tmp_res = 1.0;

    return 0.9 * tmp_res * pow(real(n), -0.2);
}

real KDE::density(matrix_real *vec_data, size_t vec_size, Kernel kernel, int n, bool max, \
                BwOption bw, real adjust, real from, real to) {

    Vector<matrix_real> vec(vec_size);
    for(int i = 0; i < vec_size; i++) {
        vec(i) = vec_data[i];
    }
// printf("x[5] = %.20f\n", vec_data[5]);
    real bandwidth = bw(vec) * adjust;
    int N = vec.size();
    // Calculate from/to
    if(to<0) {
        to = vec(0);
        for(int i=1;i<N;i++)
            if(vec(i)>to)to = vec(i);
        to += 3*bandwidth;
    }
    const int kords_len = 2*n;
    real up = to + 4*bandwidth;
    real lo = from - 4*bandwidth;
    real *kords = uniform(0, 2*(up-lo), kords_len);
    for(int i=n+1;i<kords_len;i++)
        kords[i] = std::abs(-kords[kords_len - i]);
    const real a = bandwidth*sqrt(5.0);
    for(int i=0;i<kords_len;i++)
        kords[i] = kords[i]<a?Epanechnikov(kords[i]/a)/a:0;
    Complex* kords_dft = fft(kords, kords_len);
    for(int i=0;i<kords_len;i++)
        kords_dft[i].y=-kords_dft[i].y;
    real *y = binDist(vec, real(1.0)/N, lo, up, n);
    Complex* y_dft = fft(y, kords_len);
    for(int i=0;i<kords_len;i++)
        kords_dft[i] *= y_dft[i];
    delete[] y;
    delete[] y_dft;
    kords_dft = fft(kords_dft, kords_len, -1);
    for(int i=0;i<n;i++)kords[i] = std::max(0.0, kords_dft[i].x/kords_len);
    real *xords = uniform(lo, up, n);
    real *x = uniform(from, to, n);
    real *yout = new real[n];
    approx(xords, kords, n, x, yout, n);
    real m_y = yout[0], ret = x[0];
    for(int i=1;i<n;i++)
        if(m_y < yout[i] && max || m_y>yout[i] && !max && m_y>1e-17) {
            m_y = yout[i];
            ret = x[i];
        }
    delete[] kords;
    delete[] kords_dft;
    delete[] yout;    
    return ret;
}
