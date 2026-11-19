/* bench/reverse_complement.prg in C: the output of fasta for n = 2,500,000 (made before the clock starts) read
 * section by section, each sequence joined, reverse-complemented through the IUB table and written again in lines
 * of 60 after its header. */
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

static char *in, *out, *seq, comp[256];
static int64_t inlen, outlen, seed = 42;

static void put(const char *s, int64_t n) {
  memcpy(in + inlen, s, (size_t)n);
  inlen += n;
}

static void repeat(const char *head, int64_t n) {
  int64_t k = (int64_t)strlen(alu), pos = 0;
  put(head, (int64_t)strlen(head));
  for (int64_t done = 0; done < n; done += LINE) {
    int64_t m = n - done < LINE ? n - done : LINE;
    for (int64_t i = 0; i < m; ++i) {
      in[inlen++] = alu[pos++];
      if (pos == k) pos = 0;
    }
    in[inlen++] = '\n';
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
      in[inlen++] = sym[j];
    }
    in[inlen++] = '\n';
  }
}

/* The sequence gathered so far, reverse-complemented, in lines of 60. */
static void flush(int64_t n) {
  int64_t col = 0;
  for (int64_t i = n - 1; i >= 0; --i) {
    out[outlen++] = comp[(unsigned char)seq[i]];
    if (++col == LINE) {
      out[outlen++] = '\n';
      col = 0;
    }
  }
  if (col > 0) out[outlen++] = '\n';
}

int main(void) {
  int64_t n = 2500000, nseq = 0;
  static const double iub[] = {0.27, 0.12, 0.12, 0.27, 0.02, 0.02, 0.02,
                               0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02};
  static const double homo[] = {0.3029549426680, 0.1979883004921, 0.1975473066391, 0.3015094502008};
  static const char from[] = "ACBDGHKMNSRUTWVYacbdghkmnsrutwvy", to[] = "TGVHCDMKNSYAABWRTGVHCDMKNSYAABWR";
  in = malloc((size_t)(n * 11 + 1000));
  repeat(">ONE Homo sapiens alu\n", 2 * n);
  random_seq(">TWO IUB ambiguity codes\n", "acgtBDHKMNRSVWY", iub, 15, 3 * n);
  random_seq(">THREE Homo sapiens frequency\n", "acgt", homo, 4, 5 * n);
  long long t = bench_ms();
  for (int i = 0; i < 256; ++i) comp[i] = (char)i;
  for (int i = 0; from[i]; ++i) comp[(unsigned char)from[i]] = to[i];
  out = malloc((size_t)inlen + 1);
  seq = malloc((size_t)inlen);
  for (int64_t i = 0; i < inlen;) {
    if (in[i] == '>') {
      if (nseq > 0) flush(nseq);
      nseq = 0;
      while (in[i] != '\n') out[outlen++] = in[i++];
      out[outlen++] = in[i++];
    } else if (in[i] == '\n') {
      ++i;
    } else {
      seq[nseq++] = in[i++];
    }
  }
  flush(nseq);
  out[outlen] = 0;
  char *two = strstr(out, ">TWO"), *three = strstr(out, ">THREE");
  int ok = outlen == 25416745 &&
           memcmp(out + 22, "GTAGCTGGGATTACAGGCGCGCGCCACCACGCCCGGCTAATTTTTGTATTTTTAGTAGAG", 60) == 0 &&
           memcmp(two + 25, "TTTATTYSTTGTATCCCTMWCGAACTAVGYHTATANTAWCARAWTTTVCTSTKDNTATHA", 60) == 0 &&
           memcmp(three + 30, "GTAATTTCTCTCGGCTATTAGATTTGAACATATACACAGGCCAGATGATCTGGAAAAATC", 60) == 0 &&
           memcmp(out + outlen - 21, "CTATAACTAATATATTCGTC\n", 21) == 0;
  free(in);
  free(out);
  free(seq);
  bench_report("reverse_complement", t, ok);
  return 0;
}
