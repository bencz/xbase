/* reads.prg in C: the same expressions over a point's fields and an array of 1000 numbers, the repeated reads written
 * out as the xBase code writes them (the C compiler keeps the loads in registers) and once into a local; prints
 * `<name> <ns per expression>`. The sink keeps every result alive. */
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#define N 4000000
#define M 1000

static int64_t sink;
static struct timespec t0;

static void start(void) { timespec_get(&t0, TIME_UTC); }

static void report(const char *name) {
  struct timespec t1;
  timespec_get(&t1, TIME_UTC);
  double ns = ((double)(t1.tv_sec - t0.tv_sec) * 1e9 + (double)(t1.tv_nsec - t0.tv_nsec)) / N;
  printf("%-18s %8.1f\n", name, ns);
}

#define MEASURE(name, ...)                                                                                             \
  do {                                                                                                                 \
    start();                                                                                                           \
    for (int k = 0; k < N / M; ++k) {                                                                                  \
      __VA_ARGS__;                                                                                                     \
    }                                                                                                                  \
    report(name);                                                                                                      \
  } while (0)

typedef struct point {
  int64_t x, y;
} point;

static int64_t member_thrice(const point *o) {
  int64_t n = 0;
  for (int64_t i = 1; i <= M; ++i) n += o->x * o->x + o->y * o->x + i;
  return n;
}

static int64_t member_once(const point *o) {
  int64_t n = 0;
  for (int64_t i = 1; i <= M; ++i) {
    int64_t x = o->x;
    n += x * x + o->y * x + i;
  }
  return n;
}

static int64_t element_thrice(const int64_t *a) {
  int64_t n = 0;
  for (int i = 0; i < M; ++i) n += a[i] * a[i] + a[0] * a[i];
  return n;
}

static int64_t element_once(const int64_t *a) {
  int64_t n = 0;
  for (int i = 0; i < M; ++i) {
    int64_t x = a[i];
    n += x * x + a[0] * x;
  }
  return n;
}

int main(void) {
  static int64_t a[M];
  for (int i = 0; i < M; ++i) a[i] = (i + 1) % 97;
  volatile point vp = {3, 4};
  point o = {vp.x, vp.y};
  MEASURE("member_thrice", sink += member_thrice(&o));
  MEASURE("member_once", sink += member_once(&o));
  MEASURE("element_thrice", sink += element_thrice(a));
  MEASURE("element_once", sink += element_once(a));
  printf("sink %lld\n", (long long)sink);
  return 0;
}
