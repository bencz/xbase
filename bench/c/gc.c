/* bench/gc.prg in C: 40,000 slots holding strings of 16 to 2,000 bytes and small records of them, replaced at random
 * 3,000,000 times (each replaced value freed), while 20,000 records that live to the end are kept beside them. */
#include "bench.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
  char *a, *c;
  int64_t i;
} Rec;
typedef struct {
  int kind; /* 0 none, 1 string, 2 record */
  char *s;
  Rec *r;
} Slot;

static unsigned long long seed = 7;
static int64_t rnd(int64_t n) {
  seed = (seed * 1103515245ULL + 12345ULL) % 2147483648ULL;
  return (int64_t)(seed / 65536) % n;
}

static char *replicate(char ch, int64_t n) {
  char *s = malloc((size_t)n + 1);
  memset(s, ch, (size_t)n);
  s[n] = '\0';
  return s;
}

static void drop(Slot *s) {
  if (s->kind == 1) free(s->s);
  if (s->kind == 2) {
    free(s->r->a);
    free(s->r->c);
    free(s->r);
  }
  s->kind = 0;
}

int main(void) {
  long long t = bench_ms();
  Slot *slots = calloc(40000, sizeof *slots);
  Rec **kept = malloc(sizeof *kept * 20001);
  int64_t nkept = 0, check = 0;
  for (int64_t i = 1; i <= 3000000; ++i) {
    int64_t n = rnd(40000);
    Slot x = {0, NULL, NULL};
    if (rnd(4) == 0) {
      x.kind = 2;
      x.r = malloc(sizeof *x.r);
      x.r->a = replicate((char)(65 + i % 26), 16 + rnd(200));
      x.r->i = i;
      x.r->c = replicate(' ', rnd(100));
    } else {
      x.kind = 1;
      x.s = replicate((char)(97 + i % 26), 16 + rnd(2000));
    }
    drop(&slots[n]);
    slots[n] = x;
    if (i % 150 == 0) {
      Rec *k = malloc(sizeof *k);
      k->i = i;
      k->a = malloc(24);
      snprintf(k->a, 24, "%10lld", (long long)i);
      k->c = replicate('k', 64);
      kept[nkept++] = k;
    }
  }
  for (int64_t k = 0; k < 40000; ++k) {
    if (slots[k].kind == 1) check += (int64_t)strlen(slots[k].s);
    if (slots[k].kind == 2) check += (int64_t)(strlen(slots[k].r->a) + strlen(slots[k].r->c));
  }
  bench_report("gc", t, nkept == 20000 && check > 0);
  return 0;
}
