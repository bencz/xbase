/* bench/codeblock_callback.prg in C: functions that take a function pointer: Simpson's rule over 3 functions (1,000,000
 * intervals each) and an insertion sort of 3,000 numbers that compares through one, 10 times. The pointers are read
 * from volatile variables, so that the compiler calls them as xbase evaluates a block. */
#include "bench.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef double (*Fn)(double);
typedef int (*Less)(int64_t, int64_t);

static int64_t seed = 1;
static int64_t rnd(int64_t n) {
  seed = seed * 16807 % 2147483647;
  return seed % n;
}

static double square(double x) { return x * x; }
static double arctan_slope(double x) { return 1 / (1 + x * x); }
static double cubic(double x) { return x * x * x - 2 * x; }
static int less(int64_t x, int64_t y) { return x < y; }
static int greater(int64_t x, int64_t y) { return x > y; }

static double simpson(Fn f, double a, double b, int64_t n) {
  double h = (b - a) / (double)n, sum = f(a) + f(b);
  for (int64_t i = 1; i <= n - 1; ++i) sum += f(a + (double)i * h) * (i % 2 == 1 ? 4 : 2);
  return sum * h / 3;
}

static void insertion_sort(int64_t *a, int64_t n, Less lt) {
  for (int64_t i = 2; i <= n; ++i) {
    int64_t x = a[i - 1], j = i - 1;
    while (j >= 1 && lt(x, a[j - 1])) {
      a[j] = a[j - 1];
      --j;
    }
    a[j] = x;
  }
}

int main(void) {
  int64_t data[3000], a[3000], check = 0;
  double integral = 0;
  Fn volatile f1 = square, f2 = arctan_slope, f3 = cubic;
  Less volatile up = less, down = greater;
  for (int i = 0; i < 3000; ++i) data[i] = rnd(100000);
  long long t = bench_ms();
  integral += simpson(f1, 0, 3, 1000000);
  integral += simpson(f2, 0, 1, 1000000);
  integral += simpson(f3, -1, 2, 1000000);
  for (int r = 1; r <= 10; ++r) {
    memcpy(a, data, sizeof a);
    insertion_sort(a, 3000, r % 2 == 0 ? up : down);
    check += a[0] + a[1499] * 2 + a[2999] * 3;
  }
  bench_report("codeblock_callback", t, fabs(integral - 10.535398163397) < 0.000000001 && check == 2991620);
  return 0;
}
