/* bench/matmul.prg in C: the product of two 100 x 100 integer matrices, 20 times. */
#include "bench.h"

#define N 100
static int64_t a[N][N], b[N][N], c[N][N];

int main(void) {
  long long t = bench_ms(), total = 0;
  for (int i = 0; i < N; ++i)
    for (int j = 0; j < N; ++j) {
      a[i][j] = (i + j) % 7;
      b[i][j] = (i * j) % 5;
    }
  for (int rep = 1; rep <= 20; ++rep)
    for (int i = 0; i < N; ++i)
      for (int j = 0; j < N; ++j) {
        int64_t s = 0;
        for (int k = 0; k < N; ++k) s += a[i][k] * b[k][j];
        c[i][j] = s;
        total += s;
      }
  bench_report("matmul", t, total == 20LL * 4798200);
  return 0;
}
