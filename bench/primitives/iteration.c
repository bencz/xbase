/* iteration.prg in C: the same collections (a string of 1000 bytes, an array and a hash of 1000 numbers, rows of 10
 * bytes) gone through the same ways, as a C program would write them: loops over bytes and arrays, a hash's entries in
 * their array, a generator as a state machine behind a function pointer, Enumerable's methods as stages that call a
 * function pointer per element; prints `<name> <ns per element>`. The sink keeps every result alive. */
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

/* A sequence of numbers: a state and the step that gives the next one (0 at the end). */
typedef struct seq {
  int (*next)(struct seq *, int64_t *);
  struct seq *from;
  int64_t (*map)(int64_t);
  int (*test)(int64_t);
  int64_t state, limit;
  const unsigned char *bytes;
} seq;

static int numbers_next(seq *s, int64_t *out) {
  if (s->state >= s->limit) return 0;
  ++s->state;
  *out = s->state % 97;
  return 1;
}
static int array_next(seq *s, int64_t *out) {
  if (s->state >= s->limit) return 0;
  *out = ((const int64_t *)(const void *)s->bytes)[s->state++];
  return 1;
}
static int bytes_next(seq *s, int64_t *out) {
  if (s->state >= s->limit) return 0;
  *out = s->bytes[s->state++];
  return 1;
}
static int select_next(seq *s, int64_t *out) {
  int64_t v;
  if (!s->from->next(s->from, &v)) return 0;
  *out = s->map(v);
  return 1;
}
static int where_next(seq *s, int64_t *out) {
  int64_t v;
  while (s->from->next(s->from, &v))
    if (s->test(v)) {
      *out = v;
      return 1;
    }
  return 0;
}
static int take_next(seq *s, int64_t *out) {
  if (s->state >= s->limit || !s->from->next(s->from, out)) return 0;
  ++s->state;
  return 1;
}
static int64_t seq_sum(seq *s) {
  int64_t n = 0, v;
  while (s->next(s, &v)) n += v;
  return n;
}
static int64_t seq_count(seq *s) {
  int64_t n = 0, v;
  while (s->next(s, &v)) ++n;
  return n;
}

static int64_t twice(int64_t v) { return v * 2; }
static int64_t plus_one(int64_t v) { return v + 1; }
static int64_t same(int64_t v) { return v; }
static int third(int64_t v) { return v % 3 == 0; }
static int even(int64_t v) { return v % 2 == 0; }
static int past_z(int64_t c) { return c > 'Z'; }

int main(void) {
  static unsigned char s[M];
  static int64_t a[M];
  static struct {
    int64_t key, value;
  } h[M];
  int64_t n = 0;
  for (int i = 1; i <= M; ++i) {
    s[i - 1] = (unsigned char)(48 + i % 64);
    a[i - 1] = i % 97;
    h[i - 1].key = i;
    h[i - 1].value = i % 89;
  }
  MEASURE("string_foreach", for (int i = 0; i < M; ++i) n += s[i]);
  MEASURE("string_index", for (int i = 0; i < M; ++i) n += s[i]);
  MEASURE("array_foreach", for (int i = 0; i < M; ++i) n += a[i]);
  MEASURE("array_index", for (int i = 0; i < M; ++i) n += a[i]);
  MEASURE("hash_foreach", for (int i = 0; i < M; ++i) n += h[i].value);
  MEASURE("generator", seq g = {.next = numbers_next, .limit = M}; n += seq_sum(&g));
  MEASURE("object_enum", seq g = {.next = numbers_next, .limit = M}; n += seq_sum(&g));
  MEASURE("nested_string", for (int r = 0; r < M / 10; ++r) for (int c = 0; c < 10; ++c) n += s[c]);
  MEASURE("select_sum", seq src = {.next = array_next, .limit = M, .bytes = (const unsigned char *)a};
          seq sel = {.next = select_next, .from = &src, .map = twice}; n += seq_sum(&sel));
  MEASURE("select_loop", for (int i = 0; i < M; ++i) n += a[i] * 2);
  MEASURE("where_count", seq src = {.next = array_next, .limit = M, .bytes = (const unsigned char *)a};
          seq w = {.next = where_next, .from = &src, .test = third}; n += seq_count(&w));
  MEASURE("where_loop", for (int i = 0; i < M; ++i) n += a[i] % 3 == 0);
  MEASURE("string_where", seq src = {.next = bytes_next, .limit = M, .bytes = s};
          seq w = {.next = where_next, .from = &src, .test = past_z}; n += seq_count(&w));
  MEASURE("string_select", seq src = {.next = bytes_next, .limit = M, .bytes = s};
          seq sel = {.next = select_next, .from = &src, .map = same}; n += seq_sum(&sel));
  MEASURE("chain_3", seq src = {.next = array_next, .limit = M, .bytes = (const unsigned char *)a};
          seq w = {.next = where_next, .from = &src, .test = even};
          seq sel = {.next = select_next, .from = &w, .map = plus_one};
          seq t = {.next = take_next, .from = &sel, .limit = 400}; n += seq_sum(&t));
  sink += n;
  printf("sink %lld\n", (long long)sink);
  return 0;
}
