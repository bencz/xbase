/* bench/trie.prg in C: a trie of hash tables, one per node, from a letter to the node { children, words } (open
 * addressing from 8 slots, doubling as it fills): 100,000 words of 3 to 10 letters from an 8-letter alphabet inserted,
 * then 100,000 other words of the same kind looked up, as words and as prefixes of the words stored. */
#include "bench.h"
#include <stdlib.h>
#include <string.h>

typedef struct Node Node;

typedef struct {
  char key;
  Node *child; /* NULL for a free slot */
} Slot;

struct Node {
  Slot *slots;
  int64_t count, cap, words;
};

static int64_t seed = 2718;
static int64_t rnd(int64_t n) {
  seed = seed * 16807 % 2147483647;
  return seed % n;
}

static Node *node_new(void) {
  Node *n = malloc(sizeof *n);
  *n = (Node){calloc(8, sizeof(Slot)), 0, 8, 0};
  return n;
}

static uint64_t mix(uint64_t k) {
  k ^= k >> 33;
  k *= 0xff51afd7ed558ccdULL;
  k ^= k >> 33;
  return k;
}

static Slot *find(const Node *n, char key) {
  uint64_t i = mix((unsigned char)key) & (uint64_t)(n->cap - 1);
  while (n->slots[i].child && n->slots[i].key != key) i = (i + 1) & (uint64_t)(n->cap - 1);
  return &n->slots[i];
}

static Node *add(Node *n, char key) {
  if ((n->count + 1) * 4 > n->cap * 3) {
    Slot *old = n->slots;
    int64_t cap = n->cap;
    n->cap *= 2;
    n->slots = calloc((size_t)n->cap, sizeof *n->slots);
    for (int64_t i = 0; i < cap; ++i)
      if (old[i].child) *find(n, old[i].key) = old[i];
    free(old);
  }
  Slot *s = find(n, key);
  s->key = key;
  s->child = node_new();
  ++n->count;
  return s->child;
}

static void node_free(Node *n) {
  for (int64_t i = 0; i < n->cap; ++i)
    if (n->slots[i].child) node_free(n->slots[i].child);
  free(n->slots);
  free(n);
}

static void new_word(char *w) {
  int64_t n = rnd(8) + 3, i;
  for (i = 0; i < n; ++i) w[i] = "etaoinsr"[rnd(rnd(8) + 1)];
  w[i] = '\0';
}

int main(void) {
  int64_t len = 100000, nodes = 1, found = 0, prefix = 0;
  char(*words)[12] = malloc((size_t)len * sizeof *words), (*probes)[12] = malloc((size_t)len * sizeof *probes);
  for (int64_t i = 0; i < len; ++i) {
    new_word(words[i]);
    new_word(probes[i]);
  }
  long long t = bench_ms();
  Node *root = node_new();
  for (int64_t i = 0; i < len; ++i) {
    Node *node = root;
    for (const char *c = words[i]; *c; ++c) {
      Slot *s = find(node, *c);
      if (s->child)
        node = s->child;
      else {
        node = add(node, *c);
        ++nodes;
      }
    }
    ++node->words;
  }
  for (int64_t i = 0; i < len; ++i) {
    Node *node = root;
    for (const char *c = probes[i]; *c && node; ++c) node = find(node, *c)->child;
    if (node) {
      ++prefix;
      if (node->words > 0) ++found;
    }
  }
  bench_report("trie", t, nodes == 159557 && found == 40767 && prefix == 50841);
  node_free(root);
  free(words);
  free(probes);
  return 0;
}
