/* bench/pidigits.prg in C: the first 1,500 digits of pi from Gibbons' unbounded spigot, on numbers kept as arrays of
 * 30-bit limbs (the least significant first; the accumulator's sign apart) with the same operations as the .prg:
 * multiply by a small number, add or subtract a multiple, compare, and a quotient below 2^30 estimated from the
 * leading limbs, then corrected. */
#include "bench.h"
#include <stdlib.h>
#include <string.h>

#define BITS 30
#define LIMB_MASK ((1 << BITS) - 1)

typedef struct {
  int64_t *d;
  int64_t n, cap;
} big;

static void reserve(big *a, int64_t n) {
  if (n > a->cap) {
    a->cap = n * 2;
    a->d = realloc(a->d, (size_t)a->cap * sizeof *a->d);
  }
}

static void set_small(big *a, int64_t v) {
  reserve(a, 1);
  a->d[0] = v;
  a->n = 1;
}

static void copy(big *a, const big *b) {
  reserve(a, b->n);
  memcpy(a->d, b->d, (size_t)b->n * sizeof *a->d);
  a->n = b->n;
}

static void trim(big *a) {
  while (a->n > 1 && a->d[a->n - 1] == 0) --a->n;
}

/* a *= m */
static void mul_small(big *a, int64_t m) {
  int64_t carry = 0;
  for (int64_t i = 0; i < a->n; ++i) {
    int64_t x = a->d[i] * m + carry;
    a->d[i] = x & LIMB_MASK;
    carry = x >> BITS;
  }
  while (carry > 0) {
    reserve(a, a->n + 1);
    a->d[a->n++] = carry & LIMB_MASK;
    carry >>= BITS;
  }
  trim(a);
}

/* a += b * m */
static void add_mul(big *a, const big *b, int64_t m) {
  int64_t carry = 0, i;
  if (a->n < b->n) {
    reserve(a, b->n);
    for (i = a->n; i < b->n; ++i) a->d[i] = 0;
    a->n = b->n;
  }
  for (i = 0; i < b->n; ++i) {
    int64_t x = a->d[i] + b->d[i] * m + carry;
    a->d[i] = x & LIMB_MASK;
    carry = x >> BITS;
  }
  for (; carry > 0 && i < a->n; ++i) {
    int64_t x = a->d[i] + carry;
    a->d[i] = x & LIMB_MASK;
    carry = x >> BITS;
  }
  while (carry > 0) {
    reserve(a, a->n + 1);
    a->d[a->n++] = carry & LIMB_MASK;
    carry >>= BITS;
  }
}

/* a -= b * m, when b * m <= a */
static void sub_mul(big *a, const big *b, int64_t m) {
  int64_t borrow = 0, i;
  for (i = 0; i < b->n; ++i) {
    int64_t x = a->d[i] - b->d[i] * m - borrow;
    a->d[i] = x & LIMB_MASK;
    borrow = -(x >> BITS);
  }
  for (; borrow > 0; ++i) {
    int64_t x = a->d[i] - borrow;
    a->d[i] = x & LIMB_MASK;
    borrow = -(x >> BITS);
  }
  trim(a);
}

static int cmp(const big *a, const big *b);

/* a += b * m (m >= 0) with the sign of a in *neg: the sign of the term is term_neg. */
static void add_signed(big *a, int *neg, const big *b, int64_t m, int term_neg, big *tmp) {
  if (*neg == term_neg) {
    add_mul(a, b, m);
  } else {
    copy(tmp, b);
    mul_small(tmp, m);
    if (cmp(a, tmp) >= 0) {
      sub_mul(a, tmp, 1);
    } else {
      sub_mul(tmp, a, 1);
      copy(a, tmp);
      *neg = term_neg;
    }
  }
  if (a->n == 1 && a->d[0] == 0) *neg = 0;
}

static int cmp(const big *a, const big *b) {
  if (a->n != b->n) return a->n < b->n ? -1 : 1;
  for (int64_t i = a->n - 1; i >= 0; --i)
    if (a->d[i] != b->d[i]) return a->d[i] < b->d[i] ? -1 : 1;
  return 0;
}

static double top(const big *a, int64_t i) { return i >= 0 && i < a->n ? (double)a->d[i] : 0.0; }

/* floor(x / d) for a quotient below 2^30: estimated from the three leading limbs, then corrected. */
static int64_t quotient(const big *x, const big *d, big *tmp) {
  int64_t h = (x->n > d->n ? x->n : d->n) - 1;
  double fx = (top(x, h) * 1073741824.0 + top(x, h - 1)) * 1073741824.0 + top(x, h - 2);
  double fd = (top(d, h) * 1073741824.0 + top(d, h - 1)) * 1073741824.0 + top(d, h - 2);
  int64_t q = (int64_t)(fx / fd);
  copy(tmp, d);
  mul_small(tmp, q);
  while (cmp(tmp, x) > 0) {
    sub_mul(tmp, d, 1);
    --q;
  }
  add_mul(tmp, d, 1);
  while (cmp(tmp, x) <= 0) {
    add_mul(tmp, d, 1);
    ++q;
  }
  return q;
}

int main(void) {
  long long t = bench_ms();
  int n = 1500, i = 0, neg = 0, xneg;
  int64_t k = 0;
  big numer = {0}, accum = {0}, denom = {0}, x = {0}, tmp = {0};
  char *digits = calloc((size_t)n + 1, 1);
  set_small(&numer, 1);
  set_small(&accum, 0);
  set_small(&denom, 1);
  while (i < n) {
    ++k;
    add_signed(&accum, &neg, &numer, 2, 0, &tmp);
    mul_small(&accum, 2 * k + 1);
    mul_small(&denom, 2 * k + 1);
    mul_small(&numer, k);
    if (neg || cmp(&numer, &accum) > 0) continue;
    copy(&x, &accum);
    xneg = neg;
    add_signed(&x, &xneg, &numer, 3, 0, &tmp);
    int64_t d = quotient(&x, &denom, &tmp);
    add_signed(&x, &xneg, &numer, 1, 0, &tmp);
    if (d != quotient(&x, &denom, &tmp)) continue;
    digits[i++] = (char)('0' + d);
    add_signed(&accum, &neg, &denom, d, 1, &tmp);
    mul_small(&accum, 10);
    mul_small(&numer, 10);
  }
  int sum = 0;
  for (int j = 0; j < n; ++j) sum += digits[j] - '0';
  int ok = strncmp(digits, "31415926535897932384626433832795028841971693993751", 50) == 0 &&
           strcmp(digits + n - 50, "55570674983850549458858692699569092721079750930295") == 0 && sum == 6775;
  free(digits);
  free(numer.d);
  free(accum.d);
  free(denom.d);
  free(x.d);
  free(tmp.d);
  bench_report("pidigits", t, ok);
  return 0;
}
