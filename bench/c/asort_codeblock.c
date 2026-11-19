/* bench/asort_codeblock.prg in C: 150,000 employee records { name, department, salary, id } made from a fixed seed,
 * ordered with the library's sort by department, then salary descending, then name; the comparison breaks the last
 * ties by the original position, which gives the order of the .prg's stable sort. The order is checked with a running
 * hash of the ids. */
#include "bench.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
  char name[9];
  int64_t dept, salary, id;
} Employee;

static int64_t seed = 4242;
static int64_t rnd(int64_t n) {
  seed = seed * 16807 % 2147483647;
  return seed % n;
}

static int compare(const void *x, const void *y) {
  const Employee *p = *(Employee *const *)x, *q = *(Employee *const *)y;
  if (p->dept != q->dept) return p->dept < q->dept ? -1 : 1;
  if (p->salary != q->salary) return p->salary > q->salary ? -1 : 1;
  int c = strcmp(p->name, q->name);
  if (c != 0) return c;
  return (p->id > q->id) - (p->id < q->id);
}

int main(void) {
  int64_t n = 150000, chk = 0;
  Employee **a = malloc((size_t)n * sizeof *a);
  for (int64_t i = 0; i < n; ++i) {
    Employee *e = malloc(sizeof *e);
    for (int k = 0; k < 8; ++k) e->name[k] = (char)(65 + rnd(26));
    e->name[8] = '\0';
    e->dept = rnd(40) + 1;
    e->salary = rnd(90000) + 1000;
    e->id = i + 1;
    a[i] = e;
  }
  long long t = bench_ms();
  qsort(a, (size_t)n, sizeof *a, compare);
  for (int64_t i = 0; i < n; ++i) chk = (chk * 31 + a[i]->id) % 1000000007;
  int ok = chk == 773935306 && a[0]->id == 84060 && strcmp(a[0]->name, "SOFHDANH") == 0 && a[75000]->id == 10853 &&
           a[149999]->id == 11664;
  bench_report("asort_codeblock", t, ok);
  for (int64_t i = 0; i < n; ++i) free(a[i]);
  free(a);
  return 0;
}
