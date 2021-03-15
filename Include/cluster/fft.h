#ifndef FFT_Z
#define FFT_Z
#include "utils.h"
struct Complex{
    real x, y;
    Complex(real _x=0.0, real _y=0.0):x(_x), y(_y){}
    inline Complex operator-(const Complex &b) const {
        return Complex(x-b.x, y-b.y);
    }
    inline Complex operator+(const Complex &b) const{
        return Complex(x+b.x, y+b.y);
    }
    inline Complex operator*(const Complex &b) const {
        return Complex(x*b.x - y*b.y, x*b.y+y*b.x);
    }
    inline Complex operator*=(const Complex&b) {
        real _x = x*b.x-y*b.y;
        real _y = x*b.y+y*b.x;
        x = _x; y = _y;
        return *this;
    }
};
void change(Complex y[], int len);

Complex* fft(Complex y[], int len, int on = 1);
Complex* fft(real y_[], int len, int on = 1);
#endif