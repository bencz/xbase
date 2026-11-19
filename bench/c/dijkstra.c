/* bench/dijkstra.prg in C: Dijkstra's shortest paths with a binary heap of (distance, vertex) pairs on a graph of
 * 20,000 vertices and 120,000 edges from an LCG, from 12 sources. */
#include "bench.h"
#include <stdbool.h>
#include <stdlib.h>

static int64_t dijkstra(int64_t src, int64_t nV, int64_t nE, const int64_t *first, const int64_t *aTo,
                        const int64_t *aW) {
  int64_t *dist = malloc(sizeof(int64_t) * (size_t)(nV + 1));
  bool *done = malloc((size_t)nV + 1);
  int64_t *hD = malloc(sizeof(int64_t) * (size_t)(nE + 2)), *hV = malloc(sizeof(int64_t) * (size_t)(nE + 2));
  for (int64_t i = 1; i <= nV; ++i) {
    dist[i] = -1;
    done[i] = false;
  }
  int64_t nH = 1, sum = 0;
  dist[src] = 0;
  hD[1] = 0;
  hV[1] = src;
  while (nH > 0) {
    int64_t d = hD[1], u = hV[1], x = hD[nH], y = hV[nH], i = 1, c;
    --nH;
    while ((c = i * 2) <= nH) {
      if (c < nH && hD[c + 1] < hD[c]) ++c;
      if (hD[c] >= x) break;
      hD[i] = hD[c];
      hV[i] = hV[c];
      i = c;
    }
    hD[i] = x;
    hV[i] = y;
    if (done[u]) continue;
    done[u] = true;
    sum += d;
    for (int64_t e = first[u]; e <= first[u + 1] - 1; ++e) {
      int64_t v = aTo[e], nd = d + aW[e];
      if (!done[v] && (dist[v] < 0 || nd < dist[v])) {
        dist[v] = nd;
        i = ++nH;
        while (i > 1 && hD[c = i >> 1] > nd) {
          hD[i] = hD[c];
          hV[i] = hV[c];
          i = c;
        }
        hD[i] = nd;
        hV[i] = v;
      }
    }
  }
  free(dist);
  free(done);
  free(hD);
  free(hV);
  return sum;
}

int main(void) {
  long long t = bench_ms();
  int64_t nV = 20000, nOut = 6, nE = nV * nOut, seed = 7, e = 0, total = 0;
  int64_t *first = malloc(sizeof(int64_t) * (size_t)(nV + 2));
  int64_t *aTo = malloc(sizeof(int64_t) * (size_t)(nE + 1)), *aW = malloc(sizeof(int64_t) * (size_t)(nE + 1));
  for (int64_t i = 1; i <= nV; ++i) {
    first[i] = e + 1;
    ++e;
    aTo[e] = i == nV ? 1 : i + 1;
    seed = seed * 16807 % 2147483647;
    aW[e] = (seed >> 8) % 100 + 1;
    for (int64_t k = 2; k <= nOut; ++k) {
      ++e;
      seed = seed * 16807 % 2147483647;
      aTo[e] = (seed >> 4) % nV + 1;
      seed = seed * 16807 % 2147483647;
      aW[e] = (seed >> 8) % 100 + 1;
    }
  }
  first[nV + 1] = e + 1;
  for (int64_t src = 1; src <= 12; ++src) total += dijkstra((src - 1) * 1600 + 1, nV, nE, first, aTo, aW);
  free(first);
  free(aTo);
  free(aW);
  bench_report("dijkstra", t, total == 41762116);
  return 0;
}
