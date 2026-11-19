/* bench/pipeline_tasks.prg in C: a pipeline of four stages on threads of their own (the program's ASYNC functions)
 * joined by bounded channels (capacity 256; a queue under a mutex and condition variables, as C has none of its own):
 * a source sends 200,000 records as text ("id;value", each a new string, the values from a fixed seed), a parser splits
 * them into { id, value }, a stage maps each value to its square modulo 1,000,003 and keeps the even ones, and a sink
 * sums them. Each stage closes its output when its input ends. The sums must be the same whatever the scheduling. */
#include "bench.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CAP 256

typedef struct {
  char *text;
  int64_t id, value;
} Item;

typedef struct {
  pthread_mutex_t lock;
  pthread_cond_t not_full, not_empty;
  Item items[CAP];
  int head, count, closed;
} Channel;

static void channel_init(Channel *c) {
  pthread_mutex_init(&c->lock, NULL);
  pthread_cond_init(&c->not_full, NULL);
  pthread_cond_init(&c->not_empty, NULL);
  c->head = c->count = c->closed = 0;
}

static void channel_free(Channel *c) {
  pthread_cond_destroy(&c->not_full);
  pthread_cond_destroy(&c->not_empty);
  pthread_mutex_destroy(&c->lock);
}

static void send(Channel *c, Item x) {
  pthread_mutex_lock(&c->lock);
  while (c->count == CAP) pthread_cond_wait(&c->not_full, &c->lock);
  c->items[(c->head + c->count++) % CAP] = x;
  pthread_cond_signal(&c->not_empty);
  pthread_mutex_unlock(&c->lock);
}

/* Whether an item came: 0 once the channel is closed and empty. */
static int receive(Channel *c, Item *x) {
  pthread_mutex_lock(&c->lock);
  while (c->count == 0 && !c->closed) pthread_cond_wait(&c->not_empty, &c->lock);
  int got = c->count > 0;
  if (got) {
    *x = c->items[c->head];
    c->head = (c->head + 1) % CAP;
    --c->count;
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

static Channel c1, c2, c3;
static int64_t sink[3]; /* the sum of the values, their count, the sum of their ids */

static void *source(void *arg) {
  int64_t n = (int64_t)(intptr_t)arg, x = 4321;
  char text[32];
  for (int64_t i = 1; i <= n; ++i) {
    x = x * 16807 % 2147483647;
    int len = snprintf(text, sizeof text, "%lld;%lld", (long long)i, (long long)(x % 100000));
    char *s = malloc((size_t)len + 1);
    memcpy(s, text, (size_t)len + 1);
    send(&c1, (Item){s, 0, 0});
  }
  channel_close(&c1);
  return NULL;
}

static void *parser(void *arg) {
  (void)arg;
  Item c;
  while (receive(&c1, &c)) {
    const char *semi = strchr(c.text, ';');
    Item a = {NULL, strtoll(c.text, NULL, 10), strtoll(semi + 1, NULL, 10)};
    free(c.text);
    send(&c2, a);
  }
  channel_close(&c2);
  return NULL;
}

static void *square(void *arg) {
  (void)arg;
  Item a;
  while (receive(&c2, &a)) {
    int64_t y = a.value * a.value % 1000003;
    if (y % 2 == 0) send(&c3, (Item){NULL, a.id, y});
  }
  channel_close(&c3);
  return NULL;
}

static void *sink_stage(void *arg) {
  (void)arg;
  Item a;
  while (receive(&c3, &a)) {
    sink[0] += a.value;
    sink[1]++;
    sink[2] += a.id;
  }
  return NULL;
}

int main(void) {
  long long t = bench_ms();
  pthread_t th[4];
  channel_init(&c1);
  channel_init(&c2);
  channel_init(&c3);
  pthread_create(&th[0], NULL, source, (void *)(intptr_t)200000);
  pthread_create(&th[1], NULL, parser, NULL);
  pthread_create(&th[2], NULL, square, NULL);
  pthread_create(&th[3], NULL, sink_stage, NULL);
  pthread_join(th[3], NULL);
  for (int k = 0; k < 3; ++k) pthread_join(th[k], NULL);
  channel_free(&c1);
  channel_free(&c2);
  channel_free(&c3);
  bench_report("pipeline_tasks", t, sink[0] == 49893566082LL && sink[1] == 100073 && sink[2] == 10032092994LL);
  return 0;
}
