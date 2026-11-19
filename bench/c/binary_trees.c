/* bench/binary_trees.prg in C: binary trees made and walked, depths 4 to 14 (a stretch tree of depth 15, one of
 * depth 14 that lives throughout, 2^(14 - d + 4) trees of each even depth d), each node allocated and freed. */
#include "bench.h"
#include <stdlib.h>

typedef struct node { struct node *left, *right; } node;

static node *make(int64_t d) {
  node *n = malloc(sizeof *n);
  if (d == 0) {
    n->left = n->right = NULL;
  } else {
    n->left = make(d - 1);
    n->right = make(d - 1);
  }
  return n;
}

static int64_t check(const node *n) {
  if (n->left == NULL) return 1;
  return 1 + check(n->left) + check(n->right);
}

static void release(node *n) {
  if (n->left != NULL) {
    release(n->left);
    release(n->right);
  }
  free(n);
}

int main(void) {
  long long t = bench_ms();
  int64_t maxDepth = 14, total = 0;
  node *n = make(maxDepth + 1);
  total += check(n);
  release(n);
  node *longLived = make(maxDepth);
  for (int64_t d = 4; d <= maxDepth; d += 2) {
    int64_t iters = (int64_t)1 << (maxDepth - d + 4);
    for (int64_t i = 1; i <= iters; ++i) {
      n = make(d);
      total += check(n);
      release(n);
    }
  }
  total += check(longLived);
  release(longLived);
  bench_report("binary_trees", t, total == 3222190);
  return 0;
}
