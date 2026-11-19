/* bench/pivot_crosstab.prg in C: 2,000,000 sales records { seller, date, amount } made from a fixed seed (60 sellers,
 * the days of 2025, a date as days since 1970), then timed: the amounts summed per seller and month (the month from
 * the civil calendar) in a hash table of rows (string keys, insertion order kept), and per weekday and month; row and
 * column totals, the rows sorted by seller, and the table rendered as text (padding and widths with snprintf()) with a
 * totals line. */
#include "bench.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  char *seller;
  int64_t date, amount;
} Sale;

typedef struct {
  const char *key;
  int64_t *row;
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

static int64_t seed = 16180;
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

static int64_t days_from_civil(int64_t y, int64_t m, int64_t d) {
  y -= m <= 2;
  int64_t era = (y >= 0 ? y : y - 399) / 400, yoe = y - era * 400;
  int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

static int64_t month_of(int64_t z) {
  z += 719468;
  int64_t era = (z >= 0 ? z : z - 146096) / 146097, doe = z - era * 146097;
  int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100), mp = (5 * doy + 2) / 153;
  return mp < 10 ? mp + 3 : mp - 9;
}

static int64_t dow_of(int64_t z) { return ((z + 4) % 7 + 7) % 7 + 1; } /* 1970-01-01 was a Thursday; 1 is Sunday */

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

/* HGetDef( h, key ): NULL when key is not there. */
static int64_t *get(const Hash *h, const char *key) {
  int64_t *s = slot_of(h, key);
  return *s ? h->entries[*s - 1].row : NULL;
}

static void set(Hash *h, const char *key, int64_t *row) {
  if ((h->count + 1) * 4 > h->slots * 3) reindex(h, h->slots * 2);
  int64_t *s = slot_of(h, key);
  if (*s) {
    h->entries[*s - 1].row = row;
    return;
  }
  if (h->count == h->cap) {
    h->cap = h->cap ? h->cap * 2 : 8;
    h->entries = realloc(h->entries, (size_t)h->cap * sizeof *h->entries);
  }
  h->entries[h->count] = (Entry){key, row};
  *s = ++h->count;
}

static int by_key(const void *p, const void *q) { return strcmp(((const Entry *)p)->key, ((const Entry *)q)->key); }

static void hsort(Hash *h) {
  qsort(h->entries, (size_t)h->count, sizeof *h->entries, by_key);
  reindex(h, h->slots);
}

int main(void) {
  int64_t n = 2000000, base = days_from_civil(2025, 1, 1), col[12] = {0}, dow[7][12] = {{0}}, sat = 0, grand = 0,
          best = 0;
  Sale *sales = malloc((size_t)n * sizeof *sales);
  char cell[32], s07[256] = "", line[256];
  const char *best_seller = "";
  for (int64_t i = 0; i < n; ++i) {
    sales[i].seller = malloc(10);
    snprintf(sales[i].seller, 10, "SELLER%03d", (int)(rnd(60) + 1));
    sales[i].date = base + rnd(365);
    sales[i].amount = rnd(50000) + 1;
  }
  long long t = bench_ms();
  Hash rows = {NULL, calloc(8, sizeof(int64_t)), 0, 0, 8};
  Buf tab = {0};
  for (int64_t i = 0; i < n; ++i) {
    Sale *r = &sales[i];
    int64_t *a = get(&rows, r->seller);
    if (!a) {
      a = calloc(12, sizeof *a);
      set(&rows, r->seller, a);
    }
    int64_t month = month_of(r->date);
    a[month - 1] += r->amount;
    dow[dow_of(r->date) - 1][month - 1] += r->amount;
  }
  hsort(&rows);
  for (int64_t e = 0; e < rows.count; ++e) {
    const char *k = rows.entries[e].key;
    int64_t *a = rows.entries[e].row, row = 0;
    snprintf(line, sizeof line, "%-10s", k);
    for (int i = 0; i < 12; ++i) {
      snprintf(cell, sizeof cell, "%11lld", (long long)a[i]);
      strcat(line, cell);
      row += a[i];
      col[i] += a[i];
    }
    snprintf(cell, sizeof cell, "%13lld", (long long)row);
    strcat(line, cell);
    if (row > best) {
      best_seller = k;
      best = row;
    }
    if (strcmp(k, "SELLER007") == 0) strcpy(s07, line);
    put(&tab, line);
    put(&tab, "\n");
  }
  snprintf(line, sizeof line, "%-10s", "TOTAL");
  for (int i = 0; i < 12; ++i) {
    snprintf(cell, sizeof cell, "%11lld", (long long)col[i]);
    strcat(line, cell);
    grand += col[i];
  }
  snprintf(cell, sizeof cell, "%13lld\n", (long long)grand);
  strcat(line, cell);
  put(&tab, line);
  for (int i = 0; i < 12; ++i) sat += dow[6][i];
  int ok = grand == 50028051852LL && strcmp(best_seller, "SELLER015") == 0 && best == 844241139 &&
           col[0] == 4275964399LL && col[1] == 3837132322LL && col[11] == 4246237533LL && dow[0][0] == 547728957 &&
           sat == 7111132032LL && tab.len == 61 * (10 + 12 * 11 + 13 + 1) &&
           strcmp(s07, "SELLER007 "
                       "   71382096   62515370   68284183   68152167   70863968   69201960"
                       "   71447433   71672784   67917922   68878748   65718083   68477906    824512620") == 0;
  bench_report("pivot_crosstab", t, ok);
  for (int64_t e = 0; e < rows.count; ++e) free(rows.entries[e].row);
  free(rows.entries);
  free(rows.index);
  free(tab.data);
  for (int64_t i = 0; i < n; ++i) free(sales[i].seller);
  free(sales);
  return 0;
}
