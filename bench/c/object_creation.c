/* bench/object_creation.prg in C: 600,000 objects (structs with a table of methods) of a class with five variables
 * (initial values: numbers, a string and an empty array, which each object gets a copy of) made by a constructor that
 * calls an Init() method setting two of them, half of them of a subclass with two more variables; one in 100 kept in
 * an array, the others freed at once. */
#include "bench.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Order Order;
typedef struct {
  Order *(*init)(Order *self, int64_t id, const char *customer);
} Methods;

struct Order {
  const Methods *m;
  int64_t id;
  char *customer;
  int64_t total;
  const char *status;
  int64_t *lines, nlines, caplines;
};

typedef struct {
  Order order;
  int64_t discount, due; /* due: days since 1970 */
} BigOrder;

static char *dup(const char *s) {
  size_t n = strlen(s) + 1;
  return memcpy(malloc(n), s, n);
}

/* Called through the class's table from the constructor; noinline and noipa keep that call, which the compiler
 * could otherwise resolve from the table it just stored. */
__attribute__((noinline, noipa)) static Order *order_init(Order *self, int64_t id, const char *customer) {
  self->id = id;
  free(self->customer);
  self->customer = dup(customer);
  return self;
}
static const Methods order_methods = {order_init}, big_order_methods = {order_init};

static void init_vars(Order *o, const Methods *m) {
  *o = (Order){m, 0, dup(""), 0, "new", malloc(4 * sizeof(int64_t)), 0, 4};
}

static Order *new_order(int64_t id, const char *customer) {
  Order *o = malloc(sizeof *o);
  init_vars(o, &order_methods);
  return o->m->init(o, id, customer);
}

static Order *new_big_order(int64_t id, const char *customer, int64_t due) {
  BigOrder *o = malloc(sizeof *o);
  init_vars(&o->order, &big_order_methods);
  o->discount = 0;
  o->due = due;
  return o->order.m->init(&o->order, id, customer);
}

static void add_line(Order *o, int64_t x) {
  if (o->nlines == o->caplines) {
    o->caplines *= 2;
    o->lines = realloc(o->lines, (size_t)o->caplines * sizeof *o->lines);
  }
  o->lines[o->nlines++] = x;
}

static void free_order(Order *o) {
  free(o->customer);
  free(o->lines);
  free(o);
}

int main(void) {
  long long t = bench_ms();
  Order **keep = malloc(6000 * sizeof *keep);
  int64_t nkeep = 0, n = 0, due = 20818; /* 2026-12-31 */
  char customer[16];
  for (int64_t i = 1; i <= 600000; ++i) {
    Order *o;
    if (i % 2 == 0) {
      snprintf(customer, sizeof customer, "c%03d", (int)(i % 1000));
      o = new_order(i, customer);
    } else {
      o = new_big_order(i, "c000", due);
      ((BigOrder *)o)->discount = 5;
    }
    add_line(o, i % 7);
    if (i % 100 == 0)
      keep[nkeep++] = o;
    else
      free_order(o);
  }
  for (int64_t i = 0; i < nkeep; ++i) {
    Order *o = keep[i];
    n += o->id + o->total + o->nlines + o->lines[0] + (int64_t)strlen(o->customer);
  }
  bench_report("object_creation", t, nkeep == 6000 && n == 1800300000LL + 6000 * 5 + 17999);
  for (int64_t i = 0; i < nkeep; ++i) free_order(keep[i]);
  free(keep);
  return 0;
}
