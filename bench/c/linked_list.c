/* bench/linked_list.prg in C: a singly linked list of nodes { value, next }: 1,000,000 nodes pushed at the head,
 * walked, reversed in place, walked again, and every second node unlinked (and freed); 3 rounds, each list freed at its
 * end. */
#include "bench.h"
#include <stdlib.h>

typedef struct Node Node;
struct Node {
  int64_t value;
  Node *next;
};

int main(void) {
  long long t = bench_ms();
  int64_t sum = 0, count = 0, first = 0;
  for (int r = 1; r <= 3; ++r) {
    Node *head = NULL, *node, *prev = NULL;
    for (int64_t i = 1; i <= 1000000; ++i) {
      node = malloc(sizeof *node);
      *node = (Node){i, head};
      head = node;
    }
    for (node = head; node; node = node->next) sum += node->value;
    for (node = head; node;) {
      Node *next = node->next;
      node->next = prev;
      prev = node;
      node = next;
    }
    head = prev;
    first += head->value;
    for (node = head; node && node->next; node = node->next) {
      Node *gone = node->next;
      node->next = gone->next;
      free(gone);
    }
    for (node = head; node; node = node->next) ++count;
    while (head) {
      node = head->next;
      free(head);
      head = node;
    }
  }
  bench_report("linked_list", t, sum == 1500001500000LL && first == 3 && count == 1500000);
  return 0;
}
