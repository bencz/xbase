/* bench/string_concatenation.prg in C: 3,000,000 strings of seven parts picked by index from small tables, each joined
 * in a buffer, its length and last character read. */
#include "bench.h"
#include <string.h>

int main(void) {
  static const char *const a[] = {"alpha", "beta", "gamma", "delta", "epsilon", "zeta", "eta"};
  static const char *const b[] = {"red", "green", "blue", "cyan", "magenta"};
  long long t = bench_ms();
  int64_t n = 0;
  char s[64];
  for (int64_t i = 1; i <= 3000000; ++i) {
    const char *parts[] = {a[i % 7], "-", b[i % 5], "/", a[i % 3], ":", b[i % 4]};
    size_t len = 0;
    for (int k = 0; k < 7; ++k) {
      size_t m = strlen(parts[k]);
      memcpy(s + len, parts[k], m);
      len += m;
    }
    s[len] = '\0';
    n += (int64_t)strlen(s) + (unsigned char)s[len - 1];
  }
  bench_report("string_concatenation", t, n == 378692857);
  return 0;
}
