/* bench/hash_resize.prg in C: 20 rounds of 100,000 keys added to an empty hash table (open addressing, its store
 * doubling from 8 slots) and deleted again (each deletion moving back the entries after it), the even ones first, then
 * the odd ones, the store halving as it empties. */
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

static void resize(Hash *h, int64_t cap) {
  Slot *old = h->slots;
  int64_t n = h->cap;
  h->cap = cap;
  h->slots = calloc((size_t)cap, sizeof *h->slots);
  for (int64_t i = 0; i < n; ++i)
    if (old[i].used) *find(h, old[i].key) = old[i];
  free(old);
}

static void set(Hash *h, int64_t key, int64_t value) {
  if ((h->count + 1) * 4 > h->cap * 3) resize(h, h->cap * 2);
  Slot *s = find(h, key);
  if (!s->used) {
    *s = (Slot){key, 0, 1};
    ++h->count;
  }
  s->value = value;
}

/* Linear probing's deletion: the entries after the hole that may move back are moved into it. */
static void del(Hash *h, int64_t key) {
  uint64_t mask = (uint64_t)(h->cap - 1), i = (uint64_t)(find(h, key) - h->slots);
  if (!h->slots[i].used) return;
  h->slots[i].used = 0;
  --h->count;
  for (uint64_t j = (i + 1) & mask; h->slots[j].used; j = (j + 1) & mask) {
    uint64_t home = mix((uint64_t)h->slots[j].key) & mask;
    if (((j - home) & mask) >= ((j - i) & mask)) {
      h->slots[i] = h->slots[j];
      h->slots[j].used = 0;
      i = j;
    }
  }
  if (h->cap > 8 && h->count * 4 < h->cap) resize(h, h->cap / 2);
}

int main(void) {
  long long t = bench_ms();
  int64_t sum = 0;
  int ok = 1;
  for (int64_t r = 1; r <= 20; ++r) {
    Hash h = {calloc(8, sizeof(Slot)), 0, 8};
    for (int64_t i = 1; i <= 100000; ++i) set(&h, i * 7 + r, i);
    ok = ok && h.count == 100000;
    for (int64_t i = 2; i <= 100000; i += 2) {
      sum += find(&h, i * 7 + r)->value;
      del(&h, i * 7 + r);
    }
    ok = ok && h.count == 50000;
    for (int64_t i = 1; i <= 100000; i += 2) {
      sum += find(&h, i * 7 + r)->value;
      del(&h, i * 7 + r);
    }
    ok = ok && h.count == 0;
    free(h.slots);
  }
  bench_report("hash_resize", t, ok && sum == 100001000000LL);
  return 0;
}
