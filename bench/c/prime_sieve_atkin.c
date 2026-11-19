/* bench/prime_sieve_atkin.prg in C: the sieve of Atkin up to 2,000,000, 12 times. */
#include "bench.h"
#include <stdbool.h>
#include <stdlib.h>

int main(void) {
  long long t = bench_ms();
  int64_t limit = 2000000;
  int ok = 1;
  bool *a = malloc((size_t)limit + 1);
  for (int rep = 1; rep <= 12; ++rep) {
    for (int64_t i = 1; i <= limit; ++i) a[i] = false;
    for (int64_t x = 1; x * x <= limit; ++x) {
      for (int64_t y = 1; y * y <= limit; ++y) {
        int64_t n = 4 * x * x + y * y;
        if (n <= limit) {
          int64_t r = n % 12;
          if (r == 1 || r == 5) a[n] = !a[n];
        }
        n = 3 * x * x + y * y;
        if (n <= limit && n % 12 == 7) a[n] = !a[n];
        if (x > y) {
          n = 3 * x * x - y * y;
          if (n <= limit && n % 12 == 11) a[n] = !a[n];
        }
      }
    }
    for (int64_t i = 5; i * i <= limit; ++i)
      if (a[i])
        for (int64_t j = i * i; j <= limit; j += i * i) a[j] = false;
    int64_t count = 2;
    for (int64_t i = 5; i <= limit; ++i)
      if (a[i]) ++count;
    ok = ok && count == 148933;
  }
  free(a);
  bench_report("prime_sieve_atkin", t, ok);
  return 0;
}
