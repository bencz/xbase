/* The macro compiler: text to a tree (the compiler's expression grammar and
 * precedence, parser/Expressions.cpp), then the tree to code in the
 * compiler's evaluation order (xir/GenExpr.cpp).
 *
 * A compiled function is an array: item 1 its code (a string of bytes), the
 * other items its constants (numbers, strings, the functions of the
 * codeblocks it makes). The code starts with a header of two 32-bit words,
 * nparams and nslots (the parameters, the stack and one scratch item).
 * Operations are a byte, then their operands as 32-bit words in the host's
 * order (the code never leaves the process):
 *   MO_CONST k              push constant k (an item of the array)
 *   MO_MV_GET id            push the memory variable numbered id (MO_MV_REF:
 *                           a reference to it); MO_MV_SET id pops a value
 *                           into it
 *   MO_LOCAL_* s            the same for the codeblock's parameter s
 *   MO_FIELD k              error 1003 on the name k (alias->name)
 *   MO_UNARY u, MO_BINARY b, MO_STEP inc   operators (macro.h)
 *   MO_JUMP t, MO_JUMP_FALSE t, MO_JUMP_TRUE t   t a code offset; the
 *                           conditional ones pop a condition
 *   MO_POP, MO_PICK n (push a copy of the item n below the top), MO_INSERT n
 *                           (move the top n places down)
 *   MO_ARRAY n, MO_HASH n (n pairs), MO_INDEX, MO_INDEX_SET (pops base,
 *                           index, value), MO_REF_INDEX
 *   MO_CALL entry argc      the entry's address in the next pointer-sized
 *                           bytes; MO_NO_FUNC k argc: error 1001 on name k
 *   MO_SEND k argc          message k to the first of argc items
 *   MO_BLOCK k              a codeblock running function k (as in Clipper, it
 *                           sees its own parameters, not those of the
 *                           codeblock around it)
 *   MO_MACRO set            `&`: pops the text (and, to set, a value)
 *   MO_RETURN               the top is the result */
#include "macro/macro.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* Nesting of expressions the parser follows, and height of the tree it
 * makes (a chain of operators, a[1][2]..., o:a:b...), before it calls the
 * text an error, so that parsing and generating keep the C stack small: the
 * compiler's limit (Parser::kMaxExprNesting). */
#define MAX_NESTING 1024

typedef enum nkind {
  N_NIL, N_TRUE, N_FALSE, N_NUMBER, N_STRING, N_VAR, N_ARG, N_FIELD, N_CALL, N_UNARY, N_BINARY, N_AND, N_OR,
  N_ASSIGN, N_INCDEC, N_IIF, N_REF, N_LIST, N_ARRAY, N_HASH, N_INDEX, N_BLOCK, N_SEND, N_MACRO,
} nkind;

typedef struct node node;
struct node {
  nkind kind;
  int op;                   /* N_UNARY, N_BINARY: MU_* / MB_*; N_ASSIGN: MB_* of a compound one, else -1;
                               N_INCDEC: 1 increment */
  int flag;                 /* N_INCDEC: prefix; N_SEND: written with arguments; N_VAR: M->name */
  int parens;               /* written ( expr ) */
  uint32_t height;          /* of the tree below it, for MAX_NESTING */
  const char *name;         /* N_VAR, N_CALL, N_SEND, N_FIELD: upper case, NUL-terminated (arena) */
  size_t nameLen;
  const mtoken *tok;        /* N_NUMBER, N_STRING */
  node *a, *b, *c;          /* operands */
  node **list;              /* arguments, elements, body; N_HASH keys and values alternating; N_BLOCK params */
  uint32_t n;
};

/* Nodes and names live in an arena freed after compiling. */
typedef struct chunk {
  struct chunk *next;
  size_t used;
  max_align_t room[];
} chunk;
#define CHUNK_BYTES 8192

typedef struct parser {
  mlexer lx;
  chunk *arena;             /* the chunk allocations come from, then the others */
  int error;
  int depth;
} parser;

static void *alloc(parser *p, size_t n) {
  n = (n + sizeof(max_align_t) - 1) / sizeof(max_align_t) * sizeof(max_align_t);
  if (n > CHUNK_BYTES) { /* a chunk of its own, behind the current one */
    chunk *c = malloc(sizeof(chunk) + n);
    if (!c) xb_gc_fatal("out of memory");
    c->used = n;
    c->next = p->arena ? p->arena->next : NULL;
    if (p->arena) {
      p->arena->next = c;
    } else {
      p->arena = c;
      c->used = CHUNK_BYTES; /* full: the next allocation takes a new chunk */
    }
    return c->room;
  }
  if (!p->arena || CHUNK_BYTES - p->arena->used < n) {
    chunk *c = malloc(sizeof(chunk) + CHUNK_BYTES);
    if (!c) xb_gc_fatal("out of memory");
    c->next = p->arena;
    c->used = 0;
    p->arena = c;
  }
  void *at = (char *)p->arena->room + p->arena->used;
  p->arena->used += n;
  return at;
}

static node *mk(parser *p, nkind k) {
  node *n = alloc(p, sizeof *n);
  memset(n, 0, sizeof *n);
  n->kind = k;
  n->op = -1;
  return n;
}

static const char *upper_name(parser *p, const char *s, size_t len) {
  char *u = alloc(p, len + 1);
  for (size_t i = 0; i < len; ++i) u[i] = xb_ascii_upper(s[i]);
  u[len] = 0;
  return u;
}

/* A growing list of nodes. */
typedef struct nlist {
  node **v;
  uint32_t n, cap;
} nlist;

static void push(parser *p, nlist *l, node *x) {
  if (l->n == l->cap) {
    uint32_t cap = l->cap ? l->cap * 2 : 4;
    node **v = alloc(p, cap * sizeof *v);
    if (l->n) memcpy(v, l->v, l->n * sizeof *v);
    l->v = v;
    l->cap = cap;
  }
  l->v[l->n++] = x;
}

/* The current token; after an error, the end: every loop stops there. */
static mtok cur(const parser *p) { return p->error ? MT_END : p->lx.tok.kind; }
static void advance(parser *p) { xb_mlex_next(&p->lx); }
static int accept(parser *p, mtok k) {
  if (cur(p) != k) return 0;
  advance(p);
  return 1;
}
static void fail(parser *p) { p->error = 1; }
static void expect(parser *p, mtok k) {
  if (!accept(p, k)) fail(p);
}
/* The token `n` places after the current one. */
static mtoken peek(const parser *p, int n) {
  mlexer copy = p->lx;
  while (n-- > 0) xb_mlex_next(&copy);
  return copy.tok;
}
static mtok ahead(const parser *p) { return peek(p, 1).kind; }
/* `&name`, the identifier right after the `&` (no blanks between). */
static int at_amp_name(const parser *p) {
  mtoken t = peek(p, 1);
  return cur(p) == MT_AMP && t.kind == MT_IDENT && !t.spaceBefore;
}
static int is_word(const parser *p, const char *w) {
  const mtoken *t = &p->lx.tok;
  size_t n = strlen(w);
  if (t->kind != MT_IDENT || t->len != n) return 0;
  for (size_t i = 0; i < n; ++i)
    if (xb_ascii_upper(t->text[i]) != w[i]) return 0;
  return 1;
}
static const mtoken *keep(parser *p) {
  mtoken *t = alloc(p, sizeof *t);
  *t = p->lx.tok;
  return t;
}

static int is_lvalue(const node *n) {
  return n->kind == N_VAR || n->kind == N_INDEX || n->kind == N_MACRO || (n->kind == N_SEND && !n->flag);
}

static node *parse_expr(parser *p);
static node *parse_postfix(parser *p);
static void fail(parser *p);

/* A node made from its operands (a, b, c and the list): its height, and an
 * error past the limit. */
static node *grown(parser *p, node *n) {
  uint32_t h = 0;
  const node *kids[3] = {n->a, n->b, n->c};
  for (int k = 0; k < 3; ++k)
    if (kids[k] && kids[k]->height > h) h = kids[k]->height;
  for (uint32_t k = 0; k < n->n && n->list; ++k)
    if (n->list[k]->height > h) h = n->list[k]->height;
  n->height = h + 1;
  if (n->height > MAX_NESTING) fail(p);
  return n;
}

static int enter(parser *p) {
  if (++p->depth > MAX_NESTING) {
    fail(p);
    return 0;
  }
  return 1;
}

static node *parse_assign(parser *p);

/* Arguments or elements up to `close`; an empty one is NIL. `refs`: `@` may
 * pass a variable or an element by reference. */
static void parse_args(parser *p, nlist *out, mtok close, int refs) {
  if (cur(p) == close) return;
  for (;;) {
    if (cur(p) == MT_COMMA || cur(p) == close) {
      push(p, out, mk(p, N_NIL));
    } else if (cur(p) == MT_AT) {
      advance(p);
      node *r = mk(p, N_REF);
      r->a = parse_postfix(p);
      if (!refs || (r->a->kind != N_VAR && r->a->kind != N_INDEX) || r->a->parens) fail(p);
      push(p, out, grown(p, r));
    } else {
      push(p, out, parse_expr(p));
    }
    if (!accept(p, MT_COMMA)) break;
  }
}

static node *list_node(parser *p, nkind k, const nlist *l) {
  node *n = mk(p, k);
  n->list = l->v;
  n->n = l->n;
  return grown(p, n);
}

/* {| params | exprs }, {=>}, { k => v, ... } or { elems } (the brace is current). */
static node *parse_braced(parser *p) {
  advance(p);
  if (accept(p, MT_PIPE)) {
    nlist params = {0}, body = {0};
    if (cur(p) != MT_PIPE) {
      do {
        if (cur(p) != MT_IDENT) {
          fail(p);
          break;
        }
        node *v = mk(p, N_VAR);
        v->name = upper_name(p, p->lx.tok.text, p->lx.tok.len);
        v->nameLen = p->lx.tok.len;
        push(p, &params, v);
        advance(p);
      } while (accept(p, MT_COMMA));
    }
    expect(p, MT_PIPE);
    do {
      push(p, &body, parse_expr(p));
    } while (accept(p, MT_COMMA));
    expect(p, MT_RBRACE);
    node *b = list_node(p, N_BLOCK, &body);
    b->a = list_node(p, N_LIST, &params);
    return b;
  }
  if (accept(p, MT_FATARROW)) {
    expect(p, MT_RBRACE);
    return mk(p, N_HASH);
  }
  nlist elems = {0};
  if (cur(p) == MT_COMMA || cur(p) == MT_RBRACE || cur(p) == MT_AT) {
    parse_args(p, &elems, MT_RBRACE, 0);
    expect(p, MT_RBRACE);
    return list_node(p, N_ARRAY, &elems);
  }
  node *first = parse_expr(p);
  if (accept(p, MT_FATARROW)) {
    nlist kv = {0};
    push(p, &kv, first);
    push(p, &kv, parse_expr(p));
    while (accept(p, MT_COMMA)) {
      push(p, &kv, parse_expr(p));
      expect(p, MT_FATARROW);
      push(p, &kv, parse_expr(p));
    }
    expect(p, MT_RBRACE);
    node *h = list_node(p, N_HASH, &kv);
    h->n /= 2;
    return h;
  }
  push(p, &elems, first);
  if (accept(p, MT_COMMA)) {
    nlist rest = {0};
    parse_args(p, &rest, MT_RBRACE, 0);
    if (rest.n == 0) push(p, &rest, mk(p, N_NIL)); /* { a, } has a NIL last */
    for (uint32_t k = 0; k < rest.n; ++k) push(p, &elems, rest.v[k]);
  }
  expect(p, MT_RBRACE);
  return list_node(p, N_ARRAY, &elems);
}

/* A name's node: M->name and MEMVAR->name are memory variables; another
 * alias names a field (none exists yet). */
static node *parse_name(parser *p) {
  const mtoken t = p->lx.tok;
  advance(p);
  node *v = mk(p, N_VAR);
  v->name = upper_name(p, t.text, t.len);
  v->nameLen = t.len;
  if (!accept(p, MT_ARROW)) return v;
  if (cur(p) != MT_IDENT) {
    fail(p);
    return v;
  }
  int memvar = !strcmp(v->name, "M") || !strcmp(v->name, "MEMVAR");
  v->kind = memvar ? N_VAR : N_FIELD;
  v->flag = memvar;
  v->name = upper_name(p, p->lx.tok.text, p->lx.tok.len);
  v->nameLen = p->lx.tok.len;
  advance(p);
  return v;
}

/* A macro inside the macro, `&name` or `&name.` (the `&` is current): the
 * variable holding its text. The text was substituted before it was
 * compiled, so a name joined to other identifiers (`&name.suffix`,
 * `prefix&name.`) is one substitution could not make: an error, as in
 * Clipper. */
static node *parse_macro_name(parser *p) {
  advance(p);
  node *v = mk(p, N_VAR);
  v->name = upper_name(p, p->lx.tok.text, p->lx.tok.len);
  v->nameLen = p->lx.tok.len;
  advance(p);
  if (cur(p) == MT_DOT && !p->lx.tok.spaceBefore) advance(p); /* the dot ends the name */
  if (cur(p) == MT_IDENT && !p->lx.tok.spaceBefore) fail(p);
  return v;
}

static node *parse_primary(parser *p) {
  const mtoken *t = &p->lx.tok;
  switch (t->kind) {
  case MT_NUMBER:
  case MT_STRING: {
    node *n = mk(p, t->kind == MT_NUMBER ? N_NUMBER : N_STRING);
    n->tok = keep(p);
    advance(p);
    return n;
  }
  case MT_TRUE: advance(p); return mk(p, N_TRUE);
  case MT_FALSE: advance(p); return mk(p, N_FALSE);
  case MT_LBRACE: return parse_braced(p);
  case MT_LPAREN: {
    advance(p);
    node *e = parse_expr(p);
    if (cur(p) == MT_COMMA) {
      nlist l = {0};
      push(p, &l, e);
      while (accept(p, MT_COMMA)) push(p, &l, parse_expr(p));
      e = list_node(p, N_LIST, &l);
    }
    expect(p, MT_RPAREN);
    e->parens = 1;
    return e;
  }
  case MT_AMP: { /* a macro inside the macro: &( expr ), or a macro name */
    node *m = mk(p, N_MACRO);
    if (at_amp_name(p)) {
      m->a = parse_macro_name(p);
    } else if (ahead(p) == MT_LPAREN) {
      advance(p);
      advance(p);
      m->a = parse_expr(p);
      expect(p, MT_RPAREN);
    } else {
      fail(p);
    }
    return grown(p, m);
  }
  case MT_IDENT: {
    mtoken amp = peek(p, 1), after = peek(p, 2);
    if (amp.kind == MT_AMP && !amp.spaceBefore && after.kind == MT_IDENT && !after.spaceBefore) { /* prefix&name */
      fail(p);
      return mk(p, N_NIL);
    }
    if (ahead(p) == MT_LPAREN) {
      const char *name = upper_name(p, t->text, t->len);
      size_t len = t->len;
      advance(p);
      advance(p);
      nlist args = {0};
      int iif = !strcmp(name, "IIF") || !strcmp(name, "IF");
      parse_args(p, &args, MT_RPAREN, !iif);
      expect(p, MT_RPAREN);
      if (iif) {
        if (args.n != 3) {
          fail(p);
          return mk(p, N_NIL);
        }
        node *n = mk(p, N_IIF);
        n->a = args.v[0];
        n->b = args.v[1];
        n->c = args.v[2];
        return grown(p, n);
      }
      node *c = list_node(p, N_CALL, &args);
      c->name = name;
      c->nameLen = len;
      return c;
    }
    if (is_word(p, "NIL")) {
      advance(p);
      return mk(p, N_NIL);
    }
    return parse_name(p);
  }
  default:
    fail(p);
    return mk(p, N_NIL);
  }
}

static node *parse_postfix(parser *p) {
  node *e = parse_primary(p);
  for (;;) {
    if (accept(p, MT_LBRACKET)) { /* a[i], a[i, j] == a[i][j] */
      do {
        node *ix = mk(p, N_INDEX);
        ix->a = e;
        ix->b = parse_expr(p);
        e = grown(p, ix);
      } while (accept(p, MT_COMMA));
      expect(p, MT_RBRACKET);
    } else if (accept(p, MT_COLON)) {
      if (cur(p) != MT_IDENT) {
        fail(p);
        return e;
      }
      node *s = mk(p, N_SEND);
      s->a = e;
      s->name = upper_name(p, p->lx.tok.text, p->lx.tok.len);
      s->nameLen = p->lx.tok.len;
      advance(p);
      if (accept(p, MT_LPAREN)) {
        nlist args = {0};
        parse_args(p, &args, MT_RPAREN, 0);
        expect(p, MT_RPAREN);
        s->flag = 1;
        s->list = args.v;
        s->n = args.n;
      }
      e = grown(p, s);
    } else {
      break;
    }
  }
  while (cur(p) == MT_INC || cur(p) == MT_DEC) {
    node *n = mk(p, N_INCDEC);
    n->op = cur(p) == MT_INC;
    advance(p);
    if (!is_lvalue(e)) fail(p);
    n->a = e;
    e = grown(p, n);
  }
  return e;
}

static node *parse_unary(parser *p) {
  if (!enter(p)) return mk(p, N_NIL);
  node *e;
  mtok k = cur(p);
  if (k == MT_MINUS || k == MT_PLUS || k == MT_TILDE) {
    advance(p);
    e = mk(p, N_UNARY);
    e->op = k == MT_MINUS ? MU_NEG : k == MT_TILDE ? MU_BITNOT : -1; /* -1: unary plus, the value as it is */
    e->a = parse_unary(p);
  } else if (k == MT_INC || k == MT_DEC) {
    advance(p);
    e = mk(p, N_INCDEC);
    e->op = k == MT_INC;
    e->flag = 1;
    e->a = parse_unary(p);
    if (!is_lvalue(e->a)) fail(p);
  } else {
    e = parse_postfix(p);
  }
  --p->depth;
  return grown(p, e);
}

/* The binary operators below .NOT., a level per row from the lowest (the
 * compiler's table). */
static const struct {
  mtok tok;
  int op;
} levels[][6] = {
  {{MT_PIPE, MB_BITOR}},
  {{MT_BITXOR, MB_BITXOR}},
  {{MT_AMP, MB_BITAND}},
  {{MT_EQ, MB_EQ}, {MT_EXACTEQ, MB_EXACTEQ}, {MT_NE, MB_NE}},
  {{MT_LT, MB_LT}, {MT_LE, MB_LE}, {MT_GT, MB_GT}, {MT_GE, MB_GE}, {MT_DOLLAR, MB_IN}},
  {{MT_SHL, MB_SHL}, {MT_SHR, MB_SHR}},
  {{MT_PLUS, MB_ADD}, {MT_MINUS, MB_SUB}},
  {{MT_STAR, MB_MUL}, {MT_SLASH, MB_DIV}, {MT_PERCENT, MB_MOD}},
  {{MT_POWER, MB_POW}},
};
#define LEVELS (sizeof levels / sizeof levels[0])

static node *parse_binary(parser *p, size_t level) {
  if (level == LEVELS) return parse_unary(p);
  node *lhs = parse_binary(p, level + 1);
  for (;;) {
    int op = -1;
    for (size_t k = 0; k < 6 && levels[level][k].tok != MT_END; ++k)
      if (cur(p) == levels[level][k].tok) op = levels[level][k].op;
    if (op < 0) return lhs;
    advance(p);
    node *b = mk(p, N_BINARY);
    b->op = op;
    b->a = lhs;
    b->b = parse_binary(p, level + 1);
    lhs = grown(p, b);
  }
}

static node *parse_not(parser *p) {
  if (cur(p) != MT_NOT) return parse_binary(p, 0);
  if (!enter(p)) return mk(p, N_NIL);
  advance(p);
  node *n = mk(p, N_UNARY);
  n->op = MU_NOT;
  n->a = parse_not(p);
  --p->depth;
  return grown(p, n);
}

static node *parse_and(parser *p) {
  node *lhs = parse_not(p);
  while (accept(p, MT_AND)) {
    node *n = mk(p, N_AND);
    n->a = lhs;
    n->b = parse_not(p);
    lhs = grown(p, n);
  }
  return lhs;
}

static node *parse_or(parser *p) {
  node *lhs = parse_and(p);
  while (accept(p, MT_OR)) {
    node *n = mk(p, N_OR);
    n->a = lhs;
    n->b = parse_and(p);
    lhs = grown(p, n);
  }
  return lhs;
}

static int compound_op(mtok k) {
  switch (k) {
  case MT_PLUSEQ: return MB_ADD;
  case MT_MINUSEQ: return MB_SUB;
  case MT_MULEQ: return MB_MUL;
  case MT_DIVEQ: return MB_DIV;
  case MT_MODEQ: return MB_MOD;
  case MT_POWEQ: return MB_POW;
  case MT_ANDEQ: return MB_BITAND;
  case MT_OREQ: return MB_BITOR;
  case MT_XOREQ: return MB_BITXOR;
  case MT_SHLEQ: return MB_SHL;
  case MT_SHREQ: return MB_SHR;
  default: return -2;
  }
}

static node *parse_assign(parser *p) {
  node *lhs = parse_or(p);
  int op = cur(p) == MT_ASSIGN ? -1 : compound_op(cur(p));
  if (op == -2) return lhs;
  if (!enter(p)) return lhs;
  advance(p);
  if (!is_lvalue(lhs)) fail(p);
  node *a = mk(p, N_ASSIGN);
  a->op = op;
  a->a = lhs;
  a->b = parse_assign(p); /* right associative */
  --p->depth;
  return grown(p, a);
}

static node *parse_expr(parser *p) { return parse_assign(p); }

/* Code generation. */

/* A function being generated: the expression, or a codeblock of it. */
typedef struct fn {
  const node *params;       /* the codeblock's N_LIST of parameters (N_VAR), or NULL */
  uint32_t nparams;
  unsigned char *code;
  size_t len, cap;
  xb_item self;             /* the function's array (rooted while generated) */
  uint32_t depth, maxDepth; /* the stack */
} fn;

typedef struct gen {
  xb_ctx *ctx;
  uint32_t flags;
} gen;

static void bytes(fn *f, const void *p, size_t n) {
  if (f->cap - f->len < n) {
    size_t cap = f->cap ? f->cap : 256;
    while (cap - f->len < n) cap *= 2;
    unsigned char *c = realloc(f->code, cap);
    if (!c) xb_gc_fatal("out of memory");
    f->code = c;
    f->cap = cap;
  }
  memcpy(f->code + f->len, p, n);
  f->len += n;
}
static void word(fn *f, uint32_t w) { bytes(f, &w, 4); }

/* Emits `op`, which pops `pops` items and pushes `pushes`. */
static void emit(fn *f, mop op, uint32_t pops, uint32_t pushes) {
  unsigned char b = (unsigned char)op;
  bytes(f, &b, 1);
  f->depth = f->depth - pops + pushes;
  if (f->depth > f->maxDepth) f->maxDepth = f->depth;
}
static void emit1(fn *f, mop op, uint32_t operand, uint32_t pops, uint32_t pushes) {
  emit(f, op, pops, pushes);
  word(f, operand);
}
/* A jump to be patched: its operand's offset. */
static size_t emit_jump(fn *f, mop op) {
  emit1(f, op, 0, op == MO_JUMP ? 0 : 1, 0);
  return f->len - 4;
}
static void patch(fn *f, size_t at) {
  uint32_t to = (uint32_t)f->len;
  memcpy(f->code + at, &to, 4);
}

/* A new constant of the function (`value` rooted): its index in the
 * function's array. */
static uint32_t add_const(fn *f, const xb_item *value) {
  xb_array *a = f->self.v.p;
  size_t k = a->len;
  xb_array_resize(a, k + 1);
  a->items[k] = *value;
  xb_gc_write_item(a, k);
  return (uint32_t)k;
}
static uint32_t add_string(fn *f, const char *s, size_t len) {
  xb_array *a = f->self.v.p;
  size_t k = a->len;
  xb_array_resize(a, k + 1); /* room first: the string is reachable as soon as it exists */
  xb_set_str(&a->items[k], xb_str_new(s, len));
  xb_gc_write_item(a, k);
  return (uint32_t)k;
}

/* Whether a name is a parameter of the codeblock `f`, and which one; any
 * other name is a memory variable. */
static int param_of(const fn *f, const char *name, uint32_t *index) {
  for (uint32_t k = 0; f->params && k < f->nparams; ++k)
    if (!strcmp(f->params->list[k]->name, name)) {
      *index = k;
      return 1;
    }
  return 0;
}

static void gen_expr(gen *g, fn *f, const node *n);

/* Loads a variable: the codeblock's parameter, else a memory variable. */
static void gen_load(gen *g, fn *f, const node *v) {
  if (v->kind == N_ARG) {
    emit1(f, MO_LOCAL_GET, 0, 0, 1);
    return;
  }
  uint32_t index;
  if (!v->flag && param_of(f, v->name, &index)) {
    emit1(f, MO_LOCAL_GET, index, 0, 1);
    return;
  }
  uint32_t id = xb_memvar_number(v->name, v->nameLen);
  if (!xb_memvar_exists(g->ctx, id)) g->flags |= XB_MACRO_NO_VAR;
  emit1(f, MO_MV_GET, id, 0, 1);
}

/* Stores the top item (popped) into a variable. */
static void gen_store(fn *f, const node *v) {
  uint32_t index;
  if (!v->flag && param_of(f, v->name, &index)) {
    emit1(f, MO_LOCAL_SET, index, 1, 0);
    return;
  }
  emit1(f, MO_MV_SET, xb_memvar_number(v->name, v->nameLen), 1, 0);
}

static void gen_list(gen *g, fn *f, node *const *list, uint32_t n) {
  for (uint32_t k = 0; k < n; ++k) gen_expr(g, f, list[k]);
}

/* A condition: jumps to the returned patch point when it is `when` (the
 * compiler's genCond: .NOT. and lists see through, a literal decides
 * alone; anything else is a value that must be logical). Returns SIZE_MAX
 * when it never jumps. */
static size_t gen_cond(gen *g, fn *f, const node *n, int when) {
  switch (n->kind) {
  case N_TRUE:
  case N_FALSE:
    if ((n->kind == N_TRUE) != when) return SIZE_MAX;
    return emit_jump(f, MO_JUMP);
  case N_LIST:
    for (uint32_t k = 0; k + 1 < n->n; ++k) {
      gen_expr(g, f, n->list[k]);
      emit(f, MO_POP, 1, 0);
    }
    return gen_cond(g, f, n->list[n->n - 1], when);
  case N_UNARY:
    if (n->op == MU_NOT) return gen_cond(g, f, n->a, !when);
    break;
  default:
    break;
  }
  gen_expr(g, f, n);
  return emit_jump(f, when ? MO_JUMP_TRUE : MO_JUMP_FALSE);
}

static void patch_if(fn *f, size_t at) {
  if (at != SIZE_MAX) patch(f, at);
}

/* Assigns to a target, in the compiler's order (genAssign, genIndexAssign,
 * genSendAssign), for N_ASSIGN (`value`, `op` -1 or a compound one) and
 * N_INCDEC. The result is left on the stack. */
static void gen_assign(gen *g, fn *f, const node *target, const node *value, int op, int incdec, int inc,
                       int prefix) {
  int plain = !incdec && op < 0;
  switch (target->kind) {
  case N_VAR:
    if (plain) {
      gen_expr(g, f, value);
    } else {
      gen_load(g, f, target);
      if (incdec) {
        if (!prefix) emit1(f, MO_PICK, 0, 0, 1);
        emit1(f, MO_STEP, (uint32_t)inc, 1, 1);
      } else {
        gen_expr(g, f, value);
        emit1(f, MO_BINARY, (uint32_t)op, 2, 1);
      }
    }
    if (!incdec || prefix) emit1(f, MO_PICK, 0, 0, 1);
    gen_store(f, target);
    return;
  case N_INDEX:
    if (plain) {
      gen_expr(g, f, value);
      gen_expr(g, f, target->a);
      gen_expr(g, f, target->b);
      emit1(f, MO_PICK, 2, 0, 1);
      emit(f, MO_INDEX_SET, 3, 0);
      return;
    }
    gen_expr(g, f, target->a);
    gen_expr(g, f, target->b);
    emit1(f, MO_PICK, 1, 0, 1);
    emit1(f, MO_PICK, 1, 0, 1);
    emit(f, MO_INDEX, 2, 1);
    if (incdec && !prefix) { /* keeps the old value below base and index */
      emit1(f, MO_INSERT, 2, 1, 1);
      emit1(f, MO_PICK, 2, 0, 1);
      emit1(f, MO_STEP, (uint32_t)inc, 1, 1);
      emit(f, MO_INDEX_SET, 3, 0);
      return;
    }
    if (incdec) {
      emit1(f, MO_STEP, (uint32_t)inc, 1, 1);
    } else {
      gen_expr(g, f, value);
      emit1(f, MO_BINARY, (uint32_t)op, 2, 1);
    }
    emit1(f, MO_INSERT, 2, 1, 1);
    emit1(f, MO_PICK, 2, 0, 1);
    emit(f, MO_INDEX_SET, 3, 0);
    return;
  case N_SEND: {
    char setter[256];
    if (target->nameLen + 2 > sizeof setter) { /* longer than a name the send could find */
      g->flags |= XB_MACRO_SYNTAX;
      return;
    }
    setter[0] = '_';
    memcpy(setter + 1, target->name, target->nameLen + 1);
    uint32_t set = add_string(f, setter, target->nameLen + 1);
    gen_expr(g, f, target->a);
    if (plain) {
      gen_expr(g, f, value);
      emit1(f, MO_SEND, set, 2, 1);
      word(f, 2);
      return;
    }
    emit1(f, MO_PICK, 0, 0, 1);
    emit1(f, MO_SEND, add_string(f, target->name, target->nameLen), 1, 1);
    word(f, 1);
    if (incdec && !prefix) {
      emit1(f, MO_INSERT, 1, 1, 1);
      emit1(f, MO_PICK, 1, 0, 1);
      emit1(f, MO_STEP, (uint32_t)inc, 1, 1);
      emit1(f, MO_SEND, set, 2, 1);
      word(f, 2);
      emit(f, MO_POP, 1, 0);
      return;
    }
    if (incdec) {
      emit1(f, MO_STEP, (uint32_t)inc, 1, 1);
    } else {
      gen_expr(g, f, value);
      emit1(f, MO_BINARY, (uint32_t)op, 2, 1);
    }
    emit1(f, MO_SEND, set, 2, 1);
    word(f, 2);
    return;
  }
  case N_MACRO:
    if (plain) {
      gen_expr(g, f, value);
      gen_expr(g, f, target->a);
      emit1(f, MO_PICK, 1, 0, 1);
      emit1(f, MO_MACRO, 1, 2, 0);
      return;
    }
    gen_expr(g, f, target->a);
    emit1(f, MO_PICK, 0, 0, 1);
    emit1(f, MO_MACRO, 0, 1, 1);
    if (incdec && !prefix) {
      emit1(f, MO_INSERT, 1, 1, 1);
      emit1(f, MO_PICK, 1, 0, 1);
      emit1(f, MO_STEP, (uint32_t)inc, 1, 1);
      emit1(f, MO_MACRO, 1, 2, 0);
      return;
    }
    if (incdec) {
      emit1(f, MO_STEP, (uint32_t)inc, 1, 1);
    } else {
      gen_expr(g, f, value);
      emit1(f, MO_BINARY, (uint32_t)op, 2, 1);
    }
    emit1(f, MO_INSERT, 1, 1, 1);
    emit1(f, MO_PICK, 1, 0, 1);
    emit1(f, MO_MACRO, 1, 2, 0);
    return;
  default:
    g->flags |= XB_MACRO_SYNTAX;
    return;
  }
}

static void fn_init(gen *g, fn *f, const node *params) {
  memset(f, 0, sizeof *f);
  f->params = params;
  f->nparams = params ? params->n : 0;
  xb_set_array(&f->self, xb_array_alloc(1)); /* item 1: the code, stored when it is complete */
  xb_gc_root_push(g->ctx, &f->self);
}

/* The function's code becomes item 1 of its array; `*out` takes the array. */
static void fn_finish(gen *g, fn *f, xb_item *out) {
  emit(f, MO_RETURN, 1, 0);
  fn header = {0};
  word(&header, f->nparams);
  word(&header, f->nparams + f->maxDepth + 1); /* + the scratch item */
  bytes(&header, f->code, f->len);
  xb_array *a = f->self.v.p;
  xb_set_str(&a->items[0], xb_str_new((const char *)header.code, header.len));
  xb_gc_write_item(a, 0);
  *out = f->self;
  xb_gc_root_pop(g->ctx, 1);
  free(header.code);
  free(f->code);
}

static void gen_block(gen *g, fn *f, const node *n) {
  fn child;
  fn_init(g, &child, n->a);
  for (uint32_t k = 0; k < n->n; ++k) {
    gen_expr(g, &child, n->list[k]);
    if (k + 1 < n->n) emit(&child, MO_POP, 1, 0);
  }
  xb_item code;
  fn_finish(g, &child, &code);
  xb_gc_root_push(g->ctx, &code);
  uint32_t k = add_const(f, &code);
  xb_gc_root_pop(g->ctx, 1);
  emit1(f, MO_BLOCK, k, 0, 1);
}

static void gen_call(gen *g, fn *f, const node *n) {
  gen_list(g, f, n->list, n->n);
  const xb_symbol_entry *e = xb_symbol_find(n->name, n->nameLen);
  if (!e) {
    g->flags |= XB_MACRO_NO_FUNC;
    emit1(f, MO_NO_FUNC, add_string(f, n->name, n->nameLen), n->n, 1);
    word(f, n->n);
    return;
  }
  if (!e->reserved) g->flags |= XB_MACRO_UDF;
  emit(f, MO_CALL, n->n, 1);
  bytes(f, &e, sizeof e);
  word(f, n->n);
}

static void gen_expr(gen *g, fn *f, const node *n) {
  switch (n->kind) {
  case N_NIL: emit(f, MO_NIL, 0, 1); return;
  case N_TRUE: emit(f, MO_TRUE, 0, 1); return;
  case N_FALSE: emit(f, MO_FALSE, 0, 1); return;
  case N_NUMBER: {
    const mtoken *t = n->tok;
    xb_item v;
    if (t->isInt) {
      xb_put(&v, xb_header_word(XB_T_INT, t->width, 0), (uint64_t)t->ival);
    } else {
      uint64_t bits;
      memcpy(&bits, &t->dval, 8);
      xb_put(&v, xb_header_word(XB_T_DOUBLE, t->width, t->dec), bits);
    }
    emit1(f, MO_CONST, add_const(f, &v), 0, 1);
    return;
  }
  case N_STRING: emit1(f, MO_CONST, add_string(f, n->tok->text + 1, n->tok->len - 2), 0, 1); return;
  case N_VAR:
  case N_ARG: gen_load(g, f, n); return;
  case N_FIELD:
    g->flags |= XB_MACRO_NO_VAR;
    emit1(f, MO_FIELD, add_string(f, n->name, n->nameLen), 0, 1);
    return;
  case N_CALL: gen_call(g, f, n); return;
  case N_UNARY:
    gen_expr(g, f, n->a);
    if (n->op >= 0) emit1(f, MO_UNARY, (uint32_t)n->op, 1, 1);
    return;
  case N_BINARY:
    gen_expr(g, f, n->a);
    gen_expr(g, f, n->b);
    emit1(f, MO_BINARY, (uint32_t)n->op, 2, 1);
    return;
  case N_AND:
  case N_OR: { /* the left side decides; the right side's value is the result as it is */
    int isAnd = n->kind == N_AND;
    size_t decided = gen_cond(g, f, n->a, !isAnd);
    gen_expr(g, f, n->b);
    if (decided == SIZE_MAX) return;
    size_t end = emit_jump(f, MO_JUMP);
    patch(f, decided);
    f->depth -= 1; /* the other path */
    emit(f, isAnd ? MO_FALSE : MO_TRUE, 0, 1);
    patch(f, end);
    return;
  }
  case N_ASSIGN: gen_assign(g, f, n->a, n->b, n->op, 0, 0, 0); return;
  case N_INCDEC: gen_assign(g, f, n->a, NULL, -1, 1, n->op, n->flag); return;
  case N_IIF: {
    size_t otherwise = gen_cond(g, f, n->a, 0);
    gen_expr(g, f, n->b);
    size_t end = emit_jump(f, MO_JUMP);
    patch_if(f, otherwise);
    f->depth -= 1;
    gen_expr(g, f, n->c);
    patch(f, end);
    return;
  }
  case N_REF: {
    const node *t = n->a;
    if (t->kind == N_INDEX) {
      gen_expr(g, f, t->a);
      gen_expr(g, f, t->b);
      emit(f, MO_REF_INDEX, 2, 1);
      return;
    }
    uint32_t index;
    if (!t->flag && param_of(f, t->name, &index)) {
      emit1(f, MO_LOCAL_REF, index, 0, 1);
      return;
    }
    uint32_t id = xb_memvar_number(t->name, t->nameLen);
    if (!xb_memvar_exists(g->ctx, id)) g->flags |= XB_MACRO_NO_VAR;
    emit1(f, MO_MV_REF, id, 0, 1);
    return;
  }
  case N_LIST:
    for (uint32_t k = 0; k < n->n; ++k) {
      gen_expr(g, f, n->list[k]);
      if (k + 1 < n->n) emit(f, MO_POP, 1, 0);
    }
    return;
  case N_ARRAY:
    gen_list(g, f, n->list, n->n);
    emit1(f, MO_ARRAY, n->n, n->n, 1);
    return;
  case N_HASH:
    gen_list(g, f, n->list, 2 * n->n);
    emit1(f, MO_HASH, n->n, 2 * n->n, 1);
    return;
  case N_INDEX:
    gen_expr(g, f, n->a);
    gen_expr(g, f, n->b);
    emit(f, MO_INDEX, 2, 1);
    return;
  case N_BLOCK: gen_block(g, f, n); return;
  case N_SEND: {
    gen_expr(g, f, n->a);
    gen_list(g, f, n->list, n->n);
    emit1(f, MO_SEND, add_string(f, n->name, n->nameLen), n->n + 1, 1);
    word(f, n->n + 1);
    return;
  }
  case N_MACRO:
    gen_expr(g, f, n->a);
    emit1(f, MO_MACRO, 0, 1, 1);
    return;
  }
}

static void free_arena(chunk *c) {
  while (c) {
    chunk *next = c->next;
    free(c);
    c = next;
  }
}

uint32_t xb_macro_compile(xb_ctx *ctx, xb_item *code, const char *text, size_t len, mgoal goal) {
  xb_set_nil(code);
  parser p = {0};
  xb_mlex_init(&p.lx, text, len);
  node *e = cur(&p) == MT_END ? (fail(&p), NULL) : parse_expr(&p);
  if (cur(&p) != MT_END || (goal == MG_ASSIGN && e && !is_lvalue(e))) fail(&p);
  if (p.error) {
    free_arena(p.arena);
    return XB_MACRO_SYNTAX;
  }
  gen g = {ctx, e->kind == N_BLOCK ? XB_MACRO_BLOCK : 0u};
  fn top;
  if (goal == MG_ASSIGN) { /* `target := the function's argument` */
    fn_init(&g, &top, NULL);
    top.nparams = 1;
    node arg = {.kind = N_ARG, .op = -1};
    gen_assign(&g, &top, e, &arg, -1, 0, 0, 0);
  } else {
    fn_init(&g, &top, NULL);
    gen_expr(&g, &top, e);
  }
  fn_finish(&g, &top, code);
  free_arena(p.arena);
  if (g.flags & XB_MACRO_SYNTAX) xb_set_nil(code);
  return g.flags;
}
