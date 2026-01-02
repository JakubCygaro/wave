#ifndef COMPLEX_H
#define COMPLEX_H
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
cmx cmx_recip(cmx z);
double cmx_arg(cmx z);
cmx cmx_rand(void);
cmx* cmx_fft2(const cmx* x, unsigned long n, const int* precomp_bitr, cmx* write_to);
int* cmx_precomp_reversed_bits(int max);
int cmx_rev2(int array_sz, int n);
int cmx_log2i(int n);
#ifdef COMPLEX_IMPL
#include <math.h>
#include <stdlib.h>
cmx cmx_im(double im)
{
    return (cmx) {
        .re = 0,
        .im = im,
    };
}
cmx cmx_re(double re)
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
double cmx_mod(cmx z)
{
    return sqrtl(z.re * z.re + z.im * z.im);
}
cmx cmx_exp(cmx z)
{
    double a = z.re;
    double b = z.im;
    cmx e_a = cmx_re(exp(a));
    cmx ang = cmx_add(cmx_re(cos(b)), cmx_im(sin(b)));
    return cmx_mul(e_a, ang);
}
cmx cmx_conj(cmx z)
{
    return (cmx) {
        z.re,
        -z.im,
    };
}
double cmx_arg(cmx z)
{
    return atan(z.im / z.re);
}
cmx cmx_rand(void)
{
    return (cmx) {
        (double)rand() / (double)(RAND_MAX),
        (double)rand() / (double)(RAND_MAX),
    };
}
int cmx_log2i(int n)
{
    int k = n, i = 0;
    while (k) {
        k >>= 1;
        i++;
    }
    return i - 1;
}
// https://stackoverflow.com/a/37729648
// I added my comments to explain this algorithm
int cmx_rev2(int array_sz, int n)
{
    if (!n)
        return 0;
    int ret = 0;
    // iterates through each bit of n
    // log2(array_sz) is the max size of reversed n in bits, so for example
    // N = 16 -> 4 bits; 0001 -> 1000
    // N = 8 -> 3 bits; 001 -> 100
    // N = 4 -> 2 bits; 01 -> 10
    for (int j = 1; j <= cmx_log2i(array_sz); j++) {
        // 1 << (log2i(array_sz) - j) creates a bit mask for the current
        // bit of the iteration, starting from the leftmost bit
        // assuming array_sz = 16 we will iterate through
        // 1000 ( 1 << 3 )
        // 0100 ( 1 << 2 )
        // 0010 ( 1 << 1 )
        // 0001 ( 1 << 0 )
        // the cond checks wether n has this bit set to 1
        if (n & (1 << (cmx_log2i(array_sz) - j))) {
            // (j - 1) gives us the position of the reversed bit,
            // |= adds it to ret
            // example:
            // n = 0110
            // on mask 0100 (where j = 2), j - 1 equals 1 and so
            // 1 << ( j - 1 ) equals 0010 -> the bit in reverse
            ret |= 1 << (j - 1);
        }
    }
    return ret;
}

int* cmx_precomp_reversed_bits(int max)
{
    int* ret = calloc(max, sizeof(int));
    for (size_t k = 0; k < max; k++) {
        ret[k] = cmx_rev2(max, k);
    }
    return ret;
}
static cmx* cmx_bit_reverse_copy(const cmx* a, size_t n, const int* precomp_bitr, cmx* write_to)
{
    cmx* ret = write_to ? write_to : calloc(n, sizeof(cmx));
    for (size_t k = 0; k < n; k++) {
        int bitr = precomp_bitr ? precomp_bitr[k] : cmx_rev2(n, k);
        ret[bitr] = a[k];
    }
    return ret;
}
// https://en.wikipedia.org/wiki/Cooley%E2%80%93Tukey_FFT_algorithm#Data_reordering,_bit_reversal,_and_in-place_algorithms
cmx* cmx_fft2(const cmx* x, size_t n, const int* precomp_bitr, cmx* write_to)
{
    cmx* out = cmx_bit_reverse_copy(x, n, precomp_bitr, write_to);
    for (size_t s = 1; s <= cmx_log2i(n); s++) {
        size_t m = pow(2, s);
        cmx w_m = cmx_exp(
            cmx_div(
                cmx_mul(
                    cmx_re(-2 * M_PI), cmx_im(1)),
                cmx_re(m)));
        for (size_t k = 0; k < n; k += m) {
            cmx w = cmx_re(1);
            for (size_t j = 0; j < m / 2; j++) {
                cmx t = cmx_mul(w, out[k + j + m / 2]);
                cmx u = out[k + j];
                out[k + j] = cmx_add(u, t);
                out[k + j + m / 2] = cmx_sub(u, t);
                w = cmx_mul(w, w_m);
            }
        }
    }
    if(write_to)
        return NULL;
    else
        return out;
}
#endif
#endif
