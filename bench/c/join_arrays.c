/* bench/join_arrays.prg in C: 20,000 customers { id, name, segment, city } and 500,000 orders { id, customer, amount }
 * made from a fixed seed. Timed: a hash join (the customers in a hash table by id, each order looked up; amounts summed
 * per segment, orders counted per city in a hash table of string keys), then a sort-merge join (the orders' customer
 * and position packed in one integer key, sorted with the library's sort, merged with the customers in id order; the
 * amounts weighted by segment). */
#include "bench.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  int64_t id;
  char name[24];
  int64_t segment;
  char city[8];
} Customer;

typedef struct {
  int64_t id, customer, amount;
} Order;

typedef struct {
  int64_t key;
  Customer *value; /* NULL for a free slot */
} IdSlot;

typedef struct {
  const char *key; /* NULL for a free slot */
  int64_t value;
} CitySlot;

static int64_t seed = 2718;
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

static uint64_t hash_str(const char *s) {
  uint64_t h = 14695981039346656037ULL;
  for (; *s; ++s) h = (h ^ (unsigned char)*s) * 1099511628211ULL;
  return h;
}

static IdSlot *id_find(IdSlot *slots, int64_t cap, int64_t key) {
  uint64_t i = mix((uint64_t)key) & (uint64_t)(cap - 1);
  while (slots[i].value && slots[i].key != key) i = (i + 1) & (uint64_t)(cap - 1);
  return &slots[i];
}

static void id_set(IdSlot **slots, int64_t *count, int64_t *cap, int64_t key, Customer *c) {
  if ((*count + 1) * 4 > *cap * 3) {
    IdSlot *old = *slots, *s = calloc((size_t)*cap * 2, sizeof *s);
    for (int64_t i = 0; i < *cap; ++i)
      if (old[i].value) *id_find(s, *cap * 2, old[i].key) = old[i];
    free(old);
    *slots = s;
    *cap *= 2;
  }
  IdSlot *s = id_find(*slots, *cap, key);
  if (!s->value) ++*count;
  *s = (IdSlot){key, c};
}

static CitySlot *city_find(CitySlot *slots, int64_t cap, const char *key) {
  uint64_t i = hash_str(key) & (uint64_t)(cap - 1);
  while (slots[i].key && strcmp(slots[i].key, key) != 0) i = (i + 1) & (uint64_t)(cap - 1);
  return &slots[i];
}

static void city_set(CitySlot **slots, int64_t *count, int64_t *cap, const char *key, int64_t value) {
  if ((*count + 1) * 4 > *cap * 3) {
    CitySlot *old = *slots, *s = calloc((size_t)*cap * 2, sizeof *s);
    for (int64_t i = 0; i < *cap; ++i)
      if (old[i].key) *city_find(s, *cap * 2, old[i].key) = old[i];
    free(old);
    *slots = s;
    *cap *= 2;
  }
  CitySlot *s = city_find(*slots, *cap, key);
  if (!s->key) ++*count;
  *s = (CitySlot){key, value};
}

static int by_value(const void *p, const void *q) {
  int64_t a = *(const int64_t *)p, b = *(const int64_t *)q;
  return (a > b) - (a < b);
}

int main(void) {
  int64_t ncust = 20000, nord = 500000, seg[5] = {0}, matched = 0, w = 0, merged = 0, best = 0;
  Customer *cust = malloc((size_t)ncust * sizeof *cust);
  Order *ord = malloc((size_t)nord * sizeof *ord);
  const char *best_city = NULL;
  for (int64_t i = 1; i <= ncust; ++i) {
    Customer *c = &cust[i - 1];
    c->id = i * 3;
    snprintf(c->name, sizeof c->name, "Customer %lld", (long long)i);
    c->segment = rnd(5) + 1;
    snprintf(c->city, sizeof c->city, "CITY%03d", (int)(rnd(100) + 1));
  }
  for (int64_t i = 1; i <= nord; ++i) {
    ord[i - 1].id = i;
    ord[i - 1].customer = rnd(60000) + 1;
    ord[i - 1].amount = rnd(100000) + 1;
  }
  long long t = bench_ms();
  int64_t idcount = 0, idcap = 8, citycount = 0, citycap = 8;
  IdSlot *ids = calloc((size_t)idcap, sizeof *ids);
  CitySlot *cities = calloc((size_t)citycap, sizeof *cities);
  for (int64_t i = 0; i < ncust; ++i) id_set(&ids, &idcount, &idcap, cust[i].id, &cust[i]);
  for (int64_t i = 0; i < nord; ++i) {
    IdSlot *s = id_find(ids, idcap, ord[i].customer);
    if (s->value) {
      Customer *c = s->value;
      seg[c->segment - 1] += ord[i].amount;
      CitySlot *cs = city_find(cities, citycap, c->city);
      city_set(&cities, &citycount, &citycap, c->city, (cs->key ? cs->value : 0) + 1);
      ++matched;
    }
  }
  for (int64_t i = 0; i < citycap; ++i) {
    const char *k = cities[i].key;
    if (k && (cities[i].value > best || (cities[i].value == best && strcmp(k, best_city) < 0))) {
      best_city = k;
      best = cities[i].value;
    }
  }
  int64_t *keys = malloc((size_t)nord * sizeof *keys), pos = 1;
  for (int64_t i = 1; i <= nord; ++i) keys[i - 1] = ord[i - 1].customer * 1000000 + i;
  qsort(keys, (size_t)nord, sizeof *keys, by_value);
  for (int64_t i = 0; i < nord; ++i) {
    Order *o = &ord[keys[i] % 1000000 - 1];
    while (pos <= ncust && cust[pos - 1].id < o->customer) ++pos;
    if (pos <= ncust && cust[pos - 1].id == o->customer) {
      w += o->amount * cust[pos - 1].segment;
      ++merged;
    }
  }
  int ok = matched == 166641 && merged == 166641 && w == 25122891088LL && seg[0] == 1626943956 &&
           seg[1] == 1648544230 && seg[2] == 1673211091 && seg[3] == 1694374411 && seg[4] == 1680345551 &&
           best_city && strcmp(best_city, "CITY080") == 0 && best == 1980 && citycount == 100;
  bench_report("join_arrays", t, ok);
  free(keys);
  free(ids);
  free(cities);
  free(cust);
  free(ord);
  return 0;
}
