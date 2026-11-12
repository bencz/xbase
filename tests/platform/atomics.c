/* Atomic operations under contention: bits of one byte and of one word set and cleared by eight threads at once are
 * never lost, compare-and-swap increments add up, a list pushed to by every thread holds every node once, exchanges
 * hand each value to exactly one thread; and each operation's result alone. */
#include "check.h"
#include "core/sync.h"

#define THREADS 8
#define ROUNDS 100000u
#define NODES 2000u

typedef struct node {
  struct node *next;
} node;

static uint8_t bits, seen8[THREADS];
static uint32_t counter, ticket, taken[THREADS], word, seen32[THREADS];
static void *token;
static uint32_t tokens[THREADS];
static void *list;
static node nodes[THREADS][NODES];

static void work(void *arg) {
  size_t k = (size_t)arg;
  uint8_t mine = (uint8_t)(1u << k);
  for (uint32_t i = 0; i < ROUNDS; ++i) {
    if (xb_atomic_or8(&bits, mine, XB_ACQ_REL) & mine) seen8[k] = 1; /* nobody else sets it */
    if (!(xb_atomic_and8(&bits, (uint8_t)~mine, XB_ACQ_REL) & mine)) seen8[k] = 1;
    uint32_t mine32 = 1u << (k + 20);
    if (xb_atomic_or32(&word, mine32, XB_ACQ_REL) & mine32) seen32[k] = 1;
    if (!(xb_atomic_and32(&word, ~mine32, XB_ACQ_REL) & mine32)) seen32[k] = 1;
    uint32_t old = xb_atomic_load32(&counter, XB_RELAXED);
    while (!xb_atomic_cas32(&counter, &old, old + 1, XB_ACQ_REL)) {
    }
  }
  for (uint32_t i = 0; i < NODES; ++i) {
    node *n = &nodes[k][i];
    void *head = xb_atomic_loadptr(&list, XB_RELAXED);
    do {
      n->next = head;
    } while (!xb_atomic_casptr(&list, &head, n, XB_RELEASE));
    if (xb_atomic_xchg32(&ticket, 0, XB_ACQ_REL) == 1) ++taken[k];
    if (xb_atomic_xchgptr(&token, NULL, XB_ACQ_REL) == &token) ++tokens[k];
  }
}

int main(void) {
  xb_platform_init();
  xb_thread threads[THREADS];
  ticket = 1;
  xb_atomic_storeptr(&token, &token, XB_RELEASE);
  for (size_t k = 0; k < THREADS; ++k) CHECK(xb_thread_start(&threads[k], 0, work, (void *)k));
  for (size_t k = 0; k < THREADS; ++k) xb_thread_join(threads[k]);
  CHECK(bits == 0);
  uint32_t wins = 0, tokenWins = 0;
  for (size_t k = 0; k < THREADS; ++k) {
    CHECK(!seen8[k] && !seen32[k]);
    wins += taken[k];
    tokenWins += tokens[k];
  }
  CHECK(wins == 1 && tokenWins == 1 && word == 0);
  CHECK(counter == THREADS * ROUNDS);
  size_t n = 0;
  for (node *p = xb_atomic_loadptr(&list, XB_ACQUIRE); p; p = p->next) ++n;
  CHECK(n == (size_t)THREADS * NODES);
  size_t total = 0;
  CHECK(xb_atomic_addsz(&total, 3, XB_RELAXED) == 0 && xb_atomic_loadsz(&total, XB_RELAXED) == 3);
  xb_atomic_storesz(&total, 7, XB_SEQ_CST);
  CHECK(total == 7);
  uint8_t byte = 0;
  xb_atomic_store8(&byte, 0x5A, XB_RELEASE);
  xb_atomic_fence(XB_SEQ_CST);
  CHECK(xb_atomic_load8(&byte, XB_ACQUIRE) == 0x5A);
  uint32_t expected = 1, v = 2;
  CHECK(!xb_atomic_cas32(&v, &expected, 3, XB_ACQ_REL) && expected == 2 && v == 2);
  CHECK(xb_atomic_cas32(&v, &expected, 3, XB_ACQ_REL) && v == 3);
  CHECK(xb_atomic_xchg32(&v, 9, XB_SEQ_CST) == 3 && xb_atomic_load32(&v, XB_SEQ_CST) == 9);
  puts("atomics ok");
  return 0;
}
