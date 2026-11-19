/* bench/hash_massivo.prg in C: 1,000,000 numeric keys drawn from a fixed-seed generator counted in one hash table
 * (open addressing, about 632,000 distinct), then 1,500,000 lookups of keys drawn from twice the range. */
#include "bench.h"
#include <stdlib.h>

typedef struct {
  int64_t key, value;
  int used;
} Slot;

typedef struct {
  Slot *slots;
  int64_t count, cap;
} Hash;

static int64_t seed = 2024;
static int64_t rnd(int64_t n) {
  seed = seed * 16807 % 2147483647;
  return seed % n;
}

static uint64_t mix(uint64_t k) {
  k ^= k >> 33;
  k *= 0xff51afd7ed558ccdULL;
  k ^= k >> 33;
  return k;
}

static Slot *find(const Hash *h, int64_t key) {
  uint64_t i = mix((uint64_t)key) & (uint64_t)(h->cap - 1);
  while (h->slots[i].used && h->slots[i].key != key) i = (i + 1) & (uint64_t)(h->cap - 1);
  return &h->slots[i];
}

static void grow(Hash *h) {
  Slot *old = h->slots;
  int64_t n = h->cap;
  h->cap = n * 2;
  h->slots = calloc((size_t)h->cap, sizeof *h->slots);
  for (int64_t i = 0; i < n; ++i)
    if (old[i].used) *find(h, old[i].key) = old[i];
  free(old);
}

static int has(const Hash *h, int64_t key) { return find(h, key)->used; }

static Slot *set(Hash *h, int64_t key, int64_t value) {
  if ((h->count + 1) * 4 > h->cap * 3) grow(h);
  Slot *s = find(h, key);
  if (!s->used) {
    *s = (Slot){key, 0, 1};
    ++h->count;
  }
  s->value = value;
  return s;
}

int main(void) {
  long long t = bench_ms();
  Hash h = {calloc(8, sizeof(Slot)), 0, 8};
  int64_t hits = 0, sum = 0;
  for (int64_t i = 1; i <= 1000000; ++i) {
    int64_t k = rnd(1000000);
    if (has(&h, k))
      ++find(&h, k)->value;
    else
      set(&h, k, 1);
  }
  for (int64_t i = 1; i <= 1500000; ++i) {
    int64_t k = rnd(2000000);
    if (has(&h, k)) {
      ++hits;
      sum += find(&h, k)->value;
    }
  }
  int ok = h.count == 631967 && hits == 473472 && sum == 748930;
  bench_report("hash_massivo", t, ok);
  free(h.slots);
  return 0;
}
