/* bench/object_churn.prg in C: 50,000 slots holding objects of three classes (structs with a table of methods) that
 * point to other slots' objects (cycles included); 700,000 times a slot's object is replaced by a new one, then the
 * graph is walked. With no collector, a replaced object may still be pointed to: every object is kept in a list and
 * freed at the end. */
#include "bench.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Leaf Leaf;
typedef struct {
  int64_t (*weight)(const Leaf *self);
} Methods;

struct Leaf {
  const Methods *m;
  int64_t value;
};

typedef struct {
  Leaf leaf;
  Leaf *left, *right;
} Pair;

typedef struct {
  Leaf *o;
  int64_t n;
  char *s;
} Items; /* { o, n, Str( n ) }: n is i % 100, a number with 2 decimals (SET DECIMALS), so Str() gives 13 characters */

typedef struct {
  Leaf leaf;
  Items *items;
} List;

static int64_t seed = 9;
static int64_t rnd(int64_t n) {
  seed = seed * 16807 % 2147483647;
  return seed % n;
}

static int64_t leaf_weight(const Leaf *self) { return self->value; }
static int64_t pair_weight(const Leaf *self) {
  const Pair *p = (const Pair *)self;
  return self->value + p->left->value + p->right->value;
}
static int64_t list_weight(const Leaf *self) {
  const List *l = (const List *)self;
  return self->value + l->items->o->value + (int64_t)strlen(l->items->s);
}
static const Methods leaf_methods = {leaf_weight}, pair_methods = {pair_weight}, list_methods = {list_weight};

static void **all;
static int64_t nall, capall;

static void *track(void *p) {
  if (nall == capall) {
    capall = capall ? capall * 2 : 1024;
    all = realloc(all, (size_t)capall * sizeof *all);
  }
  return all[nall++] = p;
}

static Leaf *new_leaf(int64_t n) {
  Leaf *o = track(malloc(sizeof *o));
  *o = (Leaf){&leaf_methods, n};
  return o;
}

static Leaf *new_pair(int64_t n, Leaf *l, Leaf *r) {
  Pair *o = track(malloc(sizeof *o));
  *o = (Pair){{&pair_methods, n}, l, r};
  return &o->leaf;
}

static Leaf *new_list(int64_t n, Leaf *x) {
  List *o = track(malloc(sizeof *o));
  Items *items = track(malloc(sizeof *items));
  char *s = track(malloc(24));
  snprintf(s, 24, "%13.2f", (double)n);
  *items = (Items){x, n, s};
  *o = (List){{&list_methods, n}, items};
  return &o->leaf;
}

int main(void) {
  long long t = bench_ms();
  int64_t len = 50000, n = 0;
  Leaf **slots = malloc((size_t)len * sizeof *slots);
  for (int64_t i = 1; i <= len; ++i) slots[i - 1] = new_leaf(i);
  for (int64_t i = 1; i <= 700000; ++i) {
    int64_t k = rnd(len), kind = rnd(3);
    Leaf *o;
    if (kind == 0)
      o = new_leaf(i % 100);
    else if (kind == 1) {
      Leaf *l = slots[rnd(len)];
      o = new_pair(i % 100, l, slots[rnd(len)]);
    } else
      o = new_list(i % 100, slots[rnd(len)]);
    slots[k] = o;
  }
  for (int64_t i = 0; i < len; ++i) n += slots[i]->m->weight(slots[i]);
  bench_report("object_churn", t, n == 5162688);
  for (int64_t i = 0; i < nall; ++i) free(all[i]);
  free(all);
  free(slots);
  return 0;
}
