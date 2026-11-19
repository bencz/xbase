/* bench/array_of_hashes.prg in C: 120,000 records, each a small hash table of its own from field name to value (string
 * keys hashed and compared on every read, as a hash's are), made from a fixed-seed generator; totals per group read by
 * field name, a count of the records that pass a test, and the array sorted by a field with the library's sort and a
 * comparison function that reads the field through the hash. */
#include "bench.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SLOTS 8

typedef struct {
  const char *key; /* NULL for a free slot */
  int64_t num;
  char *str;
} Slot;

typedef struct {
  Slot slots[SLOTS];
} Record;

static int64_t seed = 31337;
static int64_t rnd(int64_t n) {
  seed = seed * 16807 % 2147483647;
  return seed % n;
}

/* noinline and noipa: the field names are hashed on every read, not folded into constants. */
__attribute__((noinline, noipa)) static uint64_t hash_str(const char *s) {
  uint64_t h = 14695981039346656037ULL;
  for (; *s; ++s) h = (h ^ (unsigned char)*s) * 1099511628211ULL;
  return h;
}

static Slot *find(Record *r, const char *key) {
  uint64_t i = hash_str(key) & (SLOTS - 1);
  while (r->slots[i].key && strcmp(r->slots[i].key, key) != 0) i = (i + 1) & (SLOTS - 1);
  return &r->slots[i];
}

static void set_num(Record *r, const char *key, int64_t v) {
  Slot *s = find(r, key);
  s->key = key;
  s->num = v;
}

static void set_str(Record *r, const char *key, char *v) {
  Slot *s = find(r, key);
  s->key = key;
  s->str = v;
}

static int64_t num(Record *r, const char *key) { return find(r, key)->num; }

static int by_score(const void *p, const void *q) {
  int64_t a = num(*(Record *const *)p, "score"), b = num(*(Record *const *)q, "score");
  return (a > b) - (a < b);
}

int main(void) {
  long long t = bench_ms();
  int64_t n = 120000, total[50] = {0}, pass = 0, sum = 0;
  Record **a = malloc((size_t)n * sizeof *a);
  for (int64_t i = 0; i < n; ++i) {
    Record *r = calloc(1, sizeof *r);
    char *name = malloc(8);
    set_num(r, "id", i + 1);
    snprintf(name, 8, "N%05lld", (long long)rnd(100000));
    set_str(r, "name", name);
    set_num(r, "score", rnd(1000));
    set_num(r, "group", rnd(50) + 1);
    a[i] = r;
  }
  for (int64_t i = 0; i < n; ++i) {
    Record *r = a[i];
    total[num(r, "group") - 1] += num(r, "score");
    if (num(r, "score") >= 500 && strncmp(find(r, "name")->str, "N1", 2) == 0) ++pass;
  }
  qsort(a, (size_t)n, sizeof *a, by_score);
  for (int64_t i = 0; i < n; i += 1000) sum += num(a[i], "score");
  int ok = total[0] == 1239678 && total[49] == 1170281 && pass == 5950 && sum == 59429 && num(a[0], "score") == 0 &&
           num(a[n - 1], "score") == 999;
  bench_report("array_of_hashes", t, ok);
  for (int64_t i = 0; i < n; ++i) {
    free(find(a[i], "name")->str);
    free(a[i]);
  }
  free(a);
  return 0;
}
