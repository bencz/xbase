/* bench/set_operations.prg in C: sets as hash tables of numbers (each key its own value; open addressing over entries
 * kept in insertion order, which a walk follows): two sets of 200,000 draws each from 0 .. 399,999, their union,
 * intersection, difference and symmetric difference built as new sets; 2 rounds with new members. */
#include "bench.h"
#include <stdlib.h>

typedef struct {
  int64_t key, value;
} Entry;

typedef struct {
  Entry *entries;
  int64_t *index; /* entry number plus 1; 0 for a free slot */
  int64_t count, slots;
} Hash;

static int64_t seed = 777;
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

static int64_t *slot_of(const Hash *h, int64_t key) {
  uint64_t i = mix((uint64_t)key) & (uint64_t)(h->slots - 1);
  while (h->index[i] && h->entries[h->index[i] - 1].key != key) i = (i + 1) & (uint64_t)(h->slots - 1);
  return &h->index[i];
}

static Hash hash_new(void) { return (Hash){malloc(6 * sizeof(Entry)), calloc(8, sizeof(int64_t)), 0, 8}; }

static void hash_free(Hash *h) {
  free(h->entries);
  free(h->index);
}

static int has(const Hash *h, int64_t key) { return *slot_of(h, key) != 0; }

static void set(Hash *h, int64_t key, int64_t value) {
  if ((h->count + 1) * 4 > h->slots * 3) {
    free(h->index);
    h->slots *= 2;
    h->index = calloc((size_t)h->slots, sizeof *h->index);
    h->entries = realloc(h->entries, (size_t)(h->slots / 4 * 3) * sizeof *h->entries);
    for (int64_t e = 0; e < h->count; ++e) *slot_of(h, h->entries[e].key) = e + 1;
  }
  int64_t *s = slot_of(h, key);
  if (*s)
    h->entries[*s - 1].value = value;
  else {
    h->entries[h->count] = (Entry){key, value};
    *s = ++h->count;
  }
}

int main(void) {
  long long t = bench_ms();
  int64_t check = 0, nunion = 0, ninter = 0, nsym = 0, na = 0, ndiff = 0;
  for (int r = 1; r <= 2; ++r) {
    Hash a = hash_new(), b = hash_new(), uni = hash_new(), inter = hash_new(), diff = hash_new(), sym = hash_new();
    for (int i = 1; i <= 200000; ++i) {
      int64_t k = rnd(400000);
      set(&a, k, k);
      k = rnd(400000);
      set(&b, k, k);
    }
    for (int64_t i = 0; i < a.count; ++i) {
      int64_t k = a.entries[i].value;
      set(&uni, k, k);
      if (has(&b, k))
        set(&inter, k, k);
      else {
        set(&diff, k, k);
        set(&sym, k, k);
      }
    }
    for (int64_t i = 0; i < b.count; ++i) {
      int64_t k = b.entries[i].value;
      set(&uni, k, k);
      if (!has(&a, k)) set(&sym, k, k);
    }
    check += a.count + b.count * 2 + uni.count * 3 + inter.count * 5 + diff.count * 7 + sym.count * 11;
    nunion = uni.count;
    ninter = inter.count;
    nsym = sym.count;
    na = a.count;
    ndiff = diff.count;
    hash_free(&a);
    hash_free(&b);
    hash_free(&uni);
    hash_free(&inter);
    hash_free(&diff);
    hash_free(&sym);
  }
  int ok = check == 8609885 && nunion == 252664 && nunion == ninter + nsym && na == ninter + ndiff;
  bench_report("set_operations", t, ok);
  return 0;
}
