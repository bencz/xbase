/* bench/hash_chaves_longas.prg in C: 300,000 keys of 82 to 131 bytes that share a 66-byte prefix inserted in a hash
 * table of string keys (open addressing, the whole key hashed and, on equal hashes, compared), then looked up through
 * other strings of the same text, and 300,000 missing keys of the same shape looked up. */
#include "bench.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  const char *key; /* NULL for a free slot */
  uint64_t hash;
  int64_t value;
} Slot;

typedef struct {
  Slot *slots;
  int64_t count, cap;
} Hash;

static int64_t seed = 99;
static int64_t rnd(int64_t n) {
  seed = seed * 16807 % 2147483647;
  return seed % n;
}

static uint64_t hash_str(const char *s) {
  uint64_t h = 14695981039346656037ULL;
  for (; *s; ++s) h = (h ^ (unsigned char)*s) * 1099511628211ULL;
  return h;
}

static Slot *find(const Hash *h, const char *key, uint64_t hv) {
  uint64_t i = hv & (uint64_t)(h->cap - 1);
  while (h->slots[i].key && !(h->slots[i].hash == hv && strcmp(h->slots[i].key, key) == 0))
    i = (i + 1) & (uint64_t)(h->cap - 1);
  return &h->slots[i];
}

static void grow(Hash *h) {
  Slot *old = h->slots;
  int64_t n = h->cap;
  h->cap = n ? n * 2 : 8;
  h->slots = calloc((size_t)h->cap, sizeof *h->slots);
  for (int64_t i = 0; i < n; ++i)
    if (old[i].key) *find(h, old[i].key, old[i].hash) = old[i];
  free(old);
}

static void set(Hash *h, const char *key, int64_t value) {
  if ((h->count + 1) * 4 > h->cap * 3) grow(h);
  uint64_t hv = hash_str(key);
  Slot *s = find(h, key, hv);
  if (!s->key) {
    *s = (Slot){key, hv, 0};
    ++h->count;
  }
  s->value = value;
}

static Slot *get(const Hash *h, const char *key) {
  Slot *s = find(h, key, hash_str(key));
  return s->key ? s : NULL;
}

static char *make_key(const char *prefix, int64_t i, const char *v, int64_t n) {
  char *s = malloc(160);
  int len = snprintf(s, 160, "%s%07lld&%s=%06lld", prefix, (long long)i, v, (long long)n);
  memset(s + len, 'x', (size_t)(i % 50));
  s[len + i % 50] = '\0';
  return s;
}

int main(void) {
  const char *prefix = "https://example.com/catalog/products/category/electronics/item?id=";
  int64_t len = 300000, sum = 0, miss = 0;
  char **keys = malloc((size_t)len * sizeof *keys), **same = malloc((size_t)len * sizeof *same),
       **missing = malloc((size_t)len * sizeof *missing);
  Hash h = {0};
  for (int64_t i = 1; i <= len; ++i) {
    int64_t n = rnd(1000000);
    keys[i - 1] = make_key(prefix, i, "v", n);
    same[i - 1] = make_key(prefix, i, "v", n);
    missing[i - 1] = make_key(prefix, i, "w", n);
  }
  long long t = bench_ms();
  for (int64_t i = 0; i < len; ++i) set(&h, keys[i], i + 1);
  for (int64_t i = len; i >= 1; --i) sum += get(&h, same[i - 1])->value;
  for (int64_t i = 0; i < len; ++i)
    if (!get(&h, missing[i])) ++miss;
  int ok = h.count == 300000 && sum == 45000150000LL && miss == 300000;
  bench_report("hash_chaves_longas", t, ok);
  for (int64_t i = 0; i < len; ++i) {
    free(keys[i]);
    free(same[i]);
    free(missing[i]);
  }
  free(keys);
  free(same);
  free(missing);
  free(h.slots);
  return 0;
}
