/* bench/lru_cache.prg in C: a least-recently-used cache of 2,000 entries: a hash table from key to node and a doubly
 * linked list of nodes { key, value, prev, next } in order of use; 2,000,000 reads of keys skewed toward small numbers,
 * a miss computing the value and evicting (and freeing) the oldest entry when the cache is full. */
#include "bench.h"
#include <stdlib.h>

typedef struct Node Node;
struct Node {
  int64_t key, value;
  Node *prev, *next;
};

typedef struct {
  int64_t key;
  Node *node; /* NULL for a free slot */
} Slot;

typedef struct {
  Slot *slots;
  int64_t count, cap;
} Hash;

static int64_t seed = 1234;
static int64_t rnd(int64_t n) {
  seed = seed * 16807 % 2147483647;
  return seed % n;
}

static uint64_t mix(uint64_t k) {
  k ^= k >> 33;
  k *= 0xff51afd7ed558ccdULL;
  k ^= k >> 33;
  return k;
}

static Slot *find(const Hash *h, int64_t key) {
  uint64_t i = mix((uint64_t)key) & (uint64_t)(h->cap - 1);
  while (h->slots[i].node && h->slots[i].key != key) i = (i + 1) & (uint64_t)(h->cap - 1);
  return &h->slots[i];
}

static void set(Hash *h, int64_t key, Node *node) {
  if ((h->count + 1) * 4 > h->cap * 3) {
    Slot *old = h->slots;
    int64_t n = h->cap;
    h->cap *= 2;
    h->slots = calloc((size_t)h->cap, sizeof *h->slots);
    for (int64_t i = 0; i < n; ++i)
      if (old[i].node) *find(h, old[i].key) = old[i];
    free(old);
  }
  Slot *s = find(h, key);
  if (!s->node) ++h->count;
  *s = (Slot){key, node};
}

/* Linear probing's deletion: the entries after the hole that may move back are moved into it. */
static void del(Hash *h, int64_t key) {
  uint64_t mask = (uint64_t)(h->cap - 1), i = (uint64_t)(find(h, key) - h->slots);
  if (!h->slots[i].node) return;
  h->slots[i].node = NULL;
  --h->count;
  for (uint64_t j = (i + 1) & mask; h->slots[j].node; j = (j + 1) & mask) {
    uint64_t home = mix((uint64_t)h->slots[j].key) & mask;
    if (((j - home) & mask) >= ((j - i) & mask)) {
      h->slots[i] = h->slots[j];
      h->slots[j].node = NULL;
      i = j;
    }
  }
}

int main(void) {
  long long t = bench_ms();
  Hash h = {calloc(8, sizeof(Slot)), 0, 8};
  Node head = {0}, tail = {0};
  int64_t hits = 0, sum = 0, cap = 2000;
  head.next = &tail;
  tail.prev = &head;
  for (int64_t i = 1; i <= 2000000; ++i) {
    int64_t k = rnd(rnd(20000) + 1);
    Slot *s = find(&h, k);
    Node *node;
    if (s->node) {
      ++hits;
      node = s->node;
      node->prev->next = node->next;
      node->next->prev = node->prev;
    } else {
      if (h.count >= cap) {
        node = tail.prev;
        node->prev->next = &tail;
        tail.prev = node->prev;
        del(&h, node->key);
        free(node);
      }
      node = malloc(sizeof *node);
      *node = (Node){k, k * 3 + 1, NULL, NULL};
      set(&h, k, node);
    }
    node->next = head.next;
    node->prev = &head;
    head.next->prev = node;
    head.next = node;
    sum += node->value;
  }
  int ok = hits == 378837 && sum == 30033837356LL && h.count == cap;
  bench_report("lru_cache", t, ok);
  for (Node *n = head.next; n != &tail;) {
    Node *next = n->next;
    free(n);
    n = next;
  }
  free(h.slots);
  return 0;
}
