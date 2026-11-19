/* bench/relatorio_agregacao.prg in C: 1,000,000 sales records (region, product, quantity, unit price in cents) made
 * from a fixed seed, then timed: aggregated by region and by product in hash tables keyed by code (insertion order
 * kept), the 500 products ordered by amount with the library's sort (ties by code), and a report built with the @E
 * pictures of Transform (written here for them): a line per region, sorted by code (sales, quantity, amount), and the
 * top 20 products; the report split into lines and checked. */
#include "bench.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  char *region, *product;
  int64_t qty, price;
} Sale;

typedef struct {
  const char *key;
  int64_t *value; /* a region's { sales, quantity, amount }, or a product's amount */
} Entry;

typedef struct {
  Entry *entries;
  int64_t *index; /* entry number plus 1; 0 for a free slot */
  int64_t count, cap, slots;
} Hash;

typedef struct {
  char *data;
  size_t len, cap;
} Buf;

static int64_t seed = 12345;
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

static int64_t *slot_of(const Hash *h, const char *key) {
  uint64_t i = hash_str(key) & (uint64_t)(h->slots - 1);
  while (h->index[i] && strcmp(h->entries[h->index[i] - 1].key, key) != 0) i = (i + 1) & (uint64_t)(h->slots - 1);
  return &h->index[i];
}

static void reindex(Hash *h, int64_t slots) {
  free(h->index);
  h->slots = slots;
  h->index = calloc((size_t)slots, sizeof *h->index);
  for (int64_t e = 0; e < h->count; ++e) *slot_of(h, h->entries[e].key) = e + 1;
}

static Entry *get(const Hash *h, const char *key) {
  int64_t *s = slot_of(h, key);
  return *s ? &h->entries[*s - 1] : NULL;
}

static void set(Hash *h, const char *key, int64_t *value) {
  if ((h->count + 1) * 4 > h->slots * 3) reindex(h, h->slots * 2);
  int64_t *s = slot_of(h, key);
  if (*s) {
    h->entries[*s - 1].value = value;
    return;
  }
  if (h->count == h->cap) {
    h->cap = h->cap ? h->cap * 2 : 8;
    h->entries = realloc(h->entries, (size_t)h->cap * sizeof *h->entries);
  }
  h->entries[h->count] = (Entry){key, value};
  *s = ++h->count;
}

static int by_key(const void *p, const void *q) { return strcmp(((const Entry *)p)->key, ((const Entry *)q)->key); }

static void hsort(Hash *h) {
  qsort(h->entries, (size_t)h->count, sizeof *h->entries, by_key);
  reindex(h, h->slots);
}

static void hash_free(Hash *h) {
  for (int64_t e = 0; e < h->count; ++e) free(h->entries[e].value);
  free(h->entries);
  free(h->index);
}

/* Product by amount, descending, then by code. */
static int by_amount(const void *p, const void *q) {
  const Entry *a = p, *b = q;
  if (*a->value != *b->value) return *a->value > *b->value ? -1 : 1;
  return strcmp(a->key, b->key);
}

/* Transform( x, "@E " + pic ) of a number that is not negative: pic's 9s take the digits, a "," after a digit stays
 * (a blank otherwise); then "," and "." swapped. */
static void transform_e(char *out, double x, const char *pic) {
  int intd = 0, dec = 0, seen_dot = 0;
  size_t n = strlen(pic), k = 0;
  char digits[48];
  for (const char *p = pic; *p; ++p)
    if (*p == '.')
      seen_dot = 1;
    else if (*p == '9')
      ++*(seen_dot ? &dec : &intd);
  snprintf(digits, sizeof digits, "%*.*f", intd + (dec ? dec + 1 : 0), dec, x);
  for (size_t i = 0; i < n; ++i) {
    char c = pic[i];
    if (c == '9' || c == '.')
      c = digits[k++];
    else if (c == ',')
      c = i > 0 && out[i - 1] >= '0' && out[i - 1] <= '9' ? ',' : ' ';
    out[i] = c;
  }
  out[n] = '\0';
  for (char *p = out; *p; ++p) *p = *p == ',' ? '.' : *p == '.' ? ',' : *p;
}

static char **split_lines(const char *c, int64_t *count) {
  int64_t n = 1, k = 0;
  for (const char *p = c; *p; ++p) n += *p == '\n';
  char **a = malloc((size_t)n * sizeof *a);
  for (const char *p = c;; ++p) {
    const char *e = strchr(p, '\n');
    size_t len = e ? (size_t)(e - p) : strlen(p);
    a[k] = malloc(len + 1);
    memcpy(a[k], p, len);
    a[k++][len] = '\0';
    if (!e) break;
    p = e;
  }
  *count = k;
  return a;
}

int main(void) {
  int64_t n = 1000000, total = 0, nlines;
  Sale *sales = malloc((size_t)n * sizeof *sales);
  char f1[32], f2[32], f3[32], line[128];
  for (int64_t i = 0; i < n; ++i) {
    sales[i].region = malloc(4);
    snprintf(sales[i].region, 4, "R%02d", (int)(rnd(8) + 1));
    sales[i].product = malloc(6);
    snprintf(sales[i].product, 6, "P%04d", (int)(rnd(500) + 1));
    sales[i].qty = rnd(20) + 1;
    sales[i].price = rnd(100000) + 100;
  }
  long long t = bench_ms();
  Hash reg = {NULL, calloc(8, sizeof(int64_t)), 0, 0, 8}, prod = {NULL, calloc(8, sizeof(int64_t)), 0, 0, 8};
  Buf rep = {0};
  for (int64_t i = 0; i < n; ++i) {
    Sale *r = &sales[i];
    int64_t x = r->qty * r->price;
    Entry *e = get(&reg, r->region);
    if (e) {
      e->value[0]++;
      e->value[1] += r->qty;
      e->value[2] += x;
    } else {
      int64_t *a = malloc(3 * sizeof *a);
      a[0] = 1;
      a[1] = r->qty;
      a[2] = x;
      set(&reg, r->region, a);
    }
    e = get(&prod, r->product);
    if (e)
      *e->value += x;
    else {
      int64_t *a = malloc(sizeof *a);
      *a = x;
      set(&prod, r->product, a);
    }
    total += x;
  }
  Entry *products = malloc((size_t)prod.count * sizeof *products);
  memcpy(products, prod.entries, (size_t)prod.count * sizeof *products);
  qsort(products, (size_t)prod.count, sizeof *products, by_amount);
  hsort(&reg);
  for (int64_t e = 0; e < reg.count; ++e) {
    int64_t *a = reg.entries[e].value;
    transform_e(f1, (double)a[0], "9,999,999");
    transform_e(f2, (double)a[1], "99,999,999");
    transform_e(f3, (double)a[2] / 100, "999,999,999,999.99");
    snprintf(line, sizeof line, "%s%s%s%s\n", reg.entries[e].key, f1, f2, f3);
    put(&rep, line);
  }
  for (int i = 0; i < 20; ++i) {
    transform_e(f3, (double)*products[i].value / 100, "999,999,999,999.99");
    snprintf(line, sizeof line, "%s%s\n", products[i].key, f3);
    put(&rep, line);
  }
  char **lines = split_lines(rep.data, &nlines);
  int ok = total == 525748851783LL && rep.len == 8 * 41 + 20 * 24 && prod.count == 500 &&
           strcmp(lines[0], "R01  125.691 1.320.222    662.846.057,84") == 0 &&
           strcmp(lines[7], "R08  124.683 1.310.936    656.013.155,38") == 0 &&
           strcmp(lines[8], "P0303     11.438.921,18") == 0 && strcmp(lines[27], "P0360     11.021.303,35") == 0;
  bench_report("relatorio_agregacao", t, ok);
  for (int64_t i = 0; i < nlines; ++i) free(lines[i]);
  free(lines);
  free(products);
  hash_free(&reg);
  hash_free(&prod);
  free(rep.data);
  for (int64_t i = 0; i < n; ++i) {
    free(sales[i].region);
    free(sales[i].product);
  }
  free(sales);
  return 0;
}
