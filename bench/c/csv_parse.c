/* bench/csv_parse.prg in C: a 40,000-line CSV text (about 2 MB) of 5 fields, some quoted with commas, doubled quotes
 * and line breaks inside, read byte by byte by a state machine into an array of records, each an array of fields in
 * strings of their own; numbers read with strtoll() as Val() reads them; 6 rounds. */
#include "bench.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  char *data;
  size_t len, cap;
} Buf;

typedef struct {
  char **fields;
  int64_t count, cap;
} Record;

typedef struct {
  Record *items;
  int64_t count, cap;
} Records;

static int64_t seed = 1999;
static int64_t rnd(int64_t n) {
  seed = seed * 16807 % 2147483647;
  return seed % n;
}

static void put(Buf *b, const char *s, size_t n) {
  if (b->len + n + 1 > b->cap) {
    while (b->len + n + 1 > b->cap) b->cap = b->cap ? b->cap * 2 : 64;
    b->data = realloc(b->data, b->cap);
  }
  memcpy(b->data + b->len, s, n);
  b->len += n;
  b->data[b->len] = '\0';
}

static void putc_(Buf *b, char c) { put(b, &c, 1); }

static void puts_(Buf *b, const char *s) { put(b, s, strlen(s)); }

static void field_add(Record *r, Buf *f) {
  if (r->count == r->cap) {
    r->cap = r->cap ? r->cap * 2 : 4;
    r->fields = realloc(r->fields, (size_t)r->cap * sizeof *r->fields);
  }
  char *s = malloc(f->len + 1);
  memcpy(s, f->data ? f->data : "", f->len);
  s[f->len] = '\0';
  r->fields[r->count++] = s;
  f->len = 0;
}

static void record_add(Records *rs, Record r) {
  if (rs->count == rs->cap) {
    rs->cap = rs->cap ? rs->cap * 2 : 4;
    rs->items = realloc(rs->items, (size_t)rs->cap * sizeof *rs->items);
  }
  rs->items[rs->count++] = r;
}

static Records parse_csv(const char *c, size_t len) {
  Records recs = {0};
  Record rec = {0};
  Buf field = {0};
  int quoted = 0;
  for (size_t i = 0; i < len; ++i) {
    char x = c[i];
    if (quoted) {
      if (x == '"') {
        if (i + 1 < len && c[i + 1] == '"') {
          putc_(&field, x);
          ++i;
        } else
          quoted = 0;
      } else
        putc_(&field, x);
    } else if (x == '"')
      quoted = 1;
    else if (x == ',')
      field_add(&rec, &field);
    else if (x == '\n') {
      field_add(&rec, &field);
      record_add(&recs, rec);
      rec = (Record){0};
    } else
      putc_(&field, x);
  }
  free(rec.fields);
  free(field.data);
  return recs;
}

static void free_records(Records *rs) {
  for (int64_t i = 0; i < rs->count; ++i) {
    for (int64_t k = 0; k < rs->items[i].count; ++k) free(rs->items[i].fields[k]);
    free(rs->items[i].fields);
  }
  free(rs->items);
}

/* A field: plain letters, or quoted with a comma, a doubled quote or a line break inside. */
static void make_field(Buf *b) {
  int64_t k = rnd(6);
  char w[16];
  char letter = (char)(97 + rnd(26));
  int64_t n = 3 + rnd(12);
  memset(w, letter, (size_t)n);
  w[n] = '\0';
  if (k == 0) {
    putc_(b, '"');
    puts_(b, w);
    puts_(b, ", ");
    puts_(b, w);
    putc_(b, '"');
  } else if (k == 1) {
    puts_(b, "\"say \"\"");
    puts_(b, w);
    puts_(b, "\"\"\"");
  } else if (k == 2) {
    putc_(b, '"');
    puts_(b, w);
    putc_(b, '\n');
    puts_(b, w);
    putc_(b, '"');
  } else
    puts_(b, w);
}

int main(void) {
  Buf c = {0};
  char num[24];
  int64_t n = 0, quoted = 0, count = 0;
  for (int64_t i = 1; i <= 40000; ++i) {
    snprintf(num, sizeof num, "%lld,", (long long)i);
    puts_(&c, num);
    make_field(&c);
    snprintf(num, sizeof num, ",%lld.00,", (long long)rnd(100000)); /* Rand()'s % gives 2 decimals */
    puts_(&c, num);
    make_field(&c);
    putc_(&c, ',');
    for (int64_t k = rnd(10); k > 0; --k) putc_(&c, 'w');
    putc_(&c, '\n');
  }
  long long t = bench_ms();
  for (int r = 1; r <= 6; ++r) {
    Records recs = parse_csv(c.data, c.len);
    for (int64_t i = 0; i < recs.count; ++i) {
      char **a = recs.items[i].fields;
      n += strtoll(a[0], NULL, 10) + strtoll(a[2], NULL, 10) + (int64_t)strlen(a[1]) + (int64_t)strlen(a[3]) +
           recs.items[i].count;
      if (strchr(a[3], '\n') || strchr(a[1], '"')) ++quoted;
    }
    count = recs.count;
    free_records(&recs);
  }
  bench_report("csv_parse", t, count == 40000 && n == 16750579326LL && quoted == 73038);
  free(c.data);
  return 0;
}
