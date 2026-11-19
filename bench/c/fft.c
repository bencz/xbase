/* bench/fft.prg in C: the iterative radix-2 FFT of 65,536 complex points from a linear congruential generator,
 * forward and back 10 times, twiddle factors from cos and sin of 2 pi / 2^k. */
#include "bench.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* The transform of re + i im in place; sign -1 forward, 1 backward (unscaled). */
static void fft(double *re, double *im, int64_t n, const double *cosT, const double *sinT, double sign) {
  int64_t j = 0;
  for (int64_t i = 0; i <= n - 2; ++i) {
    if (i < j) {
      double tmp = re[i];
      re[i] = re[j];
      re[j] = tmp;
      tmp = im[i];
      im[i] = im[j];
      im[j] = tmp;
    }
    int64_t k = n >> 1;
    while (k <= j) {
      j -= k;
      k >>= 1;
    }
    j += k;
  }
  for (int64_t len = 2, st = 0; len <= n; len <<= 1, ++st) {
    int64_t half = len >> 1;
    double wr0 = cosT[st], wi0 = sign * sinT[st];
    for (int64_t i = 0; i < n; i += len) {
      double wr = 1.0, wi = 0.0;
      for (int64_t jj = i; jj <= i + half - 1; ++jj) {
        int64_t k = jj + half;
        double xr = re[k] * wr - im[k] * wi;
        double xi = re[k] * wi + im[k] * wr;
        re[k] = re[jj] - xr;
        im[k] = im[jj] - xi;
        re[jj] += xr;
        im[jj] += xi;
        double tmp = wr * wr0 - wi * wi0;
        wi = wr * wi0 + wi * wr0;
        wr = tmp;
      }
    }
  }
}

int main(void) {
  long long t = bench_ms();
  int logN = 16;
  int64_t n = (int64_t)1 << logN, seed = 2024;
  size_t bytes = sizeof(double) * (size_t)n;
  double *re = malloc(bytes), *im = malloc(bytes), *re0 = malloc(bytes), *im0 = malloc(bytes);
  double cosT[64], sinT[64], spec = 0.0, maxErr = 0.0;
  const double pi = 3.14159265358979323846;
  for (int64_t i = 0; i < n; ++i) {
    seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
    re[i] = (double)seed / 1073741824.0 - 1.0;
    seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
    im[i] = (double)seed / 1073741824.0 - 1.0;
  }
  memcpy(re0, re, bytes);
  memcpy(im0, im, bytes);
  /* cos and sin of 2 pi / 2^k, for k from 1 */
  for (int i = 0; i < logN; ++i) {
    cosT[i] = cos(2.0 * pi / (double)((int64_t)1 << (i + 1)));
    sinT[i] = sin(2.0 * pi / (double)((int64_t)1 << (i + 1)));
  }
  for (int rep = 1; rep <= 10; ++rep) {
    fft(re, im, n, cosT, sinT, -1.0);
    if (rep == 1)
      for (int64_t i = 0; i < n; ++i) spec += fabs(re[i]) + fabs(im[i]);
    fft(re, im, n, cosT, sinT, 1.0);
    for (int64_t i = 0; i < n; ++i) {
      re[i] = re[i] / (double)n;
      im[i] = im[i] / (double)n;
    }
  }
  for (int64_t i = 0; i < n; ++i) maxErr = fmax(maxErr, fmax(fabs(re[i] - re0[i]), fabs(im[i] - im0[i])));
  int ok = fabs(spec / 15471706.35354992 - 1) < 0.000000001 && maxErr < 0.000000001;
  free(re);
  free(im);
  free(re0);
  free(im0);
  bench_report("fft", t, ok);
  return 0;
}
