/* bench/higher_order_chain.prg in C: chains of lazy iterators over an array of 500,000 numbers, each step an iterator
 * (a struct with its state and a next function) fed by the one before, its test or map a closure (a function pointer
 * and the captured round): where, select, skip, take-while and sum; select with the position, where, take and count;
 * distinct with a key (the keys seen in a hash table) and count; 5 rounds. */
#include "bench.h"
#include <stdlib.h>

typedef struct Iter Iter;
struct Iter {
  int (*next)(Iter *self, int64_t *out);
};

typedef struct {
  int64_t (*eval)(int64_t r, int64_t x, int64_t pos);
  int64_t r;
} Block;

typedef struct {
  Iter it;
  const int64_t *items;
  int64_t len, at;
} ArrayIter;

typedef struct {
  Iter it;
  Iter *src;
  Block b;
  int64_t n;
} BlockIter; /* select, where, take-while */

typedef struct {
  Iter it;
  Iter *src;
  int64_t count, n;
} CountIter; /* skip, take */

typedef struct {
  int64_t *keys;
  unsigned char *used;
  int64_t count, cap;
} Set;

typedef struct {
  Iter it;
  Iter *src;
  Block key;
  Set seen;
} DistinctIter;

static int array_next(Iter *self, int64_t *out) {
  ArrayIter *a = (ArrayIter *)self;
  if (a->at >= a->len) return 0;
  *out = a->items[a->at++];
  return 1;
}

static int select_next(Iter *self, int64_t *out) {
  BlockIter *s = (BlockIter *)self;
  int64_t x;
  if (!s->src->next(s->src, &x)) return 0;
  *out = s->b.eval(s->b.r, x, ++s->n);
  return 1;
}

static int where_next(Iter *self, int64_t *out) {
  BlockIter *s = (BlockIter *)self;
  int64_t x;
  while (s->src->next(s->src, &x))
    if (s->b.eval(s->b.r, x, ++s->n)) {
      *out = x;
      return 1;
    }
  return 0;
}

static int take_while_next(Iter *self, int64_t *out) {
  BlockIter *s = (BlockIter *)self;
  int64_t x;
  if (s->n < 0 || !s->src->next(s->src, &x)) return 0;
  if (!s->b.eval(s->b.r, x, ++s->n)) {
    s->n = -1; /* done */
    return 0;
  }
  *out = x;
  return 1;
}

static int skip_next(Iter *self, int64_t *out) {
  CountIter *s = (CountIter *)self;
  int64_t x;
  while (s->src->next(s->src, &x))
    if (++s->n > s->count) {
      *out = x;
      return 1;
    }
  return 0;
}

static int take_next(Iter *self, int64_t *out) {
  CountIter *s = (CountIter *)self;
  if (s->n >= s->count || !s->src->next(s->src, out)) return 0;
  ++s->n;
  return 1;
}

static uint64_t mix(uint64_t k) {
  k ^= k >> 33;
  k *= 0xff51afd7ed558ccdULL;
  k ^= k >> 33;
  return k;
}

/* Whether key was added (0 when it was there already). */
static int set_add(Set *s, int64_t key) {
  if ((s->count + 1) * 4 > s->cap * 3) {
    Set g = {calloc((size_t)s->cap * 2, sizeof *g.keys), calloc((size_t)s->cap * 2, 1), 0, s->cap * 2};
    for (int64_t i = 0; i < s->cap; ++i)
      if (s->used[i]) set_add(&g, s->keys[i]);
    free(s->keys);
    free(s->used);
    *s = g;
  }
  uint64_t i = mix((uint64_t)key) & (uint64_t)(s->cap - 1);
  while (s->used[i]) {
    if (s->keys[i] == key) return 0;
    i = (i + 1) & (uint64_t)(s->cap - 1);
  }
  s->used[i] = 1;
  s->keys[i] = key;
  ++s->count;
  return 1;
}

static int distinct_next(Iter *self, int64_t *out) {
  DistinctIter *s = (DistinctIter *)self;
  int64_t x;
  while (s->src->next(s->src, &x))
    if (set_add(&s->seen, s->key.eval(s->key.r, x, 0))) {
      *out = x;
      return 1;
    }
  return 0;
}

static int64_t sum(Iter *it) {
  int64_t x, n = 0;
  while (it->next(it, &x)) n += x;
  return n;
}

static int64_t count(Iter *it) {
  int64_t x, n = 0;
  while (it->next(it, &x)) ++n;
  return n;
}

static int64_t not_third(int64_t r, int64_t x, int64_t pos) {
  (void)r;
  (void)pos;
  return x % 3 != 0;
}
static int64_t times_round(int64_t r, int64_t x, int64_t pos) {
  (void)pos;
  return x * r;
}
static int64_t below(int64_t r, int64_t x, int64_t pos) {
  (void)r;
  (void)pos;
  return x < 4900;
}
static int64_t plus_pos(int64_t r, int64_t x, int64_t pos) {
  (void)r;
  return x + pos % 7;
}
static int64_t even(int64_t r, int64_t x, int64_t pos) {
  (void)r;
  (void)pos;
  return x % 2 == 0;
}
static int64_t key_of(int64_t r, int64_t x, int64_t pos) {
  (void)pos;
  return x % (100 * r);
}

static Iter *array_iter(const int64_t *a, int64_t len) {
  ArrayIter *it = malloc(sizeof *it);
  *it = (ArrayIter){{array_next}, a, len, 0};
  return &it->it;
}

static Iter *block_iter(int (*next)(Iter *, int64_t *), Iter *src, int64_t (*eval)(int64_t, int64_t, int64_t),
                        int64_t r) {
  BlockIter *it = malloc(sizeof *it);
  *it = (BlockIter){{next}, src, {eval, r}, 0};
  return &it->it;
}

static Iter *count_iter(int (*next)(Iter *, int64_t *), Iter *src, int64_t n) {
  CountIter *it = malloc(sizeof *it);
  *it = (CountIter){{next}, src, n, 0};
  return &it->it;
}

static Iter *distinct_iter(Iter *src, int64_t (*eval)(int64_t, int64_t, int64_t), int64_t r) {
  DistinctIter *it = malloc(sizeof *it);
  *it = (DistinctIter){{distinct_next}, src, {eval, r}, {calloc(8, sizeof(int64_t)), calloc(8, 1), 0, 8}};
  return &it->it;
}

/* The chain from its last iterator down to the array, each freed. */
static void free_chain(Iter *it, int depth) {
  for (int i = 0; i < depth; ++i) {
    Iter *src = NULL;
    if (it->next == select_next || it->next == where_next || it->next == take_while_next)
      src = ((BlockIter *)it)->src;
    else if (it->next == skip_next || it->next == take_next)
      src = ((CountIter *)it)->src;
    else if (it->next == distinct_next) {
      src = ((DistinctIter *)it)->src;
      free(((DistinctIter *)it)->seen.keys);
      free(((DistinctIter *)it)->seen.used);
    }
    free(it);
    it = src;
  }
}

int main(void) {
  int64_t len = 500000, n = 0;
  int64_t *a = malloc((size_t)len * sizeof *a);
  for (int64_t i = 1; i <= len; ++i) a[i - 1] = i * 37 % 1000;
  long long t = bench_ms();
  for (int64_t r = 1; r <= 5; ++r) {
    Iter *volatile c1 = block_iter(
        take_while_next,
        count_iter(skip_next, block_iter(select_next, block_iter(where_next, array_iter(a, len), not_third, r),
                                         times_round, r),
                   10),
        below, r);
    n += sum(c1);
    free_chain(c1, 5);
    Iter *volatile c2 = count_iter(
        take_next, block_iter(where_next, block_iter(select_next, array_iter(a, len), plus_pos, r), even, r),
        50000 + r);
    n += count(c2);
    free_chain(c2, 4);
    Iter *volatile c3 = distinct_iter(array_iter(a, len), key_of, r);
    n += count(c3);
    free_chain(c3, 2);
  }
  bench_report("higher_order_chain", t, n == 1663633050);
  free(a);
  return 0;
}
