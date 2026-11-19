/* bench/ini_parse.prg in C: a text of 400 sections of 25 keys (comments, blank lines, spaces around "=", quoted values,
 * section names in either case) made from a fixed seed, then timed: parsed 80 times into a hash table of sections,
 * each a hash table of keys (string keys; the lines split into new strings, trimmed, names lowered, quotes taken off);
 * the numbers summed and values looked up. */
#include "bench.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  char *data;
  size_t len, cap;
} Buf;

typedef struct {
  char *key; /* NULL for a free slot */
  void *value;
} Slot;

typedef struct {
  Slot *slots;
  int64_t count, cap;
} Hash;

static int64_t seed = 7777;
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

static Hash *hash_new(void) {
  Hash *h = malloc(sizeof *h);
  *h = (Hash){calloc(8, sizeof(Slot)), 0, 8};
  return h;
}

/* Sets key (the table takes it) to value; an old key and value are freed with free_value. */
static void hash_set(Hash *h, char *key, void *value, void (*free_value)(void *)) {
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
  if (s->key) {
    free(s->key);
    free_value(s->value);
  } else
    ++h->count;
  *s = (Slot){key, value};
}

static void *hash_get(const Hash *h, const char *key) { return find(h, key)->value; }

static void hash_free(Hash *h, void (*free_value)(void *)) {
  for (int64_t i = 0; i < h->cap; ++i)
    if (h->slots[i].key) {
      free(h->slots[i].key);
      free_value(h->slots[i].value);
    }
  free(h->slots);
  free(h);
}

static void free_section(void *p) { hash_free(p, free); }

static char *substr(const char *s, size_t n) {
  char *r = malloc(n + 1);
  memcpy(r, s, n);
  r[n] = '\0';
  return r;
}

static char *alltrim(const char *s) {
  size_t n = strlen(s);
  while (n && s[n - 1] == ' ') --n;
  while (n && *s == ' ') {
    ++s;
    --n;
  }
  return substr(s, n);
}

static char *lower(char *s) {
  for (char *p = s; *p; ++p) *p = (char)tolower((unsigned char)*p);
  return s;
}

/* The lines of c, each a new string (an empty one after the last line break). */
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

static Hash *parse_ini(const char *c, int64_t *quoted) {
  Hash *ini = hash_new(), *section = NULL;
  int64_t n;
  char **lines = split_lines(c, &n);
  *quoted = 0;
  for (int64_t i = 0; i < n; ++i) {
    char *line = alltrim(lines[i]), *eq;
    size_t len = strlen(line);
    if (len == 0 || line[0] == ';' || line[0] == '#') {
      free(line);
      continue;
    }
    if (line[0] == '[') {
      section = hash_new();
      hash_set(ini, lower(substr(line + 1, len - 2)), section, free_section);
    } else if ((eq = strchr(line, '=')) != NULL) {
      size_t klen = (size_t)(eq - line);
      while (klen && line[klen - 1] == ' ') --klen;
      char *key = lower(substr(line, klen)), *val;
      const char *v = eq + 1;
      while (*v == ' ') ++v;
      size_t vlen = strlen(v);
      if (vlen >= 2 && v[0] == '"' && v[vlen - 1] == '"') {
        val = substr(v + 1, vlen - 2);
        ++*quoted;
      } else
        val = substr(v, vlen);
      hash_set(section, key, val, free);
    }
    free(line);
  }
  for (int64_t i = 0; i < n; ++i) free(lines[i]);
  free(lines);
  return ini;
}

int main(void) {
  Buf c = {0};
  char line[96], num[24];
  int64_t quoted = 0;
  int ok = 1;
  for (int64_t s = 1; s <= 400; ++s) {
    snprintf(line, sizeof line, "; section %lld\n", (long long)s);
    put(&c, line);
    snprintf(line, sizeof line, "[%s%lld]\n", rnd(2) == 1 ? "Section" : "SECTION", (long long)s);
    put(&c, line);
    for (int64_t k = 1; k <= 25; ++k) {
      int64_t sp = rnd(3);
      snprintf(num, sizeof num, "%lld", (long long)rnd(100000));
      snprintf(line, sizeof line, "%sKey%02lld%s%s%s%s\n", sp == 2 ? "  " : "", (long long)k, sp > 0 ? " = " : "=",
               k % 5 == 0 ? "" : k % 5 == 1 ? "\"text " : "w", num, k % 5 == 1 ? "\"" : "");
      put(&c, line);
      if (k % 7 == 0) put(&c, "# comment\n");
    }
    put(&c, "\n");
  }
  long long t = bench_ms();
  for (int i = 1; i <= 80; ++i) {
    Hash *h = parse_ini(c.data, &quoted);
    int64_t sum = 0;
    for (int64_t s = 0; s < h->cap; ++s) {
      if (!h->slots[s].key) continue;
      Hash *x = h->slots[s].value;
      for (int k = 5; k <= 25; k += 5) {
        snprintf(num, sizeof num, "key%02d", k);
        sum += strtoll(hash_get(x, num), NULL, 10);
      }
    }
    Hash *s77 = hash_get(h, "section77"), *s400 = hash_get(h, "section400");
    ok = ok && h->count == 400 && sum == 102092980 && quoted == 2000 &&
         strcmp(hash_get(s77, "key11"), "text 72557") == 0 && s400->count == 25;
    hash_free(h, free_section);
  }
  ok = ok && c.len == 181694;
  bench_report("ini_parse", t, ok);
  free(c.data);
  return 0;
}
