/* bench/chameneos_redux.prg in C: creatures on threads of their own ask for meetings on one channel; a broker pairs
 * them two by two and answers each on its own channel with the other's name and color, after which both change color;
 * 30,000 meetings with 3 creatures, then 30,000 with 10. A channel is a queue under a mutex and a condition variable
 * (C has none of its own). Checked: the color table, and per run 2N meetings counted, none with itself. */
#include "bench.h"
#include <pthread.h>
#include <string.h>

enum { BLUE, RED, YELLOW };
#define QUEUE 16

typedef struct {
  int id, color, done;
  struct channel *reply;
} message;

typedef struct channel {
  pthread_mutex_t lock;
  pthread_cond_t ready;
  message items[QUEUE];
  int head, count;
} channel;

static void channel_init(channel *c) {
  pthread_mutex_init(&c->lock, NULL);
  pthread_cond_init(&c->ready, NULL);
  c->head = c->count = 0;
}

static void channel_free(channel *c) {
  pthread_cond_destroy(&c->ready);
  pthread_mutex_destroy(&c->lock);
}

/* The queues never hold more than one message per creature: a send never waits. */
static void send(channel *c, message m) {
  pthread_mutex_lock(&c->lock);
  c->items[(c->head + c->count++) % QUEUE] = m;
  pthread_cond_signal(&c->ready);
  pthread_mutex_unlock(&c->lock);
}

static message receive(channel *c) {
  pthread_mutex_lock(&c->lock);
  while (c->count == 0) pthread_cond_wait(&c->ready, &c->lock);
  message m = c->items[c->head];
  c->head = (c->head + 1) % QUEUE;
  --c->count;
  pthread_mutex_unlock(&c->lock);
  return m;
}

typedef struct {
  channel *meet, reply;
  int id, color;
  int64_t met, same;
} creature;

static int complement(int a, int b) { return a == b ? a : 3 - a - b; }

static void *live(void *arg) {
  creature *c = arg;
  for (;;) {
    send(c->meet, (message){.id = c->id, .color = c->color, .reply = &c->reply});
    message other = receive(&c->reply);
    if (other.done) break;
    c->color = complement(c->color, other.color);
    ++c->met;
    if (other.id == c->id) ++c->same;
  }
  return NULL;
}

/* Whether n meetings of the creatures of these colors count 2n meetings, none with itself; the caller is the broker. */
static int run(int64_t n, const int *colors, int count) {
  channel meet;
  creature cs[10];
  pthread_t th[10];
  int64_t met = 0, same = 0, left = n;
  int done = 0, have_first = 0;
  message first = {0};
  channel_init(&meet);
  for (int i = 0; i < count; ++i) {
    cs[i] = (creature){.meet = &meet, .id = i + 1, .color = colors[i]};
    channel_init(&cs[i].reply);
    pthread_create(&th[i], NULL, live, &cs[i]);
  }
  while (done < count) {
    message req = receive(&meet);
    if (left == 0) {
      send(req.reply, (message){.done = 1});
      ++done;
    } else if (!have_first) {
      first = req;
      have_first = 1;
    } else {
      send(first.reply, (message){.id = req.id, .color = req.color});
      send(req.reply, (message){.id = first.id, .color = first.color});
      have_first = 0;
      --left;
    }
  }
  for (int i = 0; i < count; ++i) {
    pthread_join(th[i], NULL);
    channel_free(&cs[i].reply);
    met += cs[i].met;
    same += cs[i].same;
  }
  channel_free(&meet);
  return met == 2 * n && same == 0;
}

int main(void) {
  long long t = bench_ms();
  static const int three[] = {BLUE, RED, YELLOW};
  static const int ten[] = {BLUE, RED, YELLOW, RED, YELLOW, BLUE, RED, YELLOW, RED, BLUE};
  char table[10];
  for (int a = 0; a < 3; ++a)
    for (int b = 0; b < 3; ++b) table[a * 3 + b] = (char)('0' + complement(a, b));
  table[9] = 0;
  int ok = strcmp(table, "021210102") == 0;
  ok = run(30000, three, 3) && ok;
  ok = run(30000, ten, 10) && ok;
  bench_report("chameneos_redux", t, ok);
  return 0;
}
