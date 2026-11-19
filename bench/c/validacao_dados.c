/* bench/validacao_dados.prg in C: 250,000 records { CPF, e-mail, date text, amount text } made from a fixed seed, with
 * errors mixed in (a wrong check digit, an e-mail with no "@", no dot or a bad character, impossible dates, a letter
 * among the digits). Timed: each field validated by its rule (the CPF's two mod-11 check digits, the e-mail's parts
 * and characters, the date as dd/mm/yyyy read into a valid date of the calendar, as CToD() does under SET DATE
 * BRITISH, the amount's digits and range) and the valid ones counted. */
#include "bench.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  char cpf[16], mail[32], date[16], amount[8];
} Record;

static int64_t seed = 8086;
static int64_t rnd(int64_t n) {
  seed = seed * 16807 % 2147483647;
  return seed % n;
}

static int cpf_ok(const char *c) {
  char d[16];
  size_t n = 0;
  int64_t s = 0;
  for (; *c && n < sizeof d - 1; ++c)
    if (*c != '.' && *c != '-') d[n++] = *c;
  if (*c || n != 11) return 0;
  for (size_t i = 0; i < 11; ++i)
    if (!isdigit((unsigned char)d[i])) return 0;
  for (int i = 1; i <= 9; ++i) s += (d[i - 1] - 48) * (11 - i);
  if (s * 10 % 11 % 10 != d[9] - 48) return 0;
  s = 0;
  for (int i = 1; i <= 10; ++i) s += (d[i - 1] - 48) * (12 - i);
  return s * 10 % 11 % 10 == d[10] - 48;
}

static int email_ok(const char *e) {
  const char *at = strchr(e, '@');
  if (!at || at - e < 1 || strchr(at + 1, '@')) return 0;
  for (const char *p = e; p < at; ++p)
    if (!strchr("abcdefghijklmnopqrstuvwxyz0123456789._", *p)) return 0;
  const char *dom = at + 1;
  size_t len = strlen(dom);
  if (len < 3 || !strchr(dom, '.') || dom[0] == '.' || dom[len - 1] == '.') return 0;
  for (const char *p = dom; *p; ++p)
    if (!strchr("abcdefghijklmnopqrstuvwxyz0123456789.-", *p)) return 0;
  return 1;
}

static int date_ok(const char *s) {
  static const int dim[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  int d, m, y;
  if (sscanf(s, "%2d/%2d/%4d", &d, &m, &y) != 3 || m < 1 || m > 12 || d < 1) return 0;
  int leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
  return d <= dim[m - 1] + (m == 2 && leap);
}

static int amount_ok(const char *c) {
  for (const char *p = c; *p; ++p)
    if (!isdigit((unsigned char)*p)) return 0;
  int64_t n = strtoll(c, NULL, 10);
  return n >= 1 && n <= 999999;
}

/* A record; its errors come from the seed as the checks' constants were computed. */
static void make_record(Record *r) {
  static const char *const doms[] = {"mail.com", "example.com.br", "x.org"};
  int64_t d[11], s = 0, k, len;
  for (k = 0; k < 9; ++k) d[k] = rnd(10);
  for (k = 1; k <= 9; ++k) s += d[k - 1] * (11 - k);
  d[9] = s * 10 % 11 % 10;
  s = 0;
  for (k = 1; k <= 10; ++k) s += d[k - 1] * (12 - k);
  d[10] = s * 10 % 11 % 10;
  if (rnd(4) == 0) d[10] = (d[10] + 1) % 10;
  char *p = r->cpf;
  for (k = 1; k <= 11; ++k) {
    *p++ = (char)('0' + d[k - 1]);
    if (k == 3 || k == 6) *p++ = '.';
    if (k == 9) *p++ = '-';
  }
  *p = '\0';
  len = rnd(8) + 3;
  for (k = 0; k < len; ++k) r->mail[k] = "abcdefghijklmnopqrstuvwxyz0123456789"[rnd(36)];
  r->mail[len] = '\0';
  int64_t kind = rnd(20);
  const char *dom = doms[rnd(3)];
  strcat(r->mail, kind == 0 ? "" : kind == 1 ? "@examplecom" : kind == 2 ? "!@" : "@");
  if (kind != 1) strcat(r->mail, dom);
  int64_t day = rnd(31) + 1, month = rnd(12) + 1, year = 1990 + rnd(41);
  snprintf(r->date, sizeof r->date, "%02d/%02d/%4d", (int)day, (int)month, (int)year);
  len = rnd(7) + 1;
  for (k = 0; k < len; ++k) r->amount[k] = (char)('0' + rnd(10));
  r->amount[len] = '\0';
  if (rnd(10) == 0) r->amount[rnd(len)] = 'x';
}

int main(void) {
  int64_t n = 250000, nc = 0, ne = 0, nd = 0, na = 0, all = 0, sum = 0;
  Record *recs = malloc((size_t)n * sizeof *recs);
  for (int64_t i = 0; i < n; ++i) make_record(&recs[i]);
  long long t = bench_ms();
  for (int64_t i = 0; i < n; ++i) {
    Record *r = &recs[i];
    int lc = cpf_ok(r->cpf), le = email_ok(r->mail), ld = date_ok(r->date), la = amount_ok(r->amount);
    if (la) sum += strtoll(r->amount, NULL, 10);
    nc += lc;
    ne += le;
    nd += ld;
    na += la;
    all += lc && le && ld && la;
  }
  bench_report("validacao_dados", t,
               nc == 187295 && ne == 212602 && nd == 245457 && na == 192603 && all == 120445 && sum == 19490512431LL);
  free(recs);
  return 0;
}
