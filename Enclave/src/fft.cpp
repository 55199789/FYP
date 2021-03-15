#include <math.h>
#include <algorithm>
#include "fft.h"
const double PI = acos(-1.0);

void change(Complex y[], int len) {
    int i, j, k;
    for(i=1, j=len/2;i<len-1;i++) {
        if(i<j) std::swap(y[i], y[j]);
        k = len>>1;
        while(j>=k) {
            j-=k;
            k>>=1;
        }
        if(j<k) j+=k;
    }
}

Complex* fft(Complex y[], int len, int on) {
    change(y, len);
    for(int h=2;h<=len; h<<=1) {
        Complex wn(cos(-on*2*PI/h), sin(-on*2*PI/h));
        for(int j=0;j<len;j+=h){
            Complex w(1, 0);
            for(int k=j;k<j+h/2;k++) {
                Complex u = y[k];
                Complex t = w*y[k+h/2];
                y[k] = u+t;
                y[k+h/2] = u-t;
                w *= wn;
            }
        }
    }
    // if(on==-1)
    //     for(int i=0;i<len;i++)y[i].x/=len;
    return y;
}

Complex* fft(double y_[], int len, int on) {
    Complex* y = new Complex[len];
    for(int i=0;i<len;i++)y[i].x=y_[i];
    return fft(y, len, on);
}
