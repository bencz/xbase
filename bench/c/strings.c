/* bench/strings.prg in C: Left, Upper, Len and At on 20,000,000 short strings (each made in a buffer). */
#include "bench.h"
#include <ctype.h>
#include <string.h>

int main(void) {
  long long t = bench_ms(), total = 0;
  const char *src = "abcdefghij";
  char w[16];
  for (int64_t i = 1; i <= 20000000; ++i) {
    size_t len = (size_t)(i % 10 + 1);
    for (size_t k = 0; k < len; ++k) w[k] = (char)toupper((unsigned char)src[k]);
    w[len] = '\0';
    const char *d = strchr(w, 'D');
    total += (long long)strlen(w) + (d ? d - w + 1 : 0);
  }
  bench_report("strings", t, total == 166000000);
  return 0;
}
