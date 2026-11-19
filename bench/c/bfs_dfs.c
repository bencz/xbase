/* bench/bfs_dfs.prg in C: BFS from 6 sources and an iterative DFS over every component of an undirected graph of
 * 50,000 vertices and 150,000 random edges from an LCG, 2 times. */
#include "bench.h"
#include <stdbool.h>
#include <stdlib.h>

static int64_t bfs(int64_t src, int64_t nV, const int64_t *first, const int64_t *adj) {
  int64_t *level = malloc(sizeof(int64_t) * (size_t)(nV + 1)), *queue = malloc(sizeof(int64_t) * (size_t)(nV + 1));
  for (int64_t i = 1; i <= nV; ++i) level[i] = -1;
  int64_t head = 1, tail = 1, sum = 0;
  level[src] = 0;
  queue[1] = src;
  while (head <= tail) {
    int64_t u = queue[head++], d = level[u];
    sum += d + 1;
    for (int64_t e = first[u]; e <= first[u + 1] - 1; ++e) {
      int64_t v = adj[e];
      if (level[v] < 0) {
        level[v] = d + 1;
        queue[++tail] = v;
      }
    }
  }
  free(level);
  free(queue);
  return sum;
}

static int64_t dfs(int64_t nV, int64_t nAdj, const int64_t *first, const int64_t *adj) {
  bool *seen = calloc((size_t)nV + 1, 1);
  int64_t *stack = malloc(sizeof(int64_t) * (size_t)(2 * nAdj + nV + 1));
  int64_t order = 0, comps = 0, sum = 0;
  for (int64_t s = 1; s <= nV; ++s) {
    if (seen[s]) continue;
    ++comps;
    int64_t top = 1;
    stack[1] = s;
    while (top > 0) {
      int64_t u = stack[top--];
      if (seen[u]) continue;
      seen[u] = true;
      ++order;
      sum += u * order;
      for (int64_t e = first[u + 1] - 1; e >= first[u]; --e)
        if (!seen[adj[e]]) stack[++top] = adj[e];
    }
  }
  free(seen);
  free(stack);
  return comps * 1000000000 + sum;
}

int main(void) {
  long long t = bench_ms();
  int64_t nV = 50000, nE = 150000, seed = 3, total = 0;
  int64_t *eu = malloc(sizeof(int64_t) * (size_t)(nE + 1)), *ev = malloc(sizeof(int64_t) * (size_t)(nE + 1));
  int64_t *first = calloc((size_t)nV + 2, sizeof(int64_t)), *fill = malloc(sizeof(int64_t) * (size_t)(nV + 2));
  int64_t *adj = malloc(sizeof(int64_t) * (size_t)(2 * nE + 1));
  for (int64_t i = 1; i <= nE; ++i) {
    seed = seed * 16807 % 2147483647;
    eu[i] = (seed >> 4) % nV + 1;
    seed = seed * 16807 % 2147483647;
    ev[i] = (seed >> 4) % nV + 1;
    first[eu[i]]++;
    first[ev[i]]++;
  }
  int64_t pos = 1;
  for (int64_t i = 1; i <= nV + 1; ++i) {
    int64_t d = first[i];
    first[i] = pos;
    fill[i] = pos;
    pos += d;
  }
  for (int64_t i = 1; i <= nE; ++i) {
    adj[fill[eu[i]]++] = ev[i];
    adj[fill[ev[i]]++] = eu[i];
  }
  for (int rep = 1; rep <= 2; ++rep) {
    for (int64_t src = 1; src <= 6; ++src) total += bfs((src - 1) * 8000 + 1, nV, first, adj);
    total += dfs(nV, 2 * nE, first, adj);
  }
  free(eu);
  free(ev);
  free(first);
  free(fill);
  free(adj);
  bench_report("bfs_dfs", t, total == 62828511998050);
  return 0;
}
