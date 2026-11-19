/* bench/sequential_scan.prg in C: 200,000 sales records (region, amount, quantity, customer, date) made from a fixed
 * seed in a file of fixed-width records, "bench_seqscan.dat" in the current directory, the fields as text as a DBF
 * keeps them (a deletion flag, numbers right-aligned, the date as YYYYMMDD; no header); then timed, 4 rounds of six
 * passes over every record, each read from the file through stdio's buffer and its fields converted when read: a loop
 * to the end, a sum, a count with a condition, an evaluation of a function for the records a condition function
 * accepts, a search for the records that match continued to the end, and a sum over a date range. The file is deleted
 * at the end. */
#include "bench.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FILE_NAME "bench_seqscan.dat"
#define RECLEN 33 /* flag 1, REGION N 1, AMOUNT N 10, QTY N 3, CUST C 10, SALEDATE D 8 */

typedef struct {
  FILE *f;
  char rec[RECLEN];
  int64_t recno, count;
  int eof;
} Table;

static int64_t seed = 31337;
static int64_t rnd(int64_t n) {
  seed = seed * 16807 % 2147483647;
  return seed % n;
}

static int64_t days_from_civil(int64_t y, int64_t m, int64_t d) {
  y -= m <= 2;
  int64_t era = (y >= 0 ? y : y - 399) / 400, yoe = y - era * 400;
  int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

static void civil_from_days(int64_t z, int64_t *y, int64_t *m, int64_t *d) {
  z += 719468;
  int64_t era = (z >= 0 ? z : z - 146096) / 146097, doe = z - era * 146097;
  int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100), mp = (5 * doy + 2) / 153;
  *d = doy - (153 * mp + 2) / 5 + 1;
  *m = mp < 10 ? mp + 3 : mp - 9;
  *y = yoe + era * 400 + (*m <= 2);
}

static void go_to(Table *t, int64_t recno) {
  t->recno = recno;
  t->eof = recno > t->count;
  if (t->eof) return;
  fseek(t->f, (long)((recno - 1) * RECLEN), SEEK_SET);
  if (fread(t->rec, RECLEN, 1, t->f) != 1) t->eof = 1;
}

static void go_top(Table *t) { go_to(t, 1); }

/* The next record, read from where the file is. */
static void skip(Table *t) {
  ++t->recno;
  t->eof = t->recno > t->count || fread(t->rec, RECLEN, 1, t->f) != 1;
}

static int64_t num_field(const Table *t, int offset, int width) {
  int64_t n = 0;
  int i = 0, neg = 0;
  while (i < width && t->rec[offset + i] == ' ') ++i;
  if (i < width && t->rec[offset + i] == '-') {
    neg = 1;
    ++i;
  }
  for (; i < width; ++i) n = n * 10 + (t->rec[offset + i] - '0');
  return neg ? -n : n;
}

static int64_t region(const Table *t) { return num_field(t, 1, 1); }
static int64_t amount(const Table *t) { return num_field(t, 2, 10); }
static int64_t qty(const Table *t) { return num_field(t, 12, 3); }
static const char *cust(const Table *t) { return t->rec + 15; }

static int64_t saledate(const Table *t) {
  const char *s = t->rec + 25;
  int64_t y = 0, m = 0, d = 0;
  for (int i = 0; i < 4; ++i) y = y * 10 + (s[i] - '0');
  for (int i = 4; i < 6; ++i) m = m * 10 + (s[i] - '0');
  for (int i = 6; i < 8; ++i) d = d * 10 + (s[i] - '0');
  return days_from_civil(y, m, d);
}

static int64_t qty_sum;
static void add_qty(const Table *t) { qty_sum += qty(t); }
static int north(const Table *t) { return region(t) <= 2; }
static int found_cust(const Table *t) { return strncmp(cust(t), "CUST0001", 8) == 0; }

/* dbEval( bAction, bFor ) over every record. */
static void db_eval(Table *t, void (*action)(const Table *), int (*cond)(const Table *)) {
  for (go_top(t); !t->eof; skip(t))
    if (cond(t)) action(t);
}

/* LOCATE FOR cond (from the top) and CONTINUE (from the next record): whether a record was found. */
static int locate(Table *t, int (*cond)(const Table *), int from_top) {
  if (from_top)
    go_top(t);
  else
    skip(t);
  for (; !t->eof; skip(t))
    if (cond(t)) return 1;
  return 0;
}

static int scans(Table *t, int64_t feb1) {
  int64_t sum_r3 = 0, cnt_r3 = 0, all = 0, big = 0, loc = 0, loc_ids = 0, jan = 0;
  for (go_top(t); !t->eof; skip(t))
    if (region(t) == 3) {
      sum_r3 += amount(t);
      ++cnt_r3;
    }
  for (go_top(t); !t->eof; skip(t)) all += amount(t);
  for (go_top(t); !t->eof; skip(t))
    if (amount(t) > 900000) ++big;
  qty_sum = 0;
  int (*volatile cond)(const Table *) = north;
  void (*volatile action)(const Table *) = add_qty;
  db_eval(t, action, cond);
  int (*volatile match)(const Table *) = found_cust;
  for (int found = locate(t, match, 1); found; found = locate(t, match, 0)) {
    ++loc;
    loc_ids += t->recno;
  }
  for (go_top(t); !t->eof; skip(t))
    if (saledate(t) < feb1) jan += amount(t);
  return sum_r3 == 11099768125LL && cnt_r3 == 22094 && all == 99898922935LL && big == 19973 && qty_sum == 1135702 &&
         loc == 396 && loc_ids == 40072280 && jan == 8435099403LL;
}

int main(void) {
  int64_t base = days_from_civil(2026, 1, 1), y, m, d;
  int ok = 1;
  char rec[48];
  FILE *f = fopen(FILE_NAME, "wb");
  if (!f) return 1;
  for (int64_t i = 1; i <= 200000; ++i) {
    int64_t reg = rnd(9) + 1, amt = rnd(1000000), q = rnd(50) + 1;
    civil_from_days(base + rnd(365), &y, &m, &d);
    snprintf(rec, sizeof rec, " %1lld%10lld%3lld%-10s%04d%02d%02d", (long long)reg, (long long)amt, (long long)q, "",
             (int)y, (int)m, (int)d);
    char c[11];
    snprintf(c, sizeof c, "CUST%06lld", (long long)rnd(50000));
    memcpy(rec + 15, c, 10);
    fwrite(rec, RECLEN, 1, f);
  }
  fclose(f);
  Table t = {fopen(FILE_NAME, "rb"), {0}, 0, 200000, 1};
  if (!t.f) return 1;
  long long start = bench_ms();
  for (int i = 1; i <= 4; ++i) ok = ok && scans(&t, days_from_civil(2026, 2, 1));
  bench_report("sequential_scan", start, ok);
  fclose(t.f);
  remove(FILE_NAME);
  return 0;
}
