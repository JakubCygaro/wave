#ifndef COMPLEX_H
#define COMPLEX_H
#include <math.h>
typedef struct {
    double re;
    double im;
} cmx;

/// cmx fmt for printf and other such functions, uses %lf
#define CMXFMT "%lf%+lfi"
/// cmx fmt for printf and other such functions, uses %e
#define CMXFMTE "%e%+ei"
/// expands to arguments for printing a complex value
#define CMXP(c) c.re, c.im

/// construct a complex number with the real component set to re, and the imaginary component set to 0
cmx cmx_re(double re);
/// construct a complex number with the imaginary component set to im, and the real component set to 0
cmx cmx_im(double im);
/// construct a complex number with the imaginary component set to im, the real component set to re
cmx cmx_from(double re, double im);
/// rounds internal values of re and im before comparison
int cmx_eq_rounded(cmx a, cmx b);
/// does no rounding of the internal values of re and im before comparison
int cmx_eq_exact(cmx a, cmx b);
cmx cmx_add(cmx a, cmx b);
cmx cmx_sub(cmx a, cmx b);
cmx cmx_mul(cmx z1, cmx z2);
cmx cmx_div(cmx a, cmx b);
cmx cmx_exp(cmx z);
double cmx_mod(cmx z);

#ifdef COMPLEX_IMPL
cmx from_im(double im)
{
    return (cmx) {
        .re = 0,
        .im = im,
    };
}
cmx from_re(double re)
{
    return (cmx) {
        .re = re,
        .im = 0,
    };
}
cmx cmx_from(double re, double im)
{
    return (cmx) { .re = re, .im = im };
}
int cmx_eq_rounded(cmx a, cmx b)
{
    return (round(a.re) == round(b.re)) && (round(a.im) == round(b.im));
}
int cmx_eq_exact(cmx a, cmx b)
{
    return ((a.re) == (b.re)) && ((a.im) == (b.im));
}
cmx cmx_add(cmx a, cmx b)
{
    return (cmx) {
        .re = a.re + b.re,
        .im = a.im + b.im
    };
}
cmx cmx_sub(cmx a, cmx b)
{
    return (cmx) {
        .re = a.re - b.re,
        .im = a.im - b.im
    };
}
cmx cmx_mul(cmx z1, cmx z2)
{
    double a_1 = z1.re;
    double b_1 = z1.im;
    double a_2 = z2.re;
    double b_2 = z2.im;
    return (cmx) {
        .re = a_1 * a_2 - b_1 * b_2,
        .im = a_1 * b_2 + a_2 * b_1
    };
}
cmx cmx_div(cmx w, cmx z)
{
    double u = w.re;
    double v = w.im;
    double x = z.re;
    double y = z.im;
    double div = ((x * x) + (y * y));
    return (cmx) {
        .re = ((u * x) + (v * y)) / div,
        .im = ((v * x) - (u * y)) / div,
    };
}
double cmx_mod(cmx z){
    return sqrtl(z.re * z.re + z.im * z.im);
}
cmx cmx_exp(cmx z)
{
    double a = z.re;
    double b = z.im;
    cmx e_a = from_re(exp(a));
    cmx ang = cmx_add(from_re(cos(b)), from_im(sin(b)));
    return cmx_mul(e_a, ang);
}
#endif
#endif
