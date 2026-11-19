/* bench/property_access.prg in C: 3,000,000 rounds on an object reached through a pointer the compiler cannot follow,
 * of variable reads and assignments (=, += and ++ on its fields), a computed access and an assign method (through the
 * class's table of methods), and the same through a second pointer to it, as WITH OBJECT does. */
#include "bench.h"
#include <string.h>

typedef struct Account Account;
typedef struct {
  int64_t (*doubled)(const Account *self);
  void (*limit)(Account *self, int64_t x);
} Methods;

struct Account {
  const Methods *m;
  int64_t balance, count, limit;
  const char *last;
};

static int64_t doubled(const Account *self) { return self->balance * 2 - self->count - 2; }
static void set_limit(Account *self, int64_t x) { self->limit = x * 10; }
static const Methods account_methods = {doubled, set_limit};

int main(void) {
  long long t = bench_ms();
  Account obj = {&account_methods, 0, 0, 0, ""};
  Account *volatile p = &obj;
  int64_t n = 0;
  for (int64_t i = 1; i <= 3000000; ++i) {
    Account *o = p;
    o->balance += 2;
    o->count++;
    o->last = "d";
    n += o->balance - o->count + o->m->doubled(o);
    o->m->limit(o, i % 10);
    Account *with = p;
    with->balance -= 1;
    n += with->limit + (int64_t)strlen(with->last);
  }
  bench_report("property_access", t,
               n == 4500001500000LL + 10 * 13500000LL + 2 * 3000000LL && p->balance == 3000000);
  return 0;
}
