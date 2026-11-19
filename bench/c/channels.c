/* bench/channels.prg in C: 4 producer threads send 250,000 numbers each through a bounded channel (capacity 64) to 2
 * consumer threads (a send waits while it is full, a receive while it is empty), then 1,000,000 send/receive pairs
 * without waiting on an unbounded one from one thread. A channel is a queue under a mutex and condition variables
 * (C has none of its own); the program's tasks are threads. */
#include "bench.h"
#include <pthread.h>
#include <stdlib.h>

typedef struct {
  pthread_mutex_t lock;
  pthread_cond_t not_full, not_empty;
  int64_t *items, head, count, cap; /* cap 0: unbounded, the store doubling as it fills */
  int64_t size;
  int closed;
} Channel;

static void channel_init(Channel *c, int64_t cap) {
  pthread_mutex_init(&c->lock, NULL);
  pthread_cond_init(&c->not_full, NULL);
  pthread_cond_init(&c->not_empty, NULL);
  c->cap = cap;
  c->size = cap ? cap : 8;
  c->items = malloc((size_t)c->size * sizeof *c->items);
  c->head = c->count = 0;
  c->closed = 0;
}

static void channel_free(Channel *c) {
  pthread_cond_destroy(&c->not_full);
  pthread_cond_destroy(&c->not_empty);
  pthread_mutex_destroy(&c->lock);
  free(c->items);
}

static void push(Channel *c, int64_t x) {
  if (c->count == c->size) {
    int64_t *items = malloc((size_t)c->size * 2 * sizeof *items);
    for (int64_t i = 0; i < c->count; ++i) items[i] = c->items[(c->head + i) % c->size];
    free(c->items);
    c->items = items;
    c->head = 0;
    c->size *= 2;
  }
  c->items[(c->head + c->count++) % c->size] = x;
}

static int64_t pop(Channel *c) {
  int64_t x = c->items[c->head];
  c->head = (c->head + 1) % c->size;
  --c->count;
  return x;
}

static void send(Channel *c, int64_t x) {
  pthread_mutex_lock(&c->lock);
  while (c->cap && c->count == c->cap) pthread_cond_wait(&c->not_full, &c->lock);
  push(c, x);
  pthread_cond_signal(&c->not_empty);
  pthread_mutex_unlock(&c->lock);
}

/* Whether a value came: 0 once the channel is closed and empty. */
static int receive(Channel *c, int64_t *x) {
  pthread_mutex_lock(&c->lock);
  while (c->count == 0 && !c->closed) pthread_cond_wait(&c->not_empty, &c->lock);
  int got = c->count > 0;
  if (got) {
    *x = pop(c);
    pthread_cond_signal(&c->not_full);
  }
  pthread_mutex_unlock(&c->lock);
  return got;
}

static int try_send(Channel *c, int64_t x) {
  pthread_mutex_lock(&c->lock);
  int ok = !c->closed && (!c->cap || c->count < c->cap);
  if (ok) {
    push(c, x);
    pthread_cond_signal(&c->not_empty);
  }
  pthread_mutex_unlock(&c->lock);
  return ok;
}

static int try_receive(Channel *c, int64_t *x) {
  pthread_mutex_lock(&c->lock);
  int got = c->count > 0;
  if (got) {
    *x = pop(c);
    pthread_cond_signal(&c->not_full);
  }
  pthread_mutex_unlock(&c->lock);
  return got;
}

static void channel_close(Channel *c) {
  pthread_mutex_lock(&c->lock);
  c->closed = 1;
  pthread_cond_broadcast(&c->not_empty);
  pthread_mutex_unlock(&c->lock);
}

static Channel ch;
static int64_t sums[2];

static void *produce(void *arg) {
  (void)arg;
  for (int64_t i = 1; i <= 250000; ++i) send(&ch, i);
  return NULL;
}

static void *consume(void *arg) {
  int64_t s = 0, x;
  while (receive(&ch, &x)) s += x;
  sums[(intptr_t)arg] = s;
  return NULL;
}

int main(void) {
  long long t = bench_ms();
  pthread_t consumers[2], producers[4];
  int64_t s = 0, slot = 0;
  Channel u;
  channel_init(&ch, 64);
  for (int k = 0; k < 2; ++k) pthread_create(&consumers[k], NULL, consume, (void *)(intptr_t)k);
  for (int k = 0; k < 4; ++k) pthread_create(&producers[k], NULL, produce, NULL);
  for (int k = 0; k < 4; ++k) pthread_join(producers[k], NULL);
  channel_close(&ch);
  for (int k = 0; k < 2; ++k) {
    pthread_join(consumers[k], NULL);
    s += sums[k];
  }
  channel_init(&u, 0);
  for (int64_t k = 1; k <= 1000000; ++k) {
    try_send(&u, k);
    try_receive(&u, &slot);
    s += slot % 2;
  }
  bench_report("channels", t, s == 4 * 31250125000LL + 500000);
  channel_free(&u);
  channel_free(&ch);
  return 0;
}
