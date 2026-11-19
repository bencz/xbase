/* bench/log_processing.prg in C: 300,000 lines of an access log (time, level, [module], user, duration, status) made
 * from a fixed seed as one text, then timed: split into lines (each a new string), each line parsed (fixed columns,
 * strstr() for the module and the key=value fields, numbers read with strtoll() as Val() reads them), lines counted per
 * level in a hash table, durations summed per module, the slowest request found, server errors counted per hour,
 * distinct users counted in a hash table (string keys, each a new string). */
#include "bench.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  char *key; /* NULL for a free slot */
  int64_t value;
} Slot;

typedef struct {
  Slot *slots;
  int64_t count, cap;
} Hash;

typedef struct {
  char *data;
  size_t len, cap;
} Buf;

static int64_t seed = 9001;
static int64_t rnd(int64_t n) {
  seed = seed * 16807 % 2147483647;
  return seed % n;
}

static void put(Buf *b, const char *s) {
  size_t n = strlen(s);
  if (b->len + n + 1 > b->cap) {
    while (b->len + n + 1 > b->cap) b->cap = b->cap ? b->cap * 2 : 64;
    b->data = realloc(b->data, b->cap);
  }
  memcpy(b->data + b->len, s, n + 1);
  b->len += n;
}

static char *substr(const char *s, size_t n) {
  char *r = malloc(n + 1);
  memcpy(r, s, n);
  r[n] = '\0';
  return r;
}

static uint64_t hash_str(const char *s) {
  uint64_t h = 14695981039346656037ULL;
  for (; *s; ++s) h = (h ^ (unsigned char)*s) * 1099511628211ULL;
  return h;
}

static Slot *find(const Hash *h, const char *key) {
  uint64_t i = hash_str(key) & (uint64_t)(h->cap - 1);
  while (h->slots[i].key && strcmp(h->slots[i].key, key) != 0) i = (i + 1) & (uint64_t)(h->cap - 1);
  return &h->slots[i];
}

static int64_t get_def(const Hash *h, const char *key, int64_t def) {
  Slot *s = find(h, key);
  return s->key ? s->value : def;
}

/* h[key] := value; the table takes key. */
static void set(Hash *h, char *key, int64_t value) {
  if ((h->count + 1) * 4 > h->cap * 3) {
    Slot *old = h->slots;
    int64_t n = h->cap;
    h->cap *= 2;
    h->slots = calloc((size_t)h->cap, sizeof *h->slots);
    for (int64_t i = 0; i < n; ++i)
      if (old[i].key) *find(h, old[i].key) = old[i];
    free(old);
  }
  Slot *s = find(h, key);
  if (s->key)
    free(key);
  else {
    s->key = key;
    ++h->count;
  }
  s->value = value;
}

static Hash hash_new(void) { return (Hash){calloc(8, sizeof(Slot)), 0, 8}; }

static void hash_free(Hash *h) {
  for (int64_t i = 0; i < h->cap; ++i) free(h->slots[i].key);
  free(h->slots);
}

static char **split_lines(const char *c, int64_t *count) {
  int64_t n = 1, k = 0;
  for (const char *p = c; *p; ++p) n += *p == '\n';
  char **a = malloc((size_t)n * sizeof *a);
  for (const char *p = c;; ++p) {
    const char *e = strchr(p, '\n');
    if (!e) {
      a[k++] = substr(p, strlen(p));
      break;
    }
    a[k++] = substr(p, (size_t)(e - p));
    p = e;
  }
  *count = k;
  return a;
}

int main(void) {
  static const char *const lv[] = {"INFO ", "WARN ", "ERROR", "DEBUG"};
  static const char *const mod[] = {"auth", "db", "http", "cache", "queue", "mail"};
  static const int64_t st[] = {200, 200, 200, 201, 204, 301, 404, 500, 503, 200};
  int64_t n = 300000, i, max = -1, max_line = 0, err5 = 0, hour_err[24] = {0}, count;
  Buf c = {0};
  char line[160];
  for (i = 1; i <= n; ++i) {
    int64_t sec = i * 86400 / n, x = rnd(1000), l = rnd(10);
    l = l < 6 ? 1 : l < 8 ? 4 : l < 9 ? 2 : 3;
    const char *m = mod[rnd(6)];
    int64_t user = rnd(5000), ms = rnd(2000), status = st[rnd(10)];
    snprintf(line, sizeof line, "2026-11-19 %02d:%02d:%02d.%03d %s [%s] request done user=u%lld ms=%lld status=%3lld\n",
             (int)(sec / 3600), (int)(sec / 60 % 60), (int)(sec % 60), (int)x, lv[l - 1], m, (long long)user,
             (long long)ms, (long long)status);
    put(&c, line);
  }
  long long t = bench_ms();
  Hash lvs = hash_new(), mods = hash_new(), users = hash_new();
  char **lines = split_lines(c.data, &count);
  i = 0;
  for (int64_t k = 0; k < count; ++k) {
    const char *x = lines[k];
    if (strspn(x, " ") == strlen(x)) continue; /* Empty() */
    ++i;
    size_t llen = 5;
    while (llen && x[24 + llen - 1] == ' ') --llen;
    char *level = substr(x + 24, llen);
    set(&lvs, level, get_def(&lvs, level, 0) + 1);
    const char *b = strchr(x, '['), *e = strchr(x, ']');
    char *module = substr(b + 1, (size_t)(e - b - 1));
    const char *msp = strstr(x, " ms=");
    int64_t ms = strtoll(msp + 4, NULL, 10);
    set(&mods, module, get_def(&mods, module, 0) + ms);
    if (ms > max) {
      max = ms;
      max_line = i;
    }
    int64_t status = strtoll(strstr(x, " status=") + 8, NULL, 10);
    if (status >= 500) {
      ++err5;
      char hh[3] = {x[11], x[12], 0};
      ++hour_err[strtoll(hh, NULL, 10)];
    }
    const char *u = strstr(x, " user=");
    set(&users, substr(u + 6, (size_t)(msp - u - 6)), 1);
  }
  int ok = c.len == 23765927 && i == n && get_def(&lvs, "INFO", 0) == 179990 && get_def(&lvs, "WARN", 0) == 29916 &&
           get_def(&lvs, "ERROR", 0) == 30173 && get_def(&lvs, "DEBUG", 0) == 59921 &&
           get_def(&mods, "auth", 0) == 50005329 && get_def(&mods, "db", 0) == 50599071 &&
           get_def(&mods, "http", 0) == 50094170 && get_def(&mods, "cache", 0) == 49851623 &&
           get_def(&mods, "queue", 0) == 49898190 && get_def(&mods, "mail", 0) == 49855051 && max == 1999 &&
           max_line == 1062 && err5 == 59808 && hour_err[13] == 2503 && users.count == 5000;
  bench_report("log_processing", t, ok);
  for (int64_t k = 0; k < count; ++k) free(lines[k]);
  free(lines);
  hash_free(&lvs);
  hash_free(&mods);
  hash_free(&users);
  free(c.data);
  return 0;
}
