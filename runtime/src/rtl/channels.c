/* Channels (docs/concurrency.md#channels): a queue that tasks and threads hand values through, unbounded or of a
 * capacity. Its state is the channel object's own, changed under its lock (a recursive lock of three words, the one a
 * Mutex has: a waiter blocks, so collections go on): three rings over arrays of the collector (the values; the tasks of
 * receivers waiting; senders waiting, each {task, value}), each an array with its head and count. Elements are read and
 * written through the array functions (the arrays are shared, as the channel is). A receiver or a sender that must
 * wait gets a task its token may cancel (xb_task_promise), which the other side completes; one canceled meanwhile is
 * passed over. A send that needs no wait returns the one task completed with .T. that all share. FOR EACH goes through
 * a channel with an enumerator of its own (each FOR EACH one), whose MoveNext() takes the next value, waiting for it. */
#include "rtl/flows.h"

#include "core/shared.h"

enum {
  H_STATE, /* the lock: H_STATE, H_OWNER, H_DEPTH */
  H_OWNER,
  H_DEPTH,
  H_VALUES,
  H_VALUES_HEAD,
  H_VALUES_COUNT,
  H_RECEIVERS,
  H_RECEIVERS_HEAD,
  H_RECEIVERS_COUNT,
  H_SENDERS,
  H_SENDERS_HEAD,
  H_SENDERS_COUNT,
  H_CAPACITY, /* 0: unbounded */
  H_CLOSED,   /* a word: read without the lock */
  H_COUNT_
};
static const xb_class_var channel_vars[] = {
  {"(STATE)", XB_VAR_ANY},         {"(OWNER)", XB_VAR_ANY},           {"(DEPTH)", XB_VAR_ANY},
  {"(VALUES)", XB_VAR_ANY},        {"(VALUESHEAD)", XB_VAR_ANY},      {"(VALUESCOUNT)", XB_VAR_ANY},
  {"(RECEIVERS)", XB_VAR_ANY},     {"(RECEIVERSHEAD)", XB_VAR_ANY},   {"(RECEIVERSCOUNT)", XB_VAR_ANY},
  {"(SENDERS)", XB_VAR_ANY},       {"(SENDERSHEAD)", XB_VAR_ANY},     {"(SENDERSCOUNT)", XB_VAR_ANY},
  {"(CAPACITY)", XB_VAR_ANY},      {"(CLOSED)", XB_VAR_ANY}};
_Static_assert(XB_TABLE_SIZE(channel_vars) == H_COUNT_, "one H_ index per Channel variable");

/* A ring: the variables of its array, its head and its count. */
typedef struct ring {
  uint32_t items, head, count;
} ring;
static const ring values = {H_VALUES, H_VALUES_HEAD, H_VALUES_COUNT};
static const ring receivers = {H_RECEIVERS, H_RECEIVERS_HEAD, H_RECEIVERS_COUNT};
static const ring senders = {H_SENDERS, H_SENDERS_HEAD, H_SENDERS_COUNT};

#define RING_FIRST 8u

/* A channel's integer variable `k` (under its lock: no other thread reads or writes it meanwhile). */
static uint32_t var(const xb_item *ch, uint32_t k) { return (uint32_t)((xb_object *)ch->v.p)->vars[k].v.i; }
static void set_var(const xb_item *ch, uint32_t k, uint32_t n) { xb_set_int(&((xb_object *)ch->v.p)->vars[k], n); }

static uint32_t ring_count(const xb_item *ch, ring g) { return var(ch, g.count); }

/* Whether a waiter of the ring needs its place no more: its task ended (canceled by its token). A ring of values
 * has none. */
static int waiter_gone(ring g, const xb_item *it) {
  xb_item task;
  if (g.items == H_VALUES) return 0;
  if (g.items == H_SENDERS) xb_array_get(it->v.p, 0, &task); /* {task, value} */
  else task = *it;
  return xb_task_is_final(&task);
}

/* The waiters that need their place no more dropped from a full ring, the others kept in order from its head (the
 * array as it is): a token that cancels waits again and again on a quiet channel keeps nothing. */
static void ring_compact(const xb_item *ch, ring g) {
  xb_object *o = ch->v.p;
  uint32_t head = var(ch, g.head), n = var(ch, g.count), kept = 0;
  size_t cap = xb_array_length(o->vars[g.items].v.p);
  xb_item it, nil;
  xb_set_nil(&nil);
  for (uint32_t k = 0; k < n; ++k) {
    xb_array_get(o->vars[g.items].v.p, (head + k) % cap, &it);
    if (waiter_gone(g, &it)) continue;
    xb_array_put(o->vars[g.items].v.p, (head + kept) % cap, &it);
    ++kept;
  }
  for (uint32_t k = kept; k < n; ++k) xb_array_put(o->vars[g.items].v.p, (head + k) % cap, &nil);
  set_var(ch, g.count, kept);
}

/* `v` (rooted) at the ring's end; when it is full, the waiters gone are dropped first, and the array grown (twice as
 * large, in order from 0) when it is still full. */
static void ring_push(const xb_item *ch, ring g, const xb_item *v) {
  xb_object *o = ch->v.p;
  size_t cap = o->vars[g.items].type == XB_T_ARRAY ? xb_array_length(o->vars[g.items].v.p) : 0;
  if (cap && var(ch, g.count) == cap) ring_compact(ch, g);
  uint32_t head = var(ch, g.head), n = var(ch, g.count);
  if (n == cap) {
    xb_item grown, it;
    xb_array *a = xb_array_alloc(cap ? 2 * cap : RING_FIRST); /* `a`, on the stack, stays where it is */
    xb_set_array(&grown, a);
    for (uint32_t k = 0; k < n; ++k) {
      xb_array_get(o->vars[g.items].v.p, (head + k) % cap, &it);
      a->items[k] = it;
    }
    xb_gc_write_items(a, 0, n);
    xb_share(&grown);
    o = ch->v.p;
    o->vars[g.items] = grown;
    xb_gc_write(o);
    head = 0;
    cap = xb_array_length(a);
    set_var(ch, g.head, 0);
  }
  xb_array_put(o->vars[g.items].v.p, (head + n) % cap, v);
  set_var(ch, g.count, n + 1);
}

/* The ring's first element, taken (its slot emptied for the collector). */
static void ring_pop(const xb_item *ch, ring g, xb_item *out) {
  xb_object *o = ch->v.p;
  uint32_t head = var(ch, g.head), n = var(ch, g.count);
  size_t cap = xb_array_length(o->vars[g.items].v.p);
  xb_item nil;
  xb_set_nil(&nil);
  xb_array_get(o->vars[g.items].v.p, head, out);
  xb_array_put(o->vars[g.items].v.p, head, &nil);
  set_var(ch, g.head, (uint32_t)((head + 1) % cap));
  set_var(ch, g.count, n - 1);
}

static int lock(xb_ctx *ctx, const xb_item *ch) { return xb_object_lock(ctx, ch, H_STATE); }
static void unlock(const xb_item *ch) { xb_rwords_release(xb_object_rwords(ch, H_STATE)); }

static int is_closed(const xb_item *ch) {
  return xb_atomic_load32(xb_object_word(ch, H_CLOSED), XB_ACQUIRE) != 0;
}

/* `v` handed to the first receiver waiting that was not canceled: whether one took it. */
static int deliver(xb_ctx *ctx, const xb_item *ch, const xb_item *v) {
  xb_item receiver;
  xb_set_nil(&receiver);
  xb_gc_root_push(ctx, &receiver);
  int taken = 0;
  while (!taken && ring_count(ch, receivers)) {
    ring_pop(ch, receivers, &receiver);
    taken = xb_task_settle(ctx, &receiver, 1, v);
  }
  xb_gc_root_pop(ctx, 1);
  return taken;
}

static int has_room(const xb_item *ch) {
  uint32_t capacity = var(ch, H_CAPACITY), n = ring_count(ch, values);
  return n < UINT32_MAX && (!capacity || n < capacity); /* the counts are 32-bit */
}

/* The values of senders waiting moved into the values while there is room, their tasks completed (.T.). */
static void refill(xb_ctx *ctx, const xb_item *ch) {
  xb_item pair, task, value, yes;
  xb_set_nil(&pair);
  xb_set_nil(&task);
  xb_set_nil(&value);
  xb_set_logical(&yes, 1);
  xb_gc_root_push(ctx, &pair);
  xb_gc_root_push(ctx, &task);
  xb_gc_root_push(ctx, &value);
  while (ring_count(ch, senders) && has_room(ch)) {
    ring_pop(ch, senders, &pair);
    xb_array_get(pair.v.p, 0, &task);
    xb_array_get(pair.v.p, 1, &value);
    if (xb_task_settle(ctx, &task, 1, &yes)) ring_push(ch, values, &value);
  }
  xb_gc_root_pop(ctx, 3);
}

/* A task failed with the error "Channel closed" of `operation`. */
static void closed_task(xb_ctx *ctx, xb_item *r, const char *operation) {
  xb_item error, nil;
  xb_set_nil(&error);
  xb_set_nil(&nil);
  xb_gc_root_push(ctx, &error);
  xb_error_object(ctx, &error, 0, XB_CODE_CHANNEL_CLOSED, "Channel closed", operation);
  xb_task_promise(ctx, r, &nil, operation, 0, NULL);
  xb_task_settle(ctx, r, 0, &error);
  xb_gc_root_pop(ctx, 1);
}

static void completed_true(xb_ctx *ctx, xb_item *r) {
  xb_item yes;
  xb_set_logical(&yes, 1);
  xb_task_completed(ctx, r, &yes);
}

/* oChannel:SendAsync( xValue, [oToken] ): a task that completes (.T.) once the channel took the value: at once when a
 * receiver waits or there is room; else when a receiver makes room (the token cancels the wait). On a closed channel,
 * failed with the error "Channel closed". */
static void channel_send_async(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  xb_item value, token;
  xb_set_nil(r);
  if (argc > 1) value = argv[1];
  else xb_set_nil(&value);
  if (argc > 2) token = argv[2];
  else xb_set_nil(&token);
  if (!xb_task_token_ok(&token)) {
    xb_flow_arg_error(ctx, XB_CODE_TASK, "CHANNEL:SENDASYNC", argc, argv);
    return;
  }
  if (is_closed(&argv[0])) {
    closed_task(ctx, r, "CHANNEL:SENDASYNC");
    return;
  }
  if (!lock(ctx, &argv[0])) return;
  if (is_closed(&argv[0])) {
    unlock(&argv[0]);
    closed_task(ctx, r, "CHANNEL:SENDASYNC");
    return;
  }
  if (deliver(ctx, &argv[0], &value) || (has_room(&argv[0]) && (ring_push(&argv[0], values, &value), 1))) {
    unlock(&argv[0]);
    completed_true(ctx, r);
    return;
  }
  if (xb_task_promise(ctx, r, &token, "CHANNEL:SENDASYNC", argc, argv)) {
    xb_item pair;
    xb_array *a = xb_array_alloc(2); /* `a`, on the stack, stays where it is */
    xb_set_array(&pair, a);
    a->items[0] = *r;
    a->items[1] = value;
    xb_gc_write(a);
    xb_gc_root_push(ctx, &pair);
    ring_push(&argv[0], senders, &pair);
    xb_gc_root_pop(ctx, 1);
  }
  unlock(&argv[0]);
}

/* oChannel:ReceiveAsync( [oToken] ): a task that completes with the next value: at once when there is one; else when a
 * sender gives one (the token cancels the wait). On a closed channel with no value left, failed with the error
 * "Channel closed". */
static void channel_receive_async(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  xb_item token, value;
  xb_set_nil(r);
  xb_set_nil(&value);
  if (argc > 1) token = argv[1];
  else xb_set_nil(&token);
  if (!xb_task_token_ok(&token)) {
    xb_flow_arg_error(ctx, XB_CODE_TASK, "CHANNEL:RECEIVEASYNC", argc, argv);
    return;
  }
  if (!lock(ctx, &argv[0])) return;
  if (ring_count(&argv[0], values)) {
    xb_gc_root_push(ctx, &value);
    ring_pop(&argv[0], values, &value);
    refill(ctx, &argv[0]);
    unlock(&argv[0]);
    xb_task_completed(ctx, r, &value);
    xb_gc_root_pop(ctx, 1);
    return;
  }
  if (is_closed(&argv[0])) {
    unlock(&argv[0]);
    closed_task(ctx, r, "CHANNEL:RECEIVEASYNC");
    return;
  }
  if (xb_task_promise(ctx, r, &token, "CHANNEL:RECEIVEASYNC", argc, argv)) ring_push(&argv[0], receivers, r);
  unlock(&argv[0]);
}

/* oChannel:TrySend( xValue ): the value given to a receiver waiting, or kept when there is room: whether it was. */
static void channel_try_send(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  xb_item value;
  if (argc > 1) value = argv[1];
  else xb_set_nil(&value);
  xb_set_logical(r, 0);
  if (is_closed(&argv[0]) || !lock(ctx, &argv[0])) return;
  int taken = !is_closed(&argv[0]) &&
              (deliver(ctx, &argv[0], &value) || (has_room(&argv[0]) && (ring_push(&argv[0], values, &value), 1)));
  unlock(&argv[0]);
  xb_set_logical(r, taken);
}

/* oChannel:TryReceive( [aSlot] ): the next value taken when there is one, into aSlot[ 1 ] (an array that has one):
 * whether there was. */
static void channel_try_receive(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  xb_item value;
  xb_set_nil(&value);
  xb_set_logical(r, 0);
  if (!lock(ctx, &argv[0])) return;
  int got = ring_count(&argv[0], values) != 0;
  xb_gc_root_push(ctx, &value);
  if (got) {
    ring_pop(&argv[0], values, &value);
    refill(ctx, &argv[0]);
  }
  unlock(&argv[0]);
  if (got && argc > 1 && argv[1].type == XB_T_ARRAY) xb_array_put(argv[1].v.p, 0, &value);
  xb_gc_root_pop(ctx, 1);
  xb_set_logical(r, got);
}

/* oChannel:Close(): no value is sent any more: the receivers and senders waiting fail with the error "Channel closed";
 * the values kept are still received. Closing again does nothing. */
static void channel_close(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  XB_UNUSED(argc);
  xb_set_nil(r);
  if (!lock(ctx, &argv[0])) return;
  if (is_closed(&argv[0])) {
    unlock(&argv[0]);
    return;
  }
  xb_atomic_store32(xb_object_word(&argv[0], H_CLOSED), 1, XB_RELEASE);
  xb_item error, task;
  xb_set_nil(&error);
  xb_set_nil(&task);
  xb_gc_root_push(ctx, &error);
  xb_gc_root_push(ctx, &task);
  xb_error_object(ctx, &error, 0, XB_CODE_CHANNEL_CLOSED, "Channel closed", "CHANNEL:CLOSE");
  while (ring_count(&argv[0], receivers)) {
    ring_pop(&argv[0], receivers, &task);
    xb_task_settle(ctx, &task, 0, &error);
  }
  while (ring_count(&argv[0], senders)) {
    ring_pop(&argv[0], senders, &task);
    xb_array_get(task.v.p, 0, &task);
    xb_task_settle(ctx, &task, 0, &error);
  }
  unlock(&argv[0]);
  xb_gc_root_pop(ctx, 2);
}

/* oChannel:Count(): the values kept, not received yet. */
static void channel_count(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  XB_UNUSED(argc);
  xb_set_nil(r);
  if (!lock(ctx, &argv[0])) return;
  uint32_t n = ring_count(&argv[0], values);
  unlock(&argv[0]);
  xb_set_int(r, n);
}

static void channel_is_closed(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  XB_UNUSED(argc);
  xb_set_logical(r, is_closed(&argv[0]));
}

/* Enumerators: the channel, and the value MoveNext() took last (CURRENT, read only). */

enum { E_CURRENT, E_CHANNEL, E_COUNT_ };
static const xb_class_var enumerator_vars[] = {{"CURRENT", XB_VAR_READ}, {"(CHANNEL)", XB_VAR_ANY}};
_Static_assert(XB_TABLE_SIZE(enumerator_vars) == E_COUNT_, "one E_ index per channel enumerator variable");

/* The next value into `*out`, waiting for one: 0 once the channel is closed and empty (or a QUIT ended the wait). */
static int receive(xb_ctx *ctx, const xb_item *ch, xb_item *out) {
  xb_item token, task;
  xb_set_nil(&token);
  xb_set_nil(&task);
  if (!lock(ctx, ch)) return 0;
  if (ring_count(ch, values)) {
    ring_pop(ch, values, out);
    xb_gc_root_push(ctx, out);
    refill(ctx, ch);
    unlock(ch);
    xb_gc_root_pop(ctx, 1);
    return 1;
  }
  if (is_closed(ch)) {
    unlock(ch);
    return 0;
  }
  xb_gc_root_push(ctx, &task);
  xb_task_promise(ctx, &task, &token, "CHANNEL:RECEIVEASYNC", 0, NULL);
  ring_push(ch, receivers, &task);
  unlock(ch);
  int got = xb_task_wait_result(ctx, &task, out);
  if (!got) { /* a QUIT, or the program's end: its place given up, so that no value goes to it */
    xb_item nil;
    xb_set_nil(&nil);
    xb_task_settle(ctx, &task, 0, &nil);
  }
  xb_gc_root_pop(ctx, 1);
  return got;
}

/* oEnumerator:MoveNext(): the next value taken into CURRENT, waiting for it: .F. once the channel is closed and empty. */
static void enumerator_move_next(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  XB_UNUSED(argc);
  xb_item ch, value;
  xb_set_nil(&value);
  xb_object_get(argv[0].v.p, E_CHANNEL, &ch);
  xb_gc_root_push(ctx, &ch);
  xb_gc_root_push(ctx, &value);
  int got = receive(ctx, &ch, &value);
  if (got) xb_object_put(argv[0].v.p, E_CURRENT, &value);
  xb_gc_root_pop(ctx, 2);
  xb_set_logical(r, got);
}

static const xb_class_method enumerator_methods[] = {{"MOVENEXT", enumerator_move_next}};
static const xb_class enumerator_class = {"CHANNELENUMERATOR", E_COUNT_, enumerator_vars, NULL,
                                          XB_TABLE_SIZE(enumerator_methods), enumerator_methods};

/* oChannel:GetEnumerator(): an enumerator of the values as they come, until the channel is closed and empty (FOR EACH
 * waits for each). */
static void channel_get_enumerator(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  XB_UNUSED(argc);
  xb_object *e = xb_object_new(&enumerator_class);
  e->vars[E_CHANNEL] = argv[0];
  xb_gc_write(e);
  xb_set_object(r, e);
}

static const xb_class_method channel_methods[] = {
  {"SENDASYNC", channel_send_async},       {"RECEIVEASYNC", channel_receive_async},
  {"TRYSEND", channel_try_send},           {"TRYRECEIVE", channel_try_receive},
  {"CLOSE", channel_close},                {"COUNT", channel_count},
  {"ISCLOSED", channel_is_closed},         {"GETENUMERATOR", channel_get_enumerator}};
static const xb_class channel_class = {
  "CHANNEL", H_COUNT_, channel_vars, NULL, XB_TABLE_SIZE(channel_methods), channel_methods};

/* Channel( [nCapacity] ): a channel that keeps up to nCapacity values not received yet (without it, or 0: any
 * number); a capacity that is no number, below 0 or past 2^31 - 1, is the argument error. */
void xbf_CHANNEL(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  int64_t capacity = 0;
  if (argc && argv[0].type != XB_T_NIL &&
      (!xb_integer_arg(ctx, "CHANNEL", XB_CODE_CHANNEL, 0, argc, argv, &capacity) || capacity < 0 ||
       capacity > INT32_MAX)) {
    if (!(xb_pending_get(ctx) & XB_PENDING_UNWIND)) xb_flow_arg_error(ctx, XB_CODE_CHANNEL, "CHANNEL", argc, argv);
    return;
  }
  xb_object *o = xb_object_new(&channel_class);
  for (uint32_t k = 0; k < H_COUNT_; ++k) xb_set_int(&o->vars[k], 0);
  xb_set_logical(&o->vars[H_STATE], 0);
  xb_set_logical(&o->vars[H_OWNER], 0);
  xb_set_logical(&o->vars[H_DEPTH], 0);
  xb_set_logical(&o->vars[H_CLOSED], 0);
  xb_set_nil(&o->vars[H_VALUES]);
  xb_set_nil(&o->vars[H_RECEIVERS]);
  xb_set_nil(&o->vars[H_SENDERS]);
  xb_set_int(&o->vars[H_CAPACITY], capacity);
  xb_set_object(ret, o);
}
