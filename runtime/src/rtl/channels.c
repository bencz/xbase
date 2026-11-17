/* Channels (docs/concurrency.md#channels): a queue that tasks and threads hand values through, unbounded or of a
 * capacity. Its state is the channel object's own, changed under its lock (a word, xb_object_lock_short: a waiter
 * sleeps as a short wait, so collections go on): three rings (xb_ring, rtl/flows.h: the values; the tasks of receivers
 * waiting; senders waiting, each {task, value}), the values shared as they come in. A receiver or a sender that must
 * wait gets a task its token may cancel (xb_task_promise), made before the lock is taken; the other side claims it
 * under the lock (xb_claims: a token cancels it no more; one canceled meanwhile is passed over) and completes it once
 * the lock is released, so that the lock is held for the rings alone: no wake, no continuation and no allocation of a
 * waiter under it. A send that needs no wait returns the one task completed with .T. that all share. FOR EACH goes
 * through a channel with an enumerator of its own (each FOR EACH one), whose MoveNext() takes the next value, waiting
 * for it. */
#include "rtl/flows.h"

#include "core/shared.h"

enum {
  H_STATE, /* the lock's word (xb_object_lock_short) */
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
  {"(STATE)", XB_VAR_ANY},
  {"(VALUES)", XB_VAR_ANY},        {"(VALUESHEAD)", XB_VAR_ANY},      {"(VALUESCOUNT)", XB_VAR_ANY},
  {"(RECEIVERS)", XB_VAR_ANY},     {"(RECEIVERSHEAD)", XB_VAR_ANY},   {"(RECEIVERSCOUNT)", XB_VAR_ANY},
  {"(SENDERS)", XB_VAR_ANY},       {"(SENDERSHEAD)", XB_VAR_ANY},     {"(SENDERSCOUNT)", XB_VAR_ANY},
  {"(CAPACITY)", XB_VAR_ANY},      {"(CLOSED)", XB_VAR_ANY}};
_Static_assert(XB_TABLE_SIZE(channel_vars) == H_COUNT_, "one H_ index per Channel variable");

static const xb_ring values = {H_VALUES, H_VALUES_HEAD, H_VALUES_COUNT};
static const xb_ring receivers = {H_RECEIVERS, H_RECEIVERS_HEAD, H_RECEIVERS_COUNT};
static const xb_ring senders = {H_SENDERS, H_SENDERS_HEAD, H_SENDERS_COUNT};

/* The channel's lock: held a moment (no wait under it), its waiters keep their workers' places. */
static void lock(const xb_item *ch) { xb_object_lock_short(ch, H_STATE); }
static void unlock(const xb_item *ch) { xb_object_unlock_short(ch, H_STATE); }

static int is_closed(const xb_item *ch) {
  return xb_atomic_load32(xb_object_word(ch, H_CLOSED), XB_ACQUIRE) != 0;
}

/* `v` claimed for the first receiver waiting that was not canceled (completed by xb_claims_settle): whether one took
 * it. */
static int deliver(const xb_item *ch, const xb_item *v, xb_claims *c) {
  while (xb_ring_count(ch, receivers)) {
    xb_ring_pop(ch, receivers, &c->held);
    if (xb_task_claim(&c->held)) {
      xb_claims_add(c, &c->held, v);
      return 1;
    }
  }
  return 0;
}

static int has_room(const xb_item *ch) {
  uint32_t capacity = xb_object_u32(ch, H_CAPACITY), n = xb_ring_count(ch, values);
  return n < UINT32_MAX && (!capacity || n < capacity); /* the counts are 32-bit */
}

/* The values of senders waiting moved into the values while there is room, their tasks claimed (completed with .T.
 * by xb_claims_settle). */
static void refill(const xb_item *ch, xb_claims *c) {
  xb_item yes;
  xb_set_logical(&yes, 1);
  while (xb_ring_count(ch, senders) && has_room(ch)) {
    xb_ring_pop(ch, senders, &c->held); /* the pair, which keeps its task and value */
    const xb_array *a = c->held.v.p;
    xb_item task = a->items[0], value = a->items[1];
    if (xb_task_claim(&task)) {
      xb_ring_push(ch, values, &value, NULL, NULL);
      xb_claims_add(c, &task, &yes);
    }
  }
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

/* Under the lock: `value` (rooted, shared) taken by a receiver waiting or kept when there is room. */
static int take_value(const xb_item *ch, const xb_item *value, xb_claims *c) {
  if (deliver(ch, value, c)) return 1;
  if (!has_room(ch)) return 0;
  xb_ring_push(ch, values, value, NULL, NULL);
  return 1;
}

/* oChannel:SendAsync( xValue, [oToken] ): a task that completes (.T.) once the channel took the value: at once when a
 * receiver waits or there is room; else when a receiver makes room (the token cancels the wait). On a closed channel,
 * failed with the error "Channel closed". The waiting task and its pair are made before the lock is taken again. */
static void channel_send_async(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  xb_item value, token, pair;
  xb_set_nil(r);
  xb_set_nil(&pair);
  if (argc > 1) value = argv[1];
  else xb_set_nil(&value);
  if (argc > 2) token = argv[2];
  else xb_set_nil(&token);
  if (!xb_task_token_ok(&token)) {
    xb_flow_arg_error(ctx, XB_CODE_TASK, "CHANNEL:SENDASYNC", argc, argv);
    return;
  }
  xb_share(&value);
  xb_claims c;
  xb_claims_root(ctx, &c);
  xb_gc_root_push(ctx, &pair);
  for (int waiting = 0;; waiting = 1) {
    if (is_closed(&argv[0])) break;
    lock(&argv[0]);
    if (is_closed(&argv[0])) {
      unlock(&argv[0]);
      break;
    }
    /* A task made and not needed after all is claimed first: a token that canceled it meanwhile keeps the value out. */
    int mine = !waiting || xb_task_claim(r);
    if (mine && take_value(&argv[0], &value, &c)) {
      unlock(&argv[0]);
      xb_claims_settle(ctx, &c);
      xb_gc_root_pop(ctx, 3);
      if (waiting) {
        xb_item yes;
        xb_set_logical(&yes, 1);
        xb_task_settle_claimed(ctx, r, 1, &yes);
      } else {
        completed_true(ctx, r);
      }
      return;
    }
    if (waiting && !mine) { /* canceled meanwhile */
      unlock(&argv[0]);
      xb_claims_settle(ctx, &c);
      xb_gc_root_pop(ctx, 3);
      return;
    }
    if (waiting) { /* the task made: in line, unless its token asked to cancel it while it was claimed */
      int queued = xb_task_unclaim(r);
      if (queued) xb_ring_push(&argv[0], senders, &pair, xb_ring_pair_gone, NULL);
      unlock(&argv[0]);
      xb_claims_settle(ctx, &c);
      xb_gc_root_pop(ctx, 3);
      if (!queued) xb_task_cancel_claimed(ctx, r);
      return;
    }
    unlock(&argv[0]);
    xb_claims_settle(ctx, &c);
    if (!xb_task_promise(ctx, r, &token, "CHANNEL:SENDASYNC", argc, argv)) {
      xb_gc_root_pop(ctx, 3);
      return;
    }
    xb_array *a = xb_array_alloc(2); /* `a`, on the stack, stays where it is */
    xb_set_array(&pair, a);
    a->items[0] = *r;
    a->items[1] = value;
    xb_gc_write(a);
    xb_share(&pair);
  }
  xb_claims_settle(ctx, &c);
  xb_gc_root_pop(ctx, 3);
  if (r->type == XB_T_OBJECT) { /* a task made, then the channel closed */
    xb_item error;
    xb_set_nil(&error);
    xb_gc_root_push(ctx, &error);
    xb_error_object(ctx, &error, 0, XB_CODE_CHANNEL_CLOSED, "Channel closed", "CHANNEL:SENDASYNC");
    xb_task_settle(ctx, r, 0, &error);
    xb_gc_root_pop(ctx, 1);
  } else {
    closed_task(ctx, r, "CHANNEL:SENDASYNC");
  }
}

/* Under the lock: the next value into `*out` (rooted) when there is one, the senders waiting moved in after it. */
static int take_next(const xb_item *ch, xb_item *out, xb_claims *c) {
  if (!xb_ring_count(ch, values)) return 0;
  xb_ring_pop(ch, values, out);
  refill(ch, c);
  return 1;
}

/* oChannel:ReceiveAsync( [oToken] ): a task that completes with the next value: at once when there is one; else when a
 * sender gives one (the token cancels the wait). On a closed channel with no value left, failed with the error
 * "Channel closed". The waiting task is made before the lock is taken again. */
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
  xb_claims c;
  xb_claims_root(ctx, &c);
  xb_gc_root_push(ctx, &value);
  for (int waiting = 0;; waiting = 1) {
    lock(&argv[0]);
    int mine = !waiting || !xb_ring_count(&argv[0], values) || xb_task_claim(r); /* as in channel_send_async */
    if (!mine) {
      unlock(&argv[0]);
      xb_claims_settle(ctx, &c);
      xb_gc_root_pop(ctx, 3);
      return;
    }
    if (take_next(&argv[0], &value, &c)) {
      unlock(&argv[0]);
      xb_claims_settle(ctx, &c);
      if (waiting) xb_task_settle_claimed(ctx, r, 1, &value); /* the task made needs no wait now */
      else xb_task_completed(ctx, r, &value);
      xb_gc_root_pop(ctx, 3);
      return;
    }
    if (is_closed(&argv[0])) {
      unlock(&argv[0]);
      xb_claims_settle(ctx, &c);
      if (waiting) {
        xb_item error;
        xb_set_nil(&error);
        xb_gc_root_push(ctx, &error);
        xb_error_object(ctx, &error, 0, XB_CODE_CHANNEL_CLOSED, "Channel closed", "CHANNEL:RECEIVEASYNC");
        xb_task_settle(ctx, r, 0, &error);
        xb_gc_root_pop(ctx, 1);
      } else {
        closed_task(ctx, r, "CHANNEL:RECEIVEASYNC");
      }
      xb_gc_root_pop(ctx, 3);
      return;
    }
    if (waiting) {
      xb_ring_push(&argv[0], receivers, r, xb_ring_task_gone, NULL);
      unlock(&argv[0]);
      xb_claims_settle(ctx, &c);
      xb_gc_root_pop(ctx, 3);
      return;
    }
    unlock(&argv[0]);
    xb_claims_settle(ctx, &c);
    if (!xb_task_promise(ctx, r, &token, "CHANNEL:RECEIVEASYNC", argc, argv)) {
      xb_gc_root_pop(ctx, 3);
      return;
    }
  }
}

/* oChannel:TrySend( xValue ): the value given to a receiver waiting, or kept when there is room: whether it was. */
static void channel_try_send(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  xb_item value;
  if (argc > 1) value = argv[1];
  else xb_set_nil(&value);
  xb_set_logical(r, 0);
  if (is_closed(&argv[0])) return;
  xb_share(&value);
  xb_claims c;
  xb_claims_root(ctx, &c);
  lock(&argv[0]);
  int taken = !is_closed(&argv[0]) && take_value(&argv[0], &value, &c);
  unlock(&argv[0]);
  xb_claims_settle(ctx, &c);
  xb_gc_root_pop(ctx, 2);
  xb_set_logical(r, taken);
}

/* oChannel:TryReceive( [aSlot] ): the next value taken when there is one, into aSlot[ 1 ] (an array that has one):
 * whether there was. */
static void channel_try_receive(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  xb_item value;
  xb_set_nil(&value);
  xb_set_logical(r, 0);
  xb_claims c;
  xb_claims_root(ctx, &c);
  xb_gc_root_push(ctx, &value);
  lock(&argv[0]);
  int got = take_next(&argv[0], &value, &c);
  unlock(&argv[0]);
  xb_claims_settle(ctx, &c);
  if (got && argc > 1 && argv[1].type == XB_T_ARRAY) xb_array_put(argv[1].v.p, 0, &value);
  xb_gc_root_pop(ctx, 3);
  xb_set_logical(r, got);
}

/* oChannel:Close(): no value is sent any more: the receivers and senders waiting fail with the error "Channel closed";
 * the values kept are still received. Closing again does nothing. */
static void channel_close(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  XB_UNUSED(argc);
  xb_set_nil(r);
  if (is_closed(&argv[0])) return;
  xb_item error;
  xb_set_nil(&error);
  xb_claims c;
  xb_claims_root(ctx, &c);
  xb_gc_root_push(ctx, &error);
  xb_error_object(ctx, &error, 0, XB_CODE_CHANNEL_CLOSED, "Channel closed", "CHANNEL:CLOSE");
  c.fail = 1;
  lock(&argv[0]);
  if (!is_closed(&argv[0])) {
    xb_atomic_store32(xb_object_word(&argv[0], H_CLOSED), 1, XB_RELEASE);
    while (xb_ring_count(&argv[0], receivers)) { /* those canceled meanwhile passed over */
      xb_ring_pop(&argv[0], receivers, &c.held);
      if (xb_task_claim(&c.held)) xb_claims_add(&c, &c.held, &error);
    }
    while (xb_ring_count(&argv[0], senders)) {
      xb_ring_pop(&argv[0], senders, &c.held);
      xb_item task = ((xb_array *)c.held.v.p)->items[0];
      if (xb_task_claim(&task)) xb_claims_add(&c, &task, &error);
    }
  }
  unlock(&argv[0]);
  xb_claims_settle(ctx, &c);
  xb_gc_root_pop(ctx, 3);
}

/* oChannel:Count(): the values kept, not received yet. */
static void channel_count(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  XB_UNUSED(argc);
  xb_set_nil(r);
  lock(&argv[0]);
  uint32_t n = xb_ring_count(&argv[0], values);
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
  xb_claims c;
  xb_claims_root(ctx, &c);
  xb_gc_root_push(ctx, out);
  xb_gc_root_push(ctx, &task);
  int got = -1;
  for (int waiting = 0; got < 0; waiting = 1) {
    lock(ch);
    if (take_next(ch, out, &c)) got = 1;
    else if (is_closed(ch)) got = 0;
    else if (waiting) xb_ring_push(ch, receivers, &task, xb_ring_task_gone, NULL);
    unlock(ch);
    xb_claims_settle(ctx, &c);
    if (got >= 0 || waiting) break;
    xb_task_promise(ctx, &task, &token, "CHANNEL:RECEIVEASYNC", 0, NULL); /* made outside the lock */
  }
  if (got < 0) {
    got = xb_task_wait_result(ctx, &task, out);
    if (!got) { /* a QUIT, or the program's end: its place given up, so that no value goes to it */
      xb_item nil;
      xb_set_nil(&nil);
      xb_task_settle(ctx, &task, 0, &nil);
    }
  }
  xb_gc_root_pop(ctx, 4);
  return got;
}

/* oEnumerator:MoveNext(): the next value taken into CURRENT, waiting for it: .F. once the channel is closed and
 * empty. */
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
  xb_set_logical(&o->vars[H_STATE], 0);
  xb_set_logical(&o->vars[H_CLOSED], 0);
  xb_ring_init(o, values);
  xb_ring_init(o, receivers);
  xb_ring_init(o, senders);
  xb_set_int(&o->vars[H_CAPACITY], capacity);
  xb_set_object(ret, o);
}
