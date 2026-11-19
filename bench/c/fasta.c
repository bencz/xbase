/* bench/fasta.prg in C: the three sequences of the fasta benchmark for n = 1,500,000 (the ALU repeated over 2n
 * characters, 3n IUB codes and 5n Homo sapiens bases from the LCG), in lines of 60, into one buffer. */
#include "bench.h"
#include <stdlib.h>
#include <string.h>

#define IM 139968
#define IA 3877
#define IC 29573
#define LINE 60

static const char alu[] =
  "GGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTC"
  "AGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCG"
  "TGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGG"
  "AGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAA";

static char *out;
static int64_t len, seed = 42, picks;

static void put(const char *s, int64_t n) {
  memcpy(out + len, s, (size_t)n);
  len += n;
}

static void repeat(const char *head, int64_t n) {
  int64_t k = (int64_t)strlen(alu), pos = 0;
  put(head, (int64_t)strlen(head));
  for (int64_t done = 0; done < n; done += LINE) {
    int64_t m = n - done < LINE ? n - done : LINE;
    for (int64_t i = 0; i < m; ++i) {
      out[len++] = alu[pos++];
      if (pos == k) pos = 0;
    }
    out[len++] = '\n';
  }
}

static void random_seq(const char *head, const char *sym, const double *prob, int nsym, int64_t n) {
  double cum[16], acc = 0;
  for (int i = 0; i < nsym; ++i) cum[i] = acc += prob[i];
  put(head, (int64_t)strlen(head));
  for (int64_t done = 0; done < n; done += LINE) {
    int64_t m = n - done < LINE ? n - done : LINE;
    for (int64_t i = 0; i < m; ++i) {
      seed = (seed * IA + IC) % IM;
      double r = (double)seed / IM;
      int j = 0;
      while (j < nsym - 1 && r >= cum[j]) ++j;
      picks += j;
      out[len++] = sym[j];
    }
    out[len++] = '\n';
  }
}

int main(void) {
  long long t = bench_ms();
  int64_t n = 1500000;
  static const double iub[] = {0.27, 0.12, 0.12, 0.27,
                               0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02};
  static const double homo[] = {0.3029549426680, 0.1979883004921, 0.1975473066391, 0.3015094502008};
  out = malloc((size_t)(n * 11 + 1000));
  repeat(">ONE Homo sapiens alu\n", 2 * n);
  random_seq(">TWO IUB ambiguity codes\n", "acgtBDHKMNRSVWY", iub, 15, 3 * n);
  random_seq(">THREE Homo sapiens frequency\n", "acgt", homo, 4, 5 * n);
  out[len] = 0;
  char *two = strstr(out, ">TWO");
  int ok = len == 15250077 && seed == 28650 && picks == 25406718 && two - out == 3050022 &&
           memcmp(two - 21, "TGGGCGACAGAGCGAGACTC\n", 21) == 0 &&
           memcmp(out + len - 21, "acacatatagttttatacaa\n", 21) == 0;
  free(out);
  bench_report("fasta", t, ok);
  return 0;
}
