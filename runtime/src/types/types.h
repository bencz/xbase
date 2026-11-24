/* Representation and base operations of the value types. */
#ifndef XB_TYPES_H
#define XB_TYPES_H

#include "core/context.h"
#include "core/shared.h"

/* Uninitialized string of `len` bytes (plus the terminating NUL). */
xb_str *xb_str_alloc(uint64_t len);
/* xb_hash_bytes of the string's bytes, computed once: kept in `hash` (literals carry theirs; a string an append grows
 * keeps its room there, and is hashed each time). */
uint32_t xb_str_hash(xb_str *s); /* 64-bit: checked before it is narrowed to size_t (32-bit hosts) */
/* The same for a length the program chose: NULL when there is no memory for it (xb_gc_try_alloc). */
xb_str *xb_str_try_alloc(uint64_t len);
/* An appending `+` (xb_op_add_grow) whose result is shorter than this gives a plain string, copied whole: up to this
 * length a copy costs no more time than a view (a second object, a buffer with its header) and takes less memory than
 * a view and its buffer's room, measured on strings built by appends of a few bytes. From this length on the result
 * is a view of a buffer with as much room again, which the next append fills in place: appending n times to an
 * element, a member, a memory variable or a STATIC copies O(n) bytes in all, not O(n^2) (a LOCAL grows in place
 * without views, xb_op_append). */
#define XB_STR_VIEW_MIN 512u
/* `a + b` of strings for xb_op_add_grow (an append): an empty operand gives the other one; a view of the
 * buffer of `a` when `a` is a view that ends at its buffer's tip with room for `b` (the bytes of `b` written there);
 * else, from XB_STR_VIEW_MIN bytes, a view of a new buffer with room to grow; else a plain string, as
 * xb_str_concat makes. `a` and `b` are reachable (an operand's item is a root): the new view's buffer is kept alive
 * through `a` while the view is made. */
xb_str *xb_str_add(xb_ctx *ctx, const xb_str *a, const xb_str *b);
/* The bytes of the string item `it` followed by a NUL, for C code that reads up to one (a path, a name the system
 * reads, a C string function): a plain string's own bytes; a view's are not followed by one, so the item takes a
 * plain copy of the same text first. That allocates: `it` is a root (an argument), and what the caller held of its
 * string before (an xb_str pointer) is read from the item again. */
const char *xb_item_cstr(xb_item *it);
/* -1, 0 or 1 as the comparison operators order two values (strings follow SET
 * EXACT), XB_ORDER_UNORDERED for a NaN (xb_order_is in xbrt.h), or 2 when they
 * cannot be compared (core/ops.c). */
int xb_compare(const xb_ctx *ctx, const xb_item *a, const xb_item *b);

/* xb_array is in xbrt.h. Its items move to an XB_GC_ITEMS object when it
 * grows past its own allocation; no malloc is involved, so dead arrays cost
 * nothing to free. */

/* Card marking. Storage for XB_ARRAY_CARD_MIN items or more carries one card
 * byte per 2^XB_ARRAY_CARD_SHIFT items, right after the last item it has room
 * for. A store into an old array marks the element's card, and a young
 * collection rescans only the marked cards instead of the whole array. */
#define XB_ARRAY_CARD_SHIFT 6
#define XB_ARRAY_CARD_MIN   1024u
static inline size_t xb_array_card_count(size_t cap) {
  return cap >= XB_ARRAY_CARD_MIN ? (cap + ((size_t)1 << XB_ARRAY_CARD_SHIFT) - 1) >> XB_ARRAY_CARD_SHIFT : 0;
}
static inline uint8_t *xb_array_cards(const xb_array *a) {
  return a->cap >= XB_ARRAY_CARD_MIN ? (uint8_t *)((xb_item *)a->data + a->cap) : NULL;
}

/* The forms of an array's elements (xb_array in xbrt.h, where the form each new array starts in is told), bits of its
 * state byte; xb_array_items gives the elements of any array as items, turning one of a compact form into one of items first, in
 * place (it allocates nothing, so it may run anywhere). C code reads and writes elements in place through it, unless
 * it handles the forms itself (types/array.c, the collector). A shared array always holds items (xb_share turns it
 * before another thread can reach it), so only a local one ever changes form. */
static inline uint32_t xb_elems_of_state(uint8_t state) {
  return (state >> XB_GC_STATE_ELEMS_SHIFT) & XB_GC_STATE_ELEMS_MASK;
}
static inline uint32_t xb_array_elems(const xb_array *a) {
  return xb_elems_of_state(xb_atomic_load8(&xb_gc_header(a)->state, XB_RELAXED));
}
void xb_array_to_items(xb_array *a);
static inline xb_item *xb_array_items(const xb_array *a) {
  if (xb_array_elems(a) != XB_ELEMS_ITEMS) xb_array_to_items((xb_array *)a); /* the values stay: a read may turn it */
  return a->data;
}

/* Longest array: its items must fit in addressable memory with room to grow. */
#define XB_ARRAY_MAX_LEN (SIZE_MAX / sizeof(xb_item) / 4)
/* Longest string: its size with the header must not wrap. */
#define XB_STR_MAX_LEN (SIZE_MAX / 2)
/* A count the program gave, as a size_t: SIZE_MAX when it is past what size_t holds (32-bit hosts), never the count
 * narrowed (2^32 + 5 is no 5). For a bound (a width, a column) that is as good as no bound. */
static inline size_t xb_size_sat(uint64_t n) { return n > SIZE_MAX ? SIZE_MAX : (size_t)n; }
/* BASE/9032 Memory low (EG_MEM): a string or an array of a size the program chose finds no memory, even after a full
 * collection; the program may handle it (the reference ends). */
#define XB_CODE_MEMORY 9032
/* BASE/1234 String overflow (EG_STROVERFLOW): a string longer than the largest one (xb_error_overflow). */
#define XB_CODE_STROVERFLOW 1234
/* CHARACTER:Split, and CHARACTER:StartsWith and EndsWith, given an argument that is no string. */
#define XB_CODE_SPLIT 9033
#define XB_CODE_AFFIX 9034

/* The 0-based index of the 1-based position `n` in `a`, or -1 outside it. */
static inline int64_t xb_array_index(const xb_array *a, int64_t n) {
  return n >= 1 && (uint64_t)n <= a->len ? n - 1 : -1;
}

/* Lengths are 64-bit so that a length past XB_ARRAY_MAX_LEN is caught before
 * it is narrowed to size_t (on 32-bit hosts). */
xb_array *xb_array_alloc(uint64_t len); /* `len` NIL items */
/* An array of a length the program chose (Array()), of NILs in form `elems` (XB_ELEMS_ITEMS or XB_ELEMS_NIL): NULL when
 * it is past the largest array or no memory holds it. */
xb_array *xb_array_try_alloc(uint64_t len, uint32_t elems);
/* The visible length set to `len`, new items NIL; `a` must be reachable. xb_array_resize is for an array no other
 * thread reaches (one the runtime has just made); xb_array_resize_any for any, where a shared one (core/shared.h)
 * grows into storage allocated before its lock is taken (rooted in `ctx` meanwhile) and changes under the lock. */
void      xb_array_resize(xb_array *a, uint64_t len);
void      xb_array_resize_any(xb_ctx *ctx, xb_array *a, uint64_t len);
/* xb_array_resize_any for a length the program chose (ASize): 0, the array as it was, when no memory holds it. */
int       xb_array_resize_try(xb_ctx *ctx, xb_array *a, uint64_t len);
/* An array that other threads may reach: its length, the element at 0-based `i` into *dst (0 past its end) and `v`
 * into element `i` (0 past its end; a shared array shares the value first), each whole under the array's sequence
 * counter or lock when it is shared, in place when it is local. */
size_t    xb_array_length(const xb_array *a);
int       xb_array_get(const xb_array *a, size_t i, xb_item *dst);
int       xb_array_put(xb_array *a, size_t i, const xb_item *v);
/* `v` into elements `from` .. `to` - 1 of `a` (those past its length left out): AFill. */
void      xb_array_fill(xb_array *a, size_t from, size_t to, const xb_item *v);
/* The last element into *dst, 0 when there is none, in one read (ATail). */
int       xb_array_last(const xb_array *a, xb_item *dst);
/* A new local array of the elements, all of them as one read saw them (an array other threads change is copied again
 * until a read of all its items finds no writer between, under its lock after a few reads writers overtook); `a` must
 * be reachable. */
xb_array *xb_array_snapshot(xb_ctx *ctx, const xb_array *a);
/* `v` added after the last element, at once whatever other threads add (AAdd); `v` must be rooted. */
void      xb_array_append(xb_ctx *ctx, xb_array *a, const xb_item *v);
/* The elements from 0-based `at` move one place up, the last one lost, and `at` becomes NIL (AIns); the elements after
 * `at` move one place down and the last becomes NIL (ADel). Nothing when `at` is past the end. */
void      xb_array_insert(xb_array *a, size_t at);
void      xb_array_delete(xb_array *a, size_t at);
void      xb_set_array(xb_item *it, xb_array *a);
void      xb_array_bound_error(xb_ctx *ctx, const xb_item *base, const xb_item *idx); /* reading outside 1 .. Len() */

/* Stores in `r` a new reference to element `index` of `coll` (which must stay
 * reachable meanwhile: a frame slot). xb_ref_copy stores in `r` a new
 * reference to the element `ref` (a reachable XB_T_REF item) designates. */
void xb_ref_init(xb_item *r, const xb_item *coll, int64_t index, uint32_t loop);
void xb_ref_copy(xb_item *r, const xb_item *ref);

/* Whether `it` holds the reference of a FOR EACH loop. */
static inline int xb_is_loop_ref(const xb_item *it) {
  return it->type == XB_T_REF && ((const xb_ref *)it->v.p)->loop;
}

/* Moves the value of the variable whose own item is `var` into a new cell
 * (unless it holds one) and returns the cell; a STATIC's under its stripe once
 * the program's storage is shared. */
xb_cell *xb_cell_move(xb_ctx *ctx, xb_item *var, uint32_t is_static);
/* A cell's value that other threads may reach (a codeblock's capture, a memory variable), read whole, or written
 * (the value shared first) under the cell's lock when it is shared (core/shared.h); in place when it is local. */
void     xb_cell_get_shared(const xb_cell *c, xb_item *dst);
void     xb_cell_put_shared(xb_cell *c, const xb_item *v);
static inline void xb_cell_get(const xb_cell *c, xb_item *dst) {
  if (xb_is_shared(c)) xb_cell_get_shared(c, dst);
  else *dst = c->value;
}
static inline void xb_cell_put(xb_cell *c, const xb_item *v) {
  uint8_t state = xb_atomic_load8(&xb_gc_header(c)->state, XB_RELAXED); /* one load: sharing and the barrier's test */
  if (state & XB_GC_SHARED) {
    xb_cell_put_shared(c, v);
    return;
  }
  c->value = *v;
  if (xb_gc_needs_record(state)) xb_gc_remember(c);
}
/* What a reference (an XB_T_CELL or XB_T_REF item) designates, read into `dst` or written from `v`, whole, through
 * the shared paths of cells, arrays and hashes: an element gone is the bound error, and then the reference's own
 * item is read, or written when no request to unwind is pending (a substituted value lands there). */
void     xb_ref_get(xb_ctx *ctx, const xb_item *r, xb_item *dst);
/* The arguments of a message (argv[1] on) that are references (an argument written @x), each replaced by the value it
 * designates: for what does not take references, an instance variable assigned or a method of the runtime's own
 * (a method compiled from xBase takes them, as a function does). */
void     xb_args_values(xb_ctx *ctx, uint32_t argc, xb_item *argv);
/* A reference's index and own item (a string's character, an enumerator's Current) when another thread may reach the
 * reference: a loop's, in a cell shared with a codeblock. */
int64_t  xb_ref_index(const xb_ref *ref);
void     xb_ref_set_index(xb_ref *ref, int64_t index);
void     xb_ref_own_get(const xb_ref *ref, xb_item *dst);
void     xb_ref_own_put(xb_ref *ref, const xb_item *v);
void     xb_ref_set(xb_ctx *ctx, const xb_item *r, const xb_item *v);

/* What an instance variable of a class accepts. A value of another type is
 * ignored by XB_VAR_ARRAY, _LOGICAL and _STRING; the numeric rules raise an
 * argument error for it, and keep a number truncated toward zero, wrapped to
 * 32 bits signed or 16 bits unsigned (0 for a double beyond 64 bits).
 * XB_VAR_READ takes no assignment (the message to assign it is not found);
 * the runtime alone sets it. */
typedef enum xb_var_rule {
  XB_VAR_ANY,
  XB_VAR_READ,
  XB_VAR_ARRAY,
  XB_VAR_LOGICAL,
  XB_VAR_STRING,
  XB_VAR_INT32,
  XB_VAR_UINT16,
} xb_var_rule;

typedef struct xb_class_var {
  const char *name;             /* upper-cased; in parentheses: the runtime's own, which no message reaches */
  xb_var_rule rule;
} xb_class_var;

/* Whether a message may name the variable: not one of the runtime's own (a generator's state, frame and resume
 * function), whatever text the message comes from (ObjSend, a macro). */
static inline int xb_var_answers(const xb_class_var *v, const char *name) {
  return v->name[0] != '(' && strcmp(v->name, name) == 0;
}

/* The number of entries of a C table (a runtime class's variables or methods). */
#define XB_TABLE_SIZE(table) ((uint32_t)(sizeof(table) / sizeof((table)[0])))

/* A method of a runtime class: argv[0] is the object, the message's arguments follow; one named _NAME answers the
 * assignment `:NAME := value`. */
typedef struct xb_class_method {
  const char *name; /* upper-cased */
  void (*fn)(xb_ctx *ctx, xb_item *r, uint32_t argc, xb_item *argv);
} xb_class_method;

/* A class: one the runtime defines in C, or one compiled from xBase
 * (`compiled`, types/class.c), whose variables all take any value. */
typedef struct xb_class {
  const char *name;             /* upper-cased, as ClassName() returns it */
  uint32_t nvars;
  const xb_class_var *vars;     /* in the order of an object's items */
  struct cls *compiled;         /* NULL for the runtime's own classes */
  uint32_t nmethods;            /* a runtime class's methods (none: 0, NULL) */
  const xb_class_method *methods;
  /* A runtime class's destructor (argv[0] the object), or NULL: runs for an object registered with xb_gc_finalizable
   * once the collector finds it unreachable (xb_classes_finalize). */
  xb_func destructor;
} xb_class;

/* The method of runtime class `cls` named `name` (upper-cased), or NULL. */
const xb_class_method *xb_class_method_of(const xb_class *cls, const char *name);

/* What a message reaches in an object of runtime class `cls` (its variables first, as xb_op_send finds them): 1 and
 * `*member`, the member's index shifted by 2 and how it is reached (XB_RT_*), or 0 for none (a generator's own
 * messages, an error). xb_runtime_perform does it (the arguments' values taken first): what a send cache keeps for a
 * runtime class (types/class.c), so that a send repeated finds its member with no search. */
enum { XB_RT_GET, XB_RT_SET, XB_RT_METHOD };
int xb_runtime_member(const xb_class *cls, const char *message, size_t *member);
void xb_runtime_perform(xb_ctx *ctx, xb_item *r, size_t member, uint32_t argc, xb_item *argv);

/* An instance of a class: one item per variable, each starting as its rule's
 * empty value (NIL, .F., "" or 0). */
xb_object *xb_object_new(const xb_class *cls);
/* Variable `k` of an object that other threads may reach, read whole, or written (the value shared first) under the
 * object's lock when it is shared (core/shared.h); in place when it is local. */
void       xb_object_get_shared(const xb_object *o, uint32_t k, xb_item *dst);
void       xb_object_put_shared(xb_object *o, uint32_t k, const xb_item *v);
static inline void xb_object_get(const xb_object *o, uint32_t k, xb_item *dst) {
  if (xb_is_shared(o)) xb_object_get_shared(o, k, dst);
  else *dst = o->vars[k];
}
static inline void xb_object_put(xb_object *o, uint32_t k, const xb_item *v) {
  uint8_t state = xb_atomic_load8(&xb_gc_header(o)->state, XB_RELAXED); /* one load: sharing and the barrier's test */
  if (state & XB_GC_SHARED) {
    xb_object_put_shared(o, k, v);
    return;
  }
  o->vars[k] = *v;
  if (xb_gc_needs_record(state)) xb_gc_remember(o); /* xb_gc_write's */
}
/* Variable `k` takes `v` only while it still holds `expected` (the same item, both words): 1 when it did. Under the
 * object's lock when it is shared (`v` shared first). */
int        xb_object_replace(xb_object *o, uint32_t k, const xb_item *expected, const xb_item *v);
void       xb_set_object(xb_item *it, xb_object *o);
/* A message to an object of a compiled class (types/class.c); one it does not
 * answer is the class's error ("Message not found"). Returns 1. */
int        xb_class_send(xb_ctx *ctx, xb_item *r, const char *message, xb_class_def *from, uint32_t super,
                         uint32_t argc, xb_item *argv);
/* Runs the destructors of the objects the collector found unreachable
 * (xb_gc_next_finalized), each class of the object's line that has one, the
 * object's class first (a runtime class's own destructor alone); they may collect. The requests pending before (and
 * the BREAK value) wait meanwhile; an error, BREAK or TRAP in a destructor
 * ends it alone, and a QUIT stays and leaves the other objects queued. */
void       xb_classes_finalize(xb_ctx *ctx);
/* A message to a value of a type a compiled class is associated with
 * (ClassAssociate): 0 when there is none. xb_class_associate associates the
 * class of `o` with the type named `type` (as ClassName() names it): 1, 0 for
 * an unknown type, -1 when the class has instance variables. */
int        xb_class_scalar_send(xb_ctx *ctx, xb_item *r, const char *message, xb_class_def *from, uint32_t super,
                                uint32_t argc, xb_item *argv);
int        xb_class_associate(const xb_object *o, const char *type);
/* BASE/3005: a class with instance variables cannot answer a type's values (ClassAssociate, a SCALAR CLASS that
 * inherits variables from a class of another module), `operation` the function or the class. */
void       xb_class_scalar_vars_error(xb_ctx *ctx, xb_item *ret, const char *operation, uint32_t argc,
                                      const xb_item *argv);
/* A message to argv[0] as if it were an object of the class whose objects the class function `classFunction` makes
 * (its methods, Self being argv[0]); `*def` keeps that class from the first message on (NULL before it). 0 when the
 * function makes no object of a compiled class, or the class does not answer the message. */
int        xb_class_send_as(xb_ctx *ctx, xb_item *r, xb_func classFunction, xb_class_def **def, const char *message,
                            uint32_t argc, xb_item *argv);
/* Generators (types/generator.c): whether `it` is a generator's enumerator;
 * one step (MoveNext: .T. while it yields an element, its CURRENT variable);
 * the messages it answers besides its CURRENT variable (1 when it did). */
static inline int xb_is_generator(const xb_item *it) {
  return it->type == XB_T_OBJECT && ((const xb_object *)it->v.p)->cls == &xb_generator_class;
}
int        xb_gen_move_next(xb_ctx *ctx, const xb_item *gen);
/* The element the last step yielded (its CURRENT variable). */
static inline void xb_gen_current(const xb_item *gen, xb_item *dst) { xb_object_get(gen->v.p, XB_GEN_CURRENT, dst); }
int        xb_gen_message(xb_ctx *ctx, xb_item *r, const char *message, uint32_t argc, xb_item *argv);

/* Items 1 .. n - 1 of `from` copied into `to` (frame slots after the holder's): a frame has a few slots, which a call
 * of memcpy would cost more than the copy. */
static inline void xb_frame_copy(xb_item *to, const xb_item *from, uint32_t n) {
  for (uint32_t k = 1; k < n; ++k) to[k] = from[k];
}
/* The frame of a resumable function (a generator's, an asynchronous function's) between its steps: an array whose
 * items 1 .. are its slots (slot 0 is its holder, the step's argument). xb_frame_load copies them back into `slots`
 * (as many as both have); xb_frame_store saves `slots` into the array of the holder's variable `k` (made at the first
 * save, grown as needed: allocations), in place, what they hold shared first when the holder is shared (a shared
 * object holds shared ones). One thread at a time runs a step, and the next one runs after the handoff that
 * publishes it (a scheduling, a claim): no other thread reads the frame meanwhile, so neither side takes its lock. */
static inline void xb_frame_load(const xb_array *frame, xb_item *slots, uint32_t nslots) {
  uint32_t n = nslots < frame->len ? nslots : (uint32_t)frame->len;
  xb_frame_copy(slots, xb_array_items(frame), n); /* one thread at a time runs a step */
}
void       xb_frame_store_slow(xb_ctx *ctx, const xb_item *holder, uint32_t k, const xb_item *slots, uint32_t nslots);
static inline void xb_frame_store(xb_ctx *ctx, const xb_item *holder, uint32_t k, const xb_item *slots,
                                  uint32_t nslots) {
  xb_object *o = holder->v.p; /* only the step's flow writes its frame variable */
  if (o->vars[k].type == XB_T_ARRAY && ((const xb_array *)o->vars[k].v.p)->len >= nslots) {
    if (xb_is_shared(o))
      for (uint32_t s = 1; s < nslots; ++s) xb_share(&slots[s]);
    xb_array *frame = o->vars[k].v.p; /* in place: items built in locals and then copied would stall the loads */
    xb_frame_copy(xb_array_items(frame), slots, nslots);
    xb_gc_write_items(frame, 1, nslots);
    return;
  }
  xb_frame_store_slow(ctx, holder, k, slots, nslots); /* the first save, or a growth */
}
/* What every value answers (ClassName), else the error of a message no
 * class has: BASE/1004, or BASE/1005 for an assignment. */
void       xb_object_message(xb_ctx *ctx, xb_item *r, const char *message, uint32_t argc, xb_item *argv);
/* An operator the class of `a` overloads (`op`, an XB_OPERATOR_* message),
 * sent to `a` with `b` and `c` (NULL when absent): 0, and nothing done, when
 * `a` is not an object of a compiled class with that operator. */
int      xb_class_operator(xb_ctx *ctx, xb_item *r, const char *op, const xb_item *a, const xb_item *b,
                           const xb_item *c);
/* Reflection on an object of a compiled class: whether its class, or one it
 * inherits from, is `name` (upper-cased); whether it answers `message` with a
 * method (`method`) or a variable, in any scope; the messages of its EXPORTED
 * variables (or methods; not the assigning "_NAME" ones, nor operators), in
 * the order of its table, stored in `out` when it is not NULL, their count
 * returned. */
int      xb_class_kind_of(const xb_object *o, const char *name);
/* Whether a class of its line implements interface `name` (upper-cased), or one that inherits it. */
#define XB_INTERFACE_SEEN 64 /* the interfaces it goes through, at most */
int      xb_class_implements(const xb_object *o, const char *name);
int      xb_class_has(const xb_object *o, const char *message, int method);
uint32_t xb_class_names(const xb_object *o, int methods, const char **out);
/* "CLASS:MESSAGE" in `buf`, cut to fit: how an error names a message to an object. */
#define XB_MESSAGE_TEXT_SIZE 128
const char *xb_message_text(char buf[XB_MESSAGE_TEXT_SIZE], const char *cls, const char *message);

/* Hashes: keys (strings, numbers, dates) to values, in insertion order. The
 * entries sit in one array, in the order they came, with a hash of each key;
 * a store of more than XB_HASH_SMALL entries has an index of open addressing
 * (linear probing, at most half full; each slot an entry + 1, 0 when free)
 * that finds them, a smaller one is searched in order. A
 * deleted entry leaves a hole (its key NIL) until the array is compacted,
 * when it fills up with enough holes. Positions count live entries: without
 * holes a position is an entry; with holes a Fenwick tree of the live entries
 * (XB_HASH_TREE: built when a position is first needed, kept up to date until
 * the next compaction) finds one in O(log cap). Entries, hashes, tree and
 * index are one XB_GC_HSTORE object, in that order,
 * which the hash's tracing marks; `cap` is a power of two. A number key that
 * equals an integer is that integer (1 and 1.0 are one key); strings compare
 * by bytes, or letter case aside. */
#define XB_HASH_IGNORECASE 0x1u
/* Keyed hashes (types/hash.c): the table met keys chosen to collide, and hashes them with a key of the process. */
#define XB_HASH_KEYED 0x2u
#define XB_HASH_TREE 0x4u /* the store's tree counts the live entries */
#define XB_HASH_SMALL 8u  /* entries a store without an index holds, at most */
#define XB_HASH_MAX_LEN (UINT32_C(1) << 30) /* entries; the index has twice as many slots */
/* xb_hentry and xb_hash: xbrt.h. */

xb_hash *xb_hash_alloc(void); /* empty, without a store */
void     xb_set_hash(xb_item *it, xb_hash *h);
/* Whether `key` can be a key (a string, a number or a date). */
int      xb_hash_key_ok(const xb_item *key);
/* The order of two keys of `h` (-1, 0, 1), as HSort() sorts them: numbers by value, then dates, then strings byte by
 * byte (a prefix first), ASCII letter case aside when `h` ignores it; 0 only for keys that are one key. */
int      xb_hash_key_order(const xb_hash *h, const xb_item *a, const xb_item *b);
/* The entry of `key` (a valid key), or -1; a found one becomes `h`'s hint (a local hash, or under its lock). */
int64_t  xb_hash_find(xb_hash *h, const xb_item *key);
/* Sets `key` (a valid key) to `value`, adding it last when new; `h` must be
 * reachable, `key` and `value` too (rooted). */
void     xb_hash_set(xb_hash *h, const xb_item *key, const xb_item *value);
/* Removes the entry of `key`; 0 when there is none. */
int      xb_hash_del(xb_hash *h, const xb_item *key);
/* The entry at 1-based position `pos` (1 .. len), through the tree when
 * there are holes; NULL outside. */
xb_hentry *xb_hash_at(xb_hash *h, int64_t pos);
/* The 1-based position of `key` (a valid key), or 0. */
int64_t  xb_hash_pos(xb_hash *h, const xb_item *key);
void     xb_hash_del_at(xb_hash *h, int64_t pos);
void     xb_hash_clear(xb_hash *h);
/* Moves the live entries over the holes, keeping their order (positions stay
 * the same), so that position k is entries[k - 1]. */
void     xb_hash_compact(xb_hash *h);
/* Room for `n` entries without growing. */
void     xb_hash_reserve(xb_hash *h, uint32_t n);
/* Fills the empty `dst` (reachable) with the entries of `src`, in order,
 * and its flags; the values themselves are not copied. */
void     xb_hash_copy(xb_hash *dst, const xb_hash *src);
/* Switches letter case aside (XB_HASH_IGNORECASE) on or off; the index is
 * rebuilt. Keys that become equal merge into the first of them, which keeps
 * its spelling and position and takes the value of the last. */
void     xb_hash_set_flags(xb_hash *h, uint32_t flags);
/* Rehashes the keys and rebuilds the index after the caller reordered the
 * entries of a hash without holes (xb_hash_compact first). */
void     xb_hash_reindex(xb_hash *h);
/* A hash that other threads may reach (core/shared.h): each of these runs under its lock when it is shared, reads
 * too, and copies keys and values out; a stored key or value is shared first. Its length; the value of a key (0 when
 * missing; `value` may be NULL); a key's position (0 when missing); the key and value at a position (0 outside);
 * the value at a position replaced by `v` when given, its value then into `now`; a key or a position removed; every
 * entry removed; every value set to `v`; whether letter case matters, changed when `change`. xb_hash_put sets a key
 * (a store it needs allocated before the lock, rooted in `ctx`), xb_hash_reserve_any makes room, and
 * xb_hash_snapshot puts into `out` (a root) a new local hash of the entries as one locked read saw them. */
uint32_t xb_hash_length(const xb_hash *h);
int      xb_hash_get(xb_hash *h, const xb_item *key, xb_item *value);
int      xb_hash_lookup(xb_hash *h, const xb_item *key, xb_item *stored, xb_item *value); /* also its key as stored */
int64_t  xb_hash_position(xb_hash *h, const xb_item *key);
int      xb_hash_entry(xb_hash *h, int64_t pos, xb_item *key, xb_item *value);
int      xb_hash_value_at(xb_hash *h, int64_t pos, const xb_item *v, xb_item *now);
int      xb_hash_replace(xb_hash *h, const xb_item *key, const xb_item *v); /* an existing key's value only */
int      xb_hash_remove(xb_hash *h, const xb_item *key);
int      xb_hash_remove_at(xb_hash *h, int64_t pos);
void     xb_hash_empty(xb_hash *h);
void     xb_hash_fill(xb_hash *h, const xb_item *v);
int      xb_hash_case_match(xb_hash *h, int change, int match);
void     xb_hash_put(xb_ctx *ctx, xb_hash *h, const xb_item *key, const xb_item *value);
void     xb_hash_reserve_any(xb_ctx *ctx, xb_hash *h, uint64_t n);
/* The same for a count the program chose (HAllocate): 0 when it is past the largest hash or no memory holds it. */
int xb_hash_reserve_try(xb_ctx *ctx, xb_hash *h, uint64_t n);
void     xb_hash_snapshot(xb_item *out, xb_hash *h);
/* xb_hash_copy from a hash other threads may reach into an empty one only this one reaches (`dst` reachable). */
void     xb_hash_copy_any(xb_hash *dst, xb_hash *src);
/* The entries of `h` put in the order of `sorted` (a local, compacted copy of it sorted), under its lock: its keys,
 * found in `h` as it is now, each once, with their values now; else `h` stays as it is. */
void     xb_hash_take_order(xb_hash *h, const xb_hash *sorted);

/* The empty string, never collected. */
xb_str *xb_str_empty(void);

/* The letter ValType() gives a value's type. */
char xb_type_letter(const xb_item *it);
/* A string of `len` bytes (unset) that may grow in place up to `room`
 * (XB_STR_APPEND, the room in `hash`; without the flag when the room cannot
 * be recorded); xb_str_share ends the growth. */
xb_str *xb_str_growing(uint64_t len, uint64_t room);
void    xb_str_share(xb_str *s);

/* Dates are Julian day numbers (0: the empty date), years 0 to 9999. Text
 * buffers include the terminating NUL: "YYYYMMDD", and a date by a SET DATE
 * FORMAT picture, which is cut at 10 characters. */
#define XB_DATE_DIGITS_SIZE 9
#define XB_DATE_TEXT_SIZE   11
int64_t xb_date_encode(int year, int month, int day);   /* 0 if invalid */
int     xb_date_valid(int year, int month, int day);    /* a date of the calendar, years 0 to 9999 */
int     xb_date_digits_valid(const char *digits);       /* "YYYYMMDD" of such a date */
void    xb_date_decode(int64_t jd, int *year, int *month, int *day);
void    xb_set_date(xb_item *it, int64_t jd);
void    xb_date_to_digits(int64_t jd, char out[XB_DATE_DIGITS_SIZE]);
/* The date of eight characters YYYYMMDD; the empty date for anything else (a blank, a date that is not one). */
int64_t xb_date_from_digits(const char *digits);
size_t  xb_date_format(int64_t jd, const char *fmt, char out[XB_DATE_TEXT_SIZE]);
/* The same for the eight characters of `digits` as YYYYMMDD (Transform()
 * formats "99999999" this way for a picture's template). */
size_t  xb_date_format_digits(const char *digits, const char *fmt, char out[XB_DATE_TEXT_SIZE]);
int     xb_date_epoch_year(int year, int epoch);
int64_t xb_date_parse(const char *text, size_t len, const char *fmt, int epoch);
int64_t xb_date_today(void);

/* Day arithmetic wraps instead of overflowing; out-of-range results are
 * invalid dates, as in Clipper. */
static inline int64_t xb_add_days(int64_t jd, int64_t days) { return (int64_t)((uint64_t)jd + (uint64_t)days); }

#endif
