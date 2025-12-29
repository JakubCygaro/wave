#ifndef COMPLEX_H
#define COMPLEX_H

#include <math.h>
typedef struct {
    double re;
    double im;
} cmx;

cmx from_im(double im);
cmx from_re(double re);
cmx cmx_add(cmx a, cmx b);
cmx cmx_sub(cmx a, cmx b);
cmx cmx_mul(cmx a, cmx b);
cmx cmx_div(cmx a, cmx b);
cmx cmx_exp(cmx z);


#ifdef COMPLEX_IMPL
cmx from_im(double im){
    return (cmx) {
        .re = 0,
        .im = im,
    };
}
cmx from_re(double re){
    return (cmx) {
        .re = re,
        .im = 0,
    };
}
cmx cmx_add(cmx a, cmx b){
    return (cmx) {
        .re = a.re + b.re,
        .im = a.im + b.im
    };
}
cmx cmx_sub(cmx a, cmx b){
    return (cmx) {
        .re = a.re - b.re,
        .im = a.im - b.im
    };
}
cmx cmx_mul(cmx a, cmx b){
    return (cmx) {
        .re = a.re * b.re - a.im * b.im,
        .im = a.re * b.im + a.im * b.re
    };
}
cmx cmx_div(cmx z1, cmx z2){
    double div = (z2.re * z2.re + z2.im * z2.im);
    return (cmx){
        .re = (z1.re * z2.re - z1.im * z2.im) / div,
        .im = (z1.re * z2.im + z1.im * z2.re) / div,
    };
}
cmx cmx_exp(cmx z){
    cmx a = from_re(exp(z.re));
    cmx b = cmx_add(from_re(cos(z.im)), from_im(sin(z.im)));
    return cmx_mul(a, b);
}
#endif
#endif
