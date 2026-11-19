/* bench/string_builder.prg in C: 3 rounds of 400,000 table rows of about 37 bytes appended to one growing buffer
 * (about 15 MB each), its length and every 1,000th byte checked. */
#include "bench.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  char *data;
  size_t len, cap;
} Buf;

static void put(Buf *b, const char *s, size_t n) {
  if (b->len + n + 1 > b->cap) {
    while (b->len + n + 1 > b->cap) b->cap = b->cap ? b->cap * 2 : 64;
    b->data = realloc(b->data, b->cap);
  }
  memcpy(b->data + b->len, s, n);
  b->len += n;
  b->data[b->len] = '\0';
}

int main(void) {
  static const char *const names[] = {"ant", "bee", "cat", "dog", "eel", "fox", "gnu", "hen", "ibis", "jay"};
  long long t = bench_ms();
  int64_t n = 0;
  char row[64];
  for (int64_t r = 1; r <= 3; ++r) {
    Buf c = {0};
    for (int64_t i = 1; i <= 400000; ++i) {
      int len = snprintf(row, sizeof row, "<tr><td>%s</td><td>%lld</td></tr>\n", names[i % 10], (long long)(i + r));
      put(&c, row, (size_t)len);
    }
    n += (int64_t)c.len;
    for (size_t i = 0; i < c.len; i += 1000) n += (unsigned char)c.data[i];
    free(c.data);
  }
  bench_report("string_builder", t, n == 47588053);
  return 0;
}
