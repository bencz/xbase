/* bench/aeval_heval.prg in C: a walk over an array of 1,000,000 numbers and one over a hash of 200,000 entries (an
 * open-addressing table that keeps its entries in insertion order), each calling a block (a function pointer with its
 * captured variables in a struct) that adds to a captured local; 20 rounds. */
#include "bench.h"
#include <stdlib.h>

typedef struct {
  int64_t *n, r;
} Env;

typedef void (*ArrayBlock)(Env *env, int64_t x, int64_t pos);
typedef void (*HashBlock)(Env *env, int64_t k, int64_t v);

typedef struct {
  int64_t key, value;
} Entry;

typedef struct {
  Entry *entries;
  int64_t *index; /* entry number plus 1; 0 for a free slot */
  int64_t count, cap, slots;
} Hash;

static uint64_t mix(uint64_t k) {
  k ^= k >> 33;
  k *= 0xff51afd7ed558ccdULL;
  k ^= k >> 33;
  return k;
}

static void hash_rehash(Hash *h, int64_t slots) {
  free(h->index);
  h->slots = slots;
  h->index = calloc((size_t)slots, sizeof *h->index);
  for (int64_t e = 0; e < h->count; ++e) {
    uint64_t i = mix((uint64_t)h->entries[e].key) & (uint64_t)(slots - 1);
    while (h->index[i]) i = (i + 1) & (uint64_t)(slots - 1);
    h->index[i] = e + 1;
  }
}

static void hash_set(Hash *h, int64_t key, int64_t value) {
  if ((h->count + 1) * 4 > h->slots * 3) hash_rehash(h, h->slots ? h->slots * 2 : 8);
  uint64_t i = mix((uint64_t)key) & (uint64_t)(h->slots - 1);
  while (h->index[i]) {
    Entry *e = &h->entries[h->index[i] - 1];
    if (e->key == key) {
      e->value = value;
      return;
    }
    i = (i + 1) & (uint64_t)(h->slots - 1);
  }
  if (h->count == h->cap) {
    h->cap = h->cap ? h->cap * 2 : 8;
    h->entries = realloc(h->entries, (size_t)h->cap * sizeof *h->entries);
  }
  h->entries[h->count] = (Entry){key, value};
  h->index[i] = ++h->count;
}

static void aeval(const int64_t *a, int64_t n, ArrayBlock b, Env *env) {
  for (int64_t i = 0; i < n; ++i) b(env, a[i], i + 1);
}

static void heval(const Hash *h, HashBlock b, Env *env) {
  for (int64_t i = 0; i < h->count; ++i) b(env, h->entries[i].key, h->entries[i].value);
}

static void add_item(Env *env, int64_t x, int64_t pos) { *env->n += x + (pos % 1000 == 0 ? env->r : 0); }
static void add_entry(Env *env, int64_t k, int64_t v) { *env->n += v + (k % 3000 == 0 ? 1 : 0); }

int main(void) {
  int64_t len = 1000000, n = 0;
  int64_t *a = malloc((size_t)len * sizeof *a);
  Hash h = {0};
  for (int64_t i = 1; i <= len; ++i) a[i - 1] = i % 777;
  for (int64_t i = 1; i <= 200000; ++i) hash_set(&h, i * 3, i % 555);
  ArrayBlock volatile ab = add_item;
  HashBlock volatile hb = add_entry;
  long long t = bench_ms();
  for (int64_t r = 1; r <= 20; ++r) {
    Env env = {&n, r};
    aeval(a, len, ab, &env);
    heval(&h, hb, &env);
  }
  int ok = n == 8867500260LL;
  bench_report("aeval_heval", t, ok);
  free(a);
  free(h.entries);
  free(h.index);
  return 0;
}
