/* The macro operator: compiled code's entry points (xb_macro_* in xbrt.h),
 * the stack machine that runs what macro/compile.c makes, and Type(). */
#include "macro/macro.h"

#include "rdd/rdd.h"

#include <stdlib.h>
#include <string.h>

static uint32_t word(const unsigned char **pc) {
  uint32_t w;
  memcpy(&w, *pc, 4);
  *pc += 4;
  return w;
}

/* The memory variable numbered `id`, as compiled code names one. */
static xb_memvar memvar_of(uint32_t id) {
  size_t len;
  xb_memvar m = {xb_memvar_name(id, &len), id};
  return m;
}

static int is_ref(const xb_item *v) { return v->type == XB_T_CELL || v->type == XB_T_REF; }

/* Reads a variable's item into `*r`: through it when it holds a cell or a
 * reference. */
static void read_var(xb_ctx *ctx, xb_item *r, xb_item *var) {
  if (!is_ref(var)) {
    *r = *var;
    return;
  }
  const xb_item *t = xb_ref_target(ctx, var);
  if (t) *r = *t;
}

static void write_var(xb_ctx *ctx, xb_item *var, const xb_item *v) {
  if (!is_ref(var)) {
    *var = *v;
    return;
  }
  xb_item *t = xb_ref_target(ctx, var);
  if (t && !(ctx->pending & XB_PENDING_UNWIND)) {
    *t = *v;
    xb_ref_written(var);
  }
}

static void block_fn(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv, xb_block *self);

/* An expression's evaluation is no activation of its own; a codeblock's is. */
static const xb_proc expression_proc = XB_RUNTIME_PROC("&", XB_PROC_HIDDEN | XB_PROC_TRANSPARENT);
static const xb_proc block_proc = XB_RUNTIME_PROC("&", XB_PROC_HIDDEN);

/* The items of a frame kept in run's own stack frame; a larger one is allocated. */
#define LOCAL_SLOTS 32u

/* Runs a compiled function. Its frame (the stack machine's items, and in slot
 * 0 the function itself) is linked into the context's chain, so the
 * collector sees every item. */
static void run(xb_ctx *ctx, xb_item *ret, const xb_item *code, uint32_t argc, xb_item *argv, xb_block *self) {
  const xb_array *fa = code->v.p;
  const unsigned char *pc = (const unsigned char *)fa->items[0].v.s->data;
  uint32_t nparams = word(&pc), nslots = word(&pc);
  const unsigned char *ops = pc;
  union {
    xb_frame frame;
    unsigned char room[sizeof(xb_frame) + LOCAL_SLOTS * sizeof(xb_item)];
  } local;
  xb_frame *frame = &local.frame;
  if (1 + (size_t)nslots > LOCAL_SLOTS) {
    frame = malloc(sizeof(xb_frame) + (1 + (size_t)nslots) * sizeof(xb_item));
    if (!frame) xb_gc_fatal("out of memory");
  }
  xb_item *s = frame->slots;
  s[0] = *code;
  for (uint32_t k = 0; k < nslots; ++k) {
    if (k < nparams && k < argc) s[1 + k] = argv[k];
    else xb_set_nil(&s[1 + k]);
  }
  xb_frame_push(ctx, frame, self ? &block_proc : &expression_proc);
  frame->nslots = 1 + nslots;
  uint32_t mark = self ? xb_mv_mark(ctx) : 0; /* a codeblock ends the PRIVATEs it creates */
  size_t sp = nparams; /* the top item; the stack starts above the parameters */
  uint32_t areas = 0, areaBefore = 0; /* alias->( expr ) entered, and the work area current before the first */
  if ((const char *)&local < ctx->stack_limit) { /* as compiled code tests its frame: a macro block recursing */
    xb_stack_overflow(ctx);
    goto out;
  }
#define FAILED() (ctx->pending && xb_pending(ctx))
  for (;;) {
    mop op = (mop)*pc++;
    xb_item *r = &s[sp + 1]; /* the scratch item above the top: results land there, rooted */
    switch (op) {
    case MO_NIL: xb_set_nil(&s[++sp]); break;
    case MO_TRUE: xb_set_logical(&s[++sp], 1); break;
    case MO_FALSE: xb_set_logical(&s[++sp], 0); break;
    case MO_CONST: s[++sp] = fa->items[word(&pc)]; break;
    case MO_MV_GET:
    case MO_MV_REF: {
      xb_memvar m = memvar_of(word(&pc));
      xb_set_nil(r);
      if (op == MO_MV_GET) xb_mv_get(ctx, r, &m);
      else xb_mv_ref(ctx, r, &m);
      ++sp;
      if (FAILED()) goto out;
      break;
    }
    case MO_MV_SET: {
      xb_memvar m = memvar_of(word(&pc));
      xb_mv_set(ctx, &m, &s[sp]);
      --sp;
      if (FAILED()) goto out;
      break;
    }
    case MO_MV_FIELD: {
      xb_memvar m = memvar_of(word(&pc));
      xb_field_cache cache = {0};
      xb_set_nil(r);
      xb_mv_field(ctx, r, &m, &cache);
      ++sp;
      if (FAILED()) goto out;
      break;
    }
    case MO_FIELD:
    case MO_FIELD_SET: {
      uint32_t alias = word(&pc);
      const char *name = fa->items[word(&pc)].v.s->data;
      xb_field_cache cache = {0};
      int onStack = alias == XB_MACRO_STACK_ALIAS;
      const char *aliasName = alias >= XB_MACRO_STACK_ALIAS ? NULL : fa->items[alias].v.s->data;
      if (op == MO_FIELD) {
        xb_set_nil(r);
        xb_field_get(ctx, r, onStack ? &s[sp] : NULL, aliasName, name, &cache);
        if (onStack) s[sp] = *r;
        else ++sp;
      } else {
        xb_field_set(ctx, onStack ? &s[sp - 1] : NULL, aliasName, name, &cache, &s[sp]);
        sp -= onStack ? 2 : 1;
      }
      if (FAILED()) goto out;
      break;
    }
    case MO_LOCAL_GET:
      xb_set_nil(r);
      read_var(ctx, r, &s[1 + word(&pc)]);
      ++sp;
      if (FAILED()) goto out;
      break;
    case MO_LOCAL_SET:
      write_var(ctx, &s[1 + word(&pc)], &s[sp]);
      --sp;
      if (FAILED()) goto out;
      break;
    case MO_LOCAL_REF:
      xb_ref_of(ctx, r, &s[1 + word(&pc)], 0);
      ++sp;
      break;
    case MO_UNARY: {
      uint32_t u = word(&pc);
      xb_set_nil(r);
      if (u == MU_NEG) xb_op_neg(ctx, r, &s[sp]);
      else if (u == MU_NOT) xb_op_not(ctx, r, &s[sp]);
      else xb_op_bitnot(ctx, r, &s[sp]);
      s[sp] = *r;
      if (FAILED()) goto out;
      break;
    }
    case MO_BINARY: {
      static void (*const binary[])(xb_ctx *, xb_item *, const xb_item *, const xb_item *) = {
        [MB_ADD] = xb_op_add,       [MB_SUB] = xb_op_sub,       [MB_MUL] = xb_op_mul,       [MB_DIV] = xb_op_div,
        [MB_MOD] = xb_op_mod,       [MB_POW] = xb_op_pow,       [MB_EQ] = xb_op_eq,         [MB_NE] = xb_op_ne,
        [MB_EXACTEQ] = xb_op_exacteq,                           [MB_LT] = xb_op_lt,         [MB_LE] = xb_op_le,
        [MB_GT] = xb_op_gt,         [MB_GE] = xb_op_ge,         [MB_IN] = xb_op_in,         [MB_BITAND] = xb_op_bitand,
        [MB_BITOR] = xb_op_bitor,   [MB_BITXOR] = xb_op_bitxor, [MB_SHL] = xb_op_shl,       [MB_SHR] = xb_op_shr,
      };
      uint32_t b = word(&pc);
      xb_set_nil(r);
      binary[b](ctx, r, &s[sp - 1], &s[sp]);
      s[--sp] = *r;
      if (FAILED()) goto out;
      break;
    }
    case MO_STEP:
      xb_set_nil(r);
      if (word(&pc)) xb_op_inc(ctx, r, &s[sp]);
      else xb_op_dec(ctx, r, &s[sp]);
      s[sp] = *r;
      if (FAILED()) goto out;
      break;
    case MO_JUMP: pc = ops + word(&pc); break;
    case MO_JUMP_FALSE:
    case MO_JUMP_TRUE: {
      uint32_t to = word(&pc);
      if (s[sp].type != XB_T_LOGICAL) {
        xb_cond_error(ctx, &s[sp]);
        goto out;
      }
      if ((s[sp--].v.l != 0) == (op == MO_JUMP_TRUE)) pc = ops + to;
      break;
    }
    case MO_POP: --sp; break;
    case MO_PICK: {
      uint32_t n = word(&pc);
      s[sp + 1] = s[sp - n];
      ++sp;
      break;
    }
    case MO_INSERT: {
      uint32_t n = word(&pc);
      xb_item top = s[sp];
      memmove(&s[sp - n + 1], &s[sp - n], n * sizeof(xb_item));
      s[sp - n] = top;
      break;
    }
    case MO_ARRAY:
    case MO_HASH: {
      uint32_t n = word(&pc);
      size_t base = sp + 1 - (op == MO_HASH ? 2 * (size_t)n : n);
      xb_set_nil(r);
      if (op == MO_ARRAY) xb_array_new(ctx, r, n, &s[base]);
      else xb_hash_new(ctx, r, n, &s[base]);
      s[base] = *r;
      sp = base;
      if (FAILED()) goto out;
      break;
    }
    case MO_INDEX:
    case MO_REF_INDEX:
      xb_set_nil(r);
      if (op == MO_INDEX) xb_op_index(ctx, r, &s[sp - 1], &s[sp]);
      else xb_ref_elem(ctx, r, &s[sp - 1], &s[sp]);
      s[--sp] = *r;
      if (FAILED()) goto out;
      break;
    case MO_INDEX_SET:
      xb_op_index_set(ctx, &s[sp - 2], &s[sp - 1], &s[sp]);
      sp -= 3;
      if (FAILED()) goto out;
      break;
    case MO_CALL: {
      const xb_symbol_entry *e;
      memcpy(&e, pc, sizeof e);
      pc += sizeof e;
      uint32_t argn = word(&pc);
      size_t base = sp + 1 - argn;
      /* a runtime function receives references only where it takes them */
      if (!xb_args_rest(ctx, &s[base], &s[base], argn, 0, xb_symbol_refs(e))) goto out;
      xb_set_nil(r);
      e->fn(ctx, r, argn, &s[base]);
      s[base] = *r;
      sp = base;
      if (FAILED()) goto out;
      break;
    }
    case MO_NO_FUNC: {
      const char *name = fa->items[word(&pc)].v.s->data;
      size_t base = sp + 1 - word(&pc);
      xb_set_nil(r);
      if (!xb_error(ctx, r, EG_NOFUNC, 1001, name, XB_ERR_SUBSTITUTE, 0, NULL)) goto out;
      s[base] = *r;
      sp = base;
      break;
    }
    case MO_SEND: {
      const char *message = fa->items[word(&pc)].v.s->data;
      size_t base = sp + 1 - word(&pc);
      xb_set_nil(r);
      xb_op_send(ctx, r, message, (uint32_t)(sp + 1 - base), &s[base]);
      s[base] = *r;
      sp = base;
      if (FAILED()) goto out;
      break;
    }
    case MO_BLOCK: { /* its function in its one cell */
      const xb_item *child = &fa->items[word(&pc)];
      xb_block *b = xb_block_new(ctx, r, block_fn, 1);
      ++sp; /* the block, rooted, while its cell is made */
      xb_cell *c = xb_gc_alloc(XB_GC_CELL, sizeof(xb_cell));
      c->value = *child;
      b->cells[0] = c;
      xb_gc_write(b);
      break;
    }
    case MO_MACRO:
      if (word(&pc)) {
        xb_macro_set(ctx, &s[sp - 1], &s[sp]);
        sp -= 2;
      } else {
        xb_set_nil(r);
        xb_macro_get(ctx, r, &s[sp]);
        s[sp] = *r;
      }
      if (FAILED()) goto out;
      break;
    case MO_AREA: {
      uint32_t skip = word(&pc), before;
      if (!xb_area_enter(ctx, &s[sp], &before)) {
        if (FAILED()) goto out;
        xb_set_nil(&s[sp]);
        pc = ops + skip;
        break;
      }
      if (areas++ == 0) areaBefore = before;
      xb_set_int(&s[sp], before);
      break;
    }
    case MO_AREA_END:
      ctx->env->areaCurrent = (uint32_t)xb_num_to_i64(&s[sp - 1]);
      --areas;
      s[sp - 1] = s[sp];
      --sp;
      break;
    case MO_RETURN:
      *ret = s[sp];
      goto out;
    }
  }
#undef FAILED
out:
  if (areas) ctx->env->areaCurrent = areaBefore; /* an error or a BREAK left alias->( expr ) */
  if (self) xb_mv_release(ctx, mark);
  xb_frame_pop(ctx, frame);
  if (frame != &local.frame) free(frame);
}

static void block_fn(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv, xb_block *self) {
  run(ctx, ret, &self->cells[0]->value, argc, argv, self);
}

/* A text that keeps inserting itself (c := "&c") stops being expanded after
 * this many insertions in a row with nothing written out, or when the text
 * grows past about this size; what is left is copied as written. */
#define MAX_IDLE_INSERTIONS 4096
#define MAX_TEXT_BYTES ((size_t)1 << 24)

static int name_start(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
static int name_char(char c) { return name_start(c) || (c >= '0' && c <= '9'); }

/* A growing run of bytes. */
typedef struct buffer {
  char *p;
  size_t len, cap;
} buffer;

static void put(buffer *t, const char *p, size_t n) {
  if (t->cap - t->len < n) {
    size_t cap = t->cap ? t->cap : 64;
    while (cap - t->len < n) cap *= 2;
    char *grown = realloc(t->p, cap);
    if (!grown) xb_gc_fatal("out of memory");
    t->p = grown;
    t->cap = cap;
  }
  memcpy(t->p + t->len, p, n);
  t->len += n;
}

/* Puts `n` bytes in front of what is left to scan, kept reversed. */
static void put_reversed(buffer *rest, const char *p, size_t n) {
  for (size_t i = n; i-- > 0;) put(rest, &p[i], 1);
}

/* Clipper's text substitution: each `&name` or `&name.` in `s` whose name is a
 * memory variable holding a string becomes that string, and scanning goes on
 * at the start of what was inserted, so a name may run on past its end;
 * anything else stays as written. Returns `s` itself when nothing changes.
 * What is left to scan is kept reversed (its last byte is the next one), so
 * an insertion is an append. */
static xb_str *substitute(xb_ctx *ctx, xb_str *s) {
  if (!memchr(s->data, '&', s->len)) return s;
  buffer out = {0}, rest = {0}, name = {0};
  put_reversed(&rest, s->data, s->len);
  unsigned idle = 0;
  int changed = 0;
  while (rest.len > 0) {
    const char *next = rest.p + rest.len; /* next[-1] is the next byte, next[-2] the one after it */
    if (next[-1] != '&' || rest.len < 2 || !name_start(next[-2])) {
      put(&out, &next[-1], 1);
      --rest.len;
      idle = 0;
      continue;
    }
    size_t k = 1; /* the name's length */
    while (k + 1 < rest.len && name_char(next[-2 - (ptrdiff_t)k])) ++k;
    name.len = 0;
    for (size_t i = 0; i < k; ++i) put(&name, &next[-2 - (ptrdiff_t)i], 1);
    const xb_item *v = NULL;
    if (idle < MAX_IDLE_INSERTIONS && (k > 1 || name.p[0] != '_')) /* `&_` names nothing */
      v = xb_memvar_peek(ctx, name.p, k);
    if (v && (v->type != XB_T_STRING || out.len + rest.len + v->v.s->len > MAX_TEXT_BYTES)) v = NULL;
    if (!v) { /* `&name` as written */
      put(&out, &next[-1], 1);
      put(&out, name.p, k);
      rest.len -= 1 + k;
      continue;
    }
    rest.len -= 1 + k;
    if (rest.len > 0 && rest.p[rest.len - 1] == '.') --rest.len; /* the dot ends the name */
    put_reversed(&rest, v->v.s->data, v->v.s->len);
    ++idle;
    changed = 1;
  }
  xb_str *result = changed ? xb_str_new(out.p ? out.p : "", out.len) : s;
  free(out.p);
  free(rest.p);
  free(name.p);
  return result;
}

void xb_macro_text(xb_ctx *ctx, xb_item *r, const xb_item *text) {
  *r = *text;
  if (text->type == XB_T_STRING) xb_set_str(r, substitute(ctx, text->v.s));
}

/* The text of `&x`, or error 1065 on a value that is not a string (`result`,
 * when not NULL, takes what the error block returns). */
static const xb_str *text_of(xb_ctx *ctx, xb_item *result, const xb_item *text) {
  if (text->type == XB_T_STRING) return text->v.s;
  xb_item ignored;
  xb_set_nil(&ignored);
  xb_gc_root_push(ctx, &ignored);
  xb_error(ctx, result ? result : &ignored, EG_ARG, 1065, "&", XB_ERR_SUBSTITUTE, 1, text);
  xb_gc_root_pop(ctx, 1);
  return NULL;
}

static void syntax_error(xb_ctx *ctx, xb_item *result, const xb_item *text) {
  xb_item ignored;
  xb_set_nil(&ignored);
  xb_gc_root_push(ctx, &ignored);
  xb_error(ctx, result ? result : &ignored, EG_SYNTAX, 1449, "&", XB_ERR_SUBSTITUTE, 1, text);
  xb_gc_root_pop(ctx, 1);
}

/* Compiles the text of `&x` (the error when it is not an expression of that
 * goal): 1 with the function in `*code`, rooted by the caller. What a text
 * compiles to depends on the text alone (functions are found in the symbol
 * table, complete before the program runs; names keep their numbers), so the
 * function is kept for the next time, in the context's cache. */
static int compile(xb_ctx *ctx, xb_item *code, xb_item *result, const xb_item *text, mgoal goal) {
  if (!text_of(ctx, result, text)) return 0;
  xb_item substituted; /* first, as in Clipper */
  xb_macro_text(ctx, &substituted, text);
  xb_gc_root_push(ctx, &substituted);
  const xb_str *s = substituted.v.s;
  uint32_t slot = (xb_hash_bytes(s->data, s->len) + (uint32_t)goal) % XB_MACRO_CACHE;
  xb_item *key = &ctx->macro_cache[2 * slot], *cached = key + 1;
  int ok = 1;
  if (key->type == XB_T_STRING && ctx->macro_cache_goal[slot] == goal && key->v.s->len == s->len &&
      memcmp(key->v.s->data, s->data, s->len) == 0) {
    *code = *cached;
  } else if (xb_macro_compile(ctx, code, s->data, s->len, goal) & XB_MACRO_SYNTAX) {
    syntax_error(ctx, result, text);
    ok = 0;
  } else {
    *key = substituted;
    *cached = *code;
    ctx->macro_cache_goal[slot] = (uint8_t)goal;
  }
  xb_gc_root_pop(ctx, 1);
  return ok;
}

void xb_macro_get(xb_ctx *ctx, xb_item *r, const xb_item *text) {
  xb_item code;
  xb_set_nil(&code);
  xb_gc_root_push(ctx, &code);
  if (compile(ctx, &code, r, text, MG_VALUE)) run(ctx, r, &code, 0, NULL, NULL);
  xb_gc_root_pop(ctx, 1);
}

void xb_macro_set(xb_ctx *ctx, const xb_item *text, const xb_item *value) {
  xb_item frame[3]; /* the function, the value as its argument, its result */
  for (int k = 0; k < 3; ++k) {
    xb_set_nil(&frame[k]);
    xb_gc_root_push(ctx, &frame[k]);
  }
  frame[1] = *value;
  if (compile(ctx, &frame[0], NULL, text, MG_ASSIGN)) run(ctx, &frame[2], &frame[0], 1, &frame[1], NULL);
  xb_gc_root_pop(ctx, 3);
}

/* The name `&x` gives (blanks around it dropped), or the error when the text
 * is not a name. */
static int name_of(xb_ctx *ctx, xb_item *result, const xb_item *text, const char **name, size_t *len) {
  const xb_str *s = text_of(ctx, result, text);
  if (!s) return 0;
  const char *p = s->data, *end = s->data + s->len;
  while (p < end && (*p == ' ' || *p == '\t')) ++p;
  while (end > p && (end[-1] == ' ' || end[-1] == '\t')) --end;
  int ok = p < end && (xb_ascii_upper(*p) >= 'A' && xb_ascii_upper(*p) <= 'Z' ? 1 : *p == '_');
  for (const char *q = p; ok && q < end; ++q) {
    char c = xb_ascii_upper(*q);
    ok = (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
  }
  if (!ok) {
    syntax_error(ctx, result, text);
    return 0;
  }
  *name = p;
  *len = (size_t)(end - p);
  return 1;
}

void xb_macro_call(xb_ctx *ctx, xb_item *r, const xb_item *text, uint32_t argc, xb_item *argv) {
  const char *name;
  size_t len;
  if (!name_of(ctx, r, text, &name, &len)) return;
  const xb_symbol_entry *e = xb_symbol_find(name, len);
  if (!e) {
    char upper[64];
    size_t n = len < sizeof upper - 1 ? len : sizeof upper - 1;
    for (size_t i = 0; i < n; ++i) upper[i] = xb_ascii_upper(name[i]);
    upper[n] = 0;
    xb_error(ctx, r, EG_NOFUNC, 1001, upper, XB_ERR_SUBSTITUTE, 0, NULL);
    return;
  }
  if (!xb_args_rest(ctx, argv, argv, argc, 0, xb_symbol_refs(e))) return;
  e->fn(ctx, r, argc, argv);
}

void xb_macro_private(xb_ctx *ctx, const xb_item *text, uint32_t mark) {
  const char *name;
  size_t len;
  if (!name_of(ctx, NULL, text, &name, &len)) return;
  xb_memvar m = memvar_of(xb_memvar_number(name, len));
  xb_mv_private(ctx, &m, mark);
}

void xb_macro_public(xb_ctx *ctx, const xb_item *text) {
  const char *name;
  size_t len;
  if (!name_of(ctx, NULL, text, &name, &len)) return;
  xb_memvar m = memvar_of(xb_memvar_number(name, len));
  xb_mv_public(ctx, &m);
}

/* Type( cExpr ): the type of the expression's value, as ValType() gives it,
 * without running the error block: "UE" when it is not an expression or its
 * evaluation fails, "U" when it names a variable that is not visible (also
 * when the evaluation finds it: a variable, field or alias that does not
 * exist) or a function the program does not have, "UI" when it calls a function that is
 * not one of Clipper's predefined ones (it is not evaluated), "B" for a
 * codeblock (not evaluated). */
void xbf_TYPE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (argc < 1 || argv[0].type != XB_T_STRING) {
    xb_fn_arg_error(ctx, ret, 1121, "TYPE", argc, argv);
    return;
  }
  xb_item work[3]; /* the text substituted, the function, its value */
  for (int k = 0; k < 3; ++k) {
    xb_set_nil(&work[k]);
    xb_gc_root_push(ctx, &work[k]);
  }
  xb_macro_text(ctx, &work[0], &argv[0]);
  const char *type;
  char letter[2] = {0, 0};
  uint32_t flags = xb_macro_compile(ctx, &work[1], work[0].v.s->data, work[0].v.s->len, MG_VALUE);
  if (flags & XB_MACRO_SYNTAX) {
    type = "UE";
  } else if (flags & XB_MACRO_BLOCK) {
    type = "B";
  } else if (flags & XB_MACRO_NO_FUNC) {
    type = "U";
  } else if (flags & XB_MACRO_UDF) {
    type = "UI";
  } else if (flags & XB_MACRO_NO_VAR) {
    type = "U";
  } else {
    int32_t trapped = ctx->error_trapped;
    ctx->error_trapped = 0;
    ++ctx->error_trap;
    run(ctx, &work[2], &work[1], 0, NULL, NULL);
    --ctx->error_trap;
    if (ctx->error_trapped) {
      ctx->pending &= ~(uint32_t)XB_PENDING_TRAP;
      type = ctx->error_trapped == EG_NOVAR || ctx->error_trapped == EG_NOALIAS ? "U" : "UE";
    } else {
      letter[0] = xb_type_letter(&work[2]);
      type = letter;
    }
    ctx->error_trapped = trapped;
  }
  xb_gc_root_pop(ctx, 3);
  xb_set_str(ret, xb_str_new(type, strlen(type)));
}
