/* bench/array_nested.prg in C: a 100 x 100 x 100 cube of nested arrays (an array of planes, each an array of rows
 * allocated on their own), filled through a[i][j][k], then 10 passes that change the cells from their neighbors along
 * the last two dimensions and sum them. */
#include "bench.h"
#include <stdlib.h>

#define N 100

int main(void) {
  long long t = bench_ms();
  int64_t ***a = malloc(N * sizeof *a), sum = 0;
  for (int i = 0; i < N; ++i) {
    a[i] = malloc(N * sizeof *a[i]);
    for (int j = 0; j < N; ++j) a[i][j] = malloc(N * sizeof *a[i][j]);
  }
  for (int64_t i = 1; i <= N; ++i)
    for (int64_t j = 1; j <= N; ++j)
      for (int64_t k = 1; k <= N; ++k) a[i - 1][j - 1][k - 1] = (i * 31 + j * 17 + k * 7) % 101;
  for (int p = 1; p <= 10; ++p) {
    for (int i = 0; i < N; ++i) {
      int64_t **plane = a[i];
      for (int j = 1; j < N; ++j) {
        int64_t *row = plane[j];
        for (int k = 1; k < N; ++k) row[k] = (row[k] + plane[j - 1][k] + row[k - 1]) % 1000;
      }
    }
    for (int i = 0; i < N; ++i)
      for (int j = 0; j < N; ++j)
        for (int k = 0; k < N; ++k) sum += a[i][j][k];
  }
  bench_report("array_nested", t, sum == 4905893027LL);
  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < N; ++j) free(a[i][j]);
    free(a[i]);
  }
  free(a);
  return 0;
}
