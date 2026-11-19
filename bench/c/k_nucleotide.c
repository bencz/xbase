/* bench/k_nucleotide.prg in C: the Homo sapiens section of fasta for n = 100,000 (500,000 bases, upper case, made
 * before the clock starts), every k-mer of it counted for k = 1, 2, 3, 4, 6, 12 and 18 (an open-addressing table
 * keyed by the k-mer's 2-bit code), the 1- and 2-mers sorted by count, and the counts of five given sequences. */
#include "bench.h"
#include <stdlib.h>
#include <string.h>

#define IM 139968
#define IA 3877
#define IC 29573
#define SLOTS (1 << 21)

typedef struct {
  int64_t key; /* the 2-bit code plus 1; 0 for a free slot */
  int64_t count;
} slot;

static slot table[SLOTS];
static int64_t used[SLOTS], nused;

static int64_t code_of(char c) { return c == 'A' ? 0 : c == 'C' ? 1 : c == 'G' ? 2 : 3; }

static slot *find(int64_t key) {
  uint64_t h = ((uint64_t)key * 0x9E3779B97F4A7C15u) >> 43;
  while (table[h].key != 0 && table[h].key != key) h = (h + 1) & (SLOTS - 1);
  return &table[h];
}

/* Counts every k-mer of s into the table, emptied first. */
static void count_frames(const char *s, int64_t n, int k) {
  for (int64_t i = 0; i < nused; ++i) table[used[i]].key = 0;
  nused = 0;
  int64_t mask = ((int64_t)1 << (2 * k)) - 1, code = 0;
  for (int64_t i = 0; i < n; ++i) {
    code = ((code << 2) | code_of(s[i])) & mask;
    if (i + 1 < k) continue;
    slot *e = find(code + 1);
    if (e->key == 0) {
      e->key = code + 1;
      e->count = 0;
      used[nused++] = e - table;
    }
    ++e->count;
  }
}

static int64_t count_of(const char *s, int64_t n, const char *seq) {
  int k = (int)strlen(seq);
  int64_t code = 0;
  count_frames(s, n, k);
  for (int i = 0; i < k; ++i) code = (code << 2) | code_of(seq[i]);
  slot *e = find(code + 1);
  return e->key == 0 ? 0 : e->count;
}

/* The k-mers (k = 1 or 2) by count, then by name: "A 150000 T 149000 ...". */
static void sorted(const char *s, int64_t n, int k, char *out) {
  static const char sym[] = "ACGT";
  int m = 1 << (2 * k), idx[16];
  int64_t cnt[16];
  count_frames(s, n, k);
  for (int c = 0; c < m; ++c) {
    slot *e = find(c + 1);
    cnt[c] = e->key == 0 ? 0 : e->count;
    idx[c] = c;
  }
  for (int i = 1; i < m; ++i)
    for (int j = i; j > 0 && cnt[idx[j]] > cnt[idx[j - 1]]; --j) {
      int x = idx[j];
      idx[j] = idx[j - 1];
      idx[j - 1] = x;
    }
  *out = 0;
  for (int i = 0; i < m; ++i) {
    char name[3] = {0};
    for (int j = 0; j < k; ++j) name[j] = sym[(idx[i] >> (2 * (k - 1 - j))) & 3];
    sprintf(out + strlen(out), "%s%s %lld", i ? " " : "", name, (long long)cnt[idx[i]]);
  }
}

int main(void) {
  int64_t n = 100000, seed = 42, len = 5 * n;
  static const double homo[] = {0.3029549426680, 0.1979883004921, 0.1975473066391, 0.3015094502008};
  double cum[4], acc = 0;
  char *s = malloc((size_t)len), one[200], two[400];
  for (int i = 0; i < 4; ++i) cum[i] = acc += homo[i];
  for (int64_t i = 0; i < 3 * n; ++i) seed = (seed * IA + IC) % IM;
  for (int64_t i = 0; i < len; ++i) {
    seed = (seed * IA + IC) % IM;
    double r = (double)seed / IM;
    int j = 0;
    while (j < 3 && r >= cum[j]) ++j;
    s[i] = "ACGT"[j];
  }
  long long t = bench_ms();
  sorted(s, len, 1, one);
  sorted(s, len, 2, two);
  int64_t c3 = count_of(s, len, "GGT"), c4 = count_of(s, len, "GGTA"), c6 = count_of(s, len, "GGTATT"),
          c12 = count_of(s, len, "GGTATTTTAATT"), c18 = count_of(s, len, "GGTATTTTAATTTATAGT");
  int ok = strcmp(one, "A 151655 T 150452 C 99156 G 98737") == 0 &&
           strcmp(two, "AA 45992 TA 45599 AT 45574 TT 45299 CA 30129 AC 30096 AG 29993 GA 29935 CT 29857 TC 29832 "
                       "GT 29722 TG 29721 GC 19617 CC 19611 CG 19559 GG 19463") == 0 &&
           c3 == 5871 && c4 == 1769 && c6 == 185 && c12 == 4 && c18 == 4;
  free(s);
  bench_report("k_nucleotide", t, ok);
  return 0;
}
