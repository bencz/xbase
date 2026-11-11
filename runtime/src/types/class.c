/* Classes compiled from xBase (xb_class_def in xbrt.h).
 *
 * A class is prepared the first time it is used: its parents are found, its
 * ancestry is linearized (the class, then each parent's line, depth first and
 * left to right, each class once), each class of the line gets an area of the
 * object's variables of its own (two ancestors' variables of the same name
 * are two variables), and the messages it answers are tabled: for each class
 * of the line in order, its methods, then its variables ("NAME" reads,
 * "_NAME" assigns), a message taken by the first class that has it. The INIT
 * values are evaluated then, once per class; when that fails, they are
 * evaluated again the next time the class is used.
 *
 * A message sent from a method of a class F reaches what F sees when it is
 * F's own HIDDEN member, else a method of the object's class (overridden
 * methods win, ACCESS and ASSIGN over an inherited variable too), else a
 * variable F sees (F's, or its ancestors'), else what the object's class
 * answers. HIDDEN members answer F's own methods only, PROTECTED ones the
 * methods of their class and its subclasses, and a READONLY variable is
 * assigned by those only; anything else is a scope violation (BASE/41,
 * BASE/42). Without a method of its own, a class answers New (it sends Init
 * with the arguments, as its own method would, and returns the object) and
 * Init (the object). */
#include "xbrt_int.h"

#include "rtl/collections.h"

#include <stdlib.h>
#include <string.h>

typedef enum member_kind { M_GET, M_SET, M_METHOD } member_kind;

typedef struct member {
  member_kind kind;
  uint32_t flags;           /* scope, XB_MEMBER_READONLY */
  xb_class_def *owner;
  uint32_t index;           /* M_GET, M_SET: the variable among its owner's */
  xb_func fn;               /* M_METHOD */
} member;

/* A class's preparation: its line while its parents are prepared (meeting it
 * again then means it inherits from itself, unless INIT values evaluated
 * meanwhile need it), its INIT values while they are evaluated (meeting it
 * then means they need an object of the class). */
typedef enum cls_state { C_LINE, C_TABLED, C_INIT, C_READY } cls_state;

typedef struct cls {
  xb_class base;            /* what the class's objects point to */
  xb_class_def *def;
  xb_class_def **line;      /* the class, then its ancestors */
  uint32_t nline;
  uint32_t *area;           /* the first slot of each one's variables */
  xb_class_var *slots;      /* every slot's name, for the object */
  xb_names messages;        /* message to its member */
  member *members;
  uint32_t nmembers, capMembers;
  xb_item *initial;         /* the class's own variables' initial values (a root) */
  cls_state state;
  uint32_t initsBefore;     /* C_LINE: the INIT evaluations in progress when it began */
} cls;

/* The INIT evaluations in progress, nested (a class met again in C_LINE with
 * more of them than when it began was reached through INIT values). */
static uint32_t initsRunning;

/* The classes by name, as modules register them (the first of a name stays). */
static xb_names names;
static xb_class_def **byId;
static uint32_t capById;

void xb_classes_register(xb_class_def *defs, uint32_t n) {
  for (uint32_t k = 0; k < n; ++k) {
    if (defs[k].flags & XB_CLASS_STATIC) continue;
    uint32_t before = names.count, id = xb_names_add(&names, defs[k].name, strlen(defs[k].name));
    if (names.count == before) continue;
    if (id == capById) {
      capById = capById ? capById * 2 : 32;
      xb_class_def **grown = realloc(byId, capById * sizeof *grown);
      if (!grown) xb_gc_fatal("out of memory");
      byId = grown;
    }
    byId[id] = &defs[k];
  }
}

static cls *runtime_of(const xb_class_def *def) { return def->runtime; }

static uint32_t position(const cls *c, const xb_class_def *x) {
  for (uint32_t k = 0; k < c->nline; ++k)
    if (c->line[k] == x) return k;
  return UINT32_MAX;
}

/* Whether class `c` is `x` or inherits from it. */
static int kind_of(const cls *c, const xb_class_def *x) { return position(c, x) != UINT32_MAX; }

static void table(cls *c, const char *message, member m) {
  uint32_t before = c->messages.count;
  uint32_t id = xb_names_add(&c->messages, message, strlen(message));
  if (c->messages.count == before) return; /* an earlier class of the line has it */
  if (id == c->capMembers) {
    c->capMembers = c->capMembers ? c->capMembers * 2 : 16;
    member *grown = realloc(c->members, c->capMembers * sizeof *grown);
    if (!grown) xb_gc_fatal("out of memory");
    c->members = grown;
  }
  c->members[id] = m;
  c->nmembers = id + 1;
}

/* A variable's assigning message ("_NAME"), kept for the life of the program
 * (the table refers to it). */
static const char *setter(const char *name) {
  size_t n = strlen(name);
  char *s = malloc(n + 2);
  if (!s) xb_gc_fatal("out of memory");
  s[0] = '_';
  memcpy(s + 1, name, n + 1);
  return s;
}

/* The tables of a class whose line is known (see the top of the file). */
static void build_tables(cls *c) {
  c->area = malloc(c->nline * sizeof *c->area);
  if (!c->area) xb_gc_fatal("out of memory");
  uint32_t slots = 0;
  for (uint32_t k = 0; k < c->nline; ++k) {
    c->area[k] = slots;
    slots += c->line[k]->nvars;
  }
  c->slots = malloc((slots ? slots : 1) * sizeof *c->slots);
  if (!c->slots) xb_gc_fatal("out of memory");
  for (uint32_t k = 0; k < c->nline; ++k) {
    xb_class_def *x = c->line[k];
    for (uint32_t i = 0; i < x->nvars; ++i) c->slots[c->area[k] + i] = (xb_class_var){x->vars[i].name, XB_VAR_ANY};
    for (uint32_t i = 0; i < x->nmethods; ++i)
      table(c, x->methods[i].message, (member){M_METHOD, x->methods[i].flags, x, 0, x->methods[i].fn});
    for (uint32_t i = 0; i < x->nvars; ++i) {
      table(c, x->vars[i].name, (member){M_GET, x->vars[i].flags, x, i, NULL});
      uint32_t id;
      const char *set = setter(x->vars[i].name);
      if (xb_names_find(&c->messages, set, strlen(set), &id)) free((char *)set);
      else table(c, set, (member){M_SET, x->vars[i].flags, x, i, NULL});
    }
  }
  c->base.name = c->def->name;
  c->base.nvars = slots;
  c->base.vars = c->slots;
  c->base.compiled = c;
  uint32_t nvars = c->def->nvars;
  c->initial = malloc((nvars ? nvars : 1) * sizeof *c->initial);
  if (!c->initial) xb_gc_fatal("out of memory");
  for (uint32_t i = 0; i < nvars; ++i) xb_set_nil(&c->initial[i]);
  if (nvars) xb_gc_add_root(c->initial, nvars);
}

/* The INIT values: 0 when evaluating them failed (they stay NIL). */
static int call_init(xb_ctx *ctx, cls *c) {
  const xb_class_def *def = c->def;
  if (!def->init) return 1;
  xb_item values;
  xb_set_nil(&values);
  xb_gc_root_push(ctx, &values);
  ++initsRunning;
  def->init(ctx, &values, 0, NULL);
  --initsRunning;
  int ok = !(ctx->pending & XB_PENDING_UNWIND);
  if (ok && values.type == XB_T_ARRAY) {
    const xb_array *a = values.v.p;
    for (uint32_t i = 0; i < def->nvars && i < a->len; ++i) c->initial[i] = a->items[i];
  }
  xb_gc_root_pop(ctx, 1);
  return ok;
}

static cls *prepare(xb_ctx *ctx, xb_class_def *def);

/* Class `def`'s line: NULL when a parent could not be prepared. */
static xb_class_def **linearize(xb_ctx *ctx, xb_class_def *def, uint32_t *count) {
  xb_class_def **line = malloc(sizeof *line);
  if (!line) xb_gc_fatal("out of memory");
  line[0] = def;
  uint32_t nline = 1;
  for (uint32_t p = 0; p < def->nparents; ++p) {
    const xb_class_parent *parent = &def->parents[p];
    xb_class_def *pd = parent->def;
    uint32_t id;
    if (!pd && xb_names_find(&names, parent->name, strlen(parent->name), &id)) pd = byId[id];
    cls *pc = NULL;
    if (pd) pc = prepare(ctx, pd);
    else xb_error(ctx, NULL, EG_NOFUNC, 1001, parent->name, 0, 0, NULL); /* as the parent's class function would */
    if (!pc) {
      free(line);
      return NULL;
    }
    for (uint32_t k = 0; k < pc->nline; ++k) {
      int seen = 0;
      for (uint32_t j = 0; j < nline && !seen; ++j) seen = line[j] == pc->line[k];
      if (seen) continue;
      xb_class_def **grown = realloc(line, (nline + 1) * sizeof *grown);
      if (!grown) xb_gc_fatal("out of memory");
      line = grown;
      line[nline++] = pc->line[k];
    }
  }
  *count = nline;
  return line;
}

/* Prepares the class (see the top of the file): NULL when an error stopped
 * it, with a request to unwind pending. */
static cls *prepare(xb_ctx *ctx, xb_class_def *def) {
  cls *c = runtime_of(def);
  if (c && c->state == C_READY) return c;
  if (c && (c->state == C_LINE || c->state == C_INIT)) {
    int cycle = c->state == C_LINE && initsRunning == c->initsBefore;
    const char *what = cycle ? "Class inherits from itself" : "Class used by its own initial values";
    xb_error_described(ctx, NULL, EG_ARG, 0, what, def->name, 0, 0, NULL);
    return NULL;
  }
  if (!c) {
    c = calloc(1, sizeof *c);
    if (!c) xb_gc_fatal("out of memory");
    c->def = def;
    c->state = C_LINE;
    c->initsBefore = initsRunning;
    def->runtime = c;
    c->line = linearize(ctx, def, &c->nline);
    if (!c->line) { /* tried again next time */
      def->runtime = NULL;
      free(c);
      return NULL;
    }
    build_tables(c);
  }
  c->state = C_INIT;
  int ok = call_init(ctx, c);
  c->state = ok ? C_READY : C_TABLED;
  return ok ? c : NULL;
}

void xb_class_instance(xb_ctx *ctx, xb_item *r, xb_class_def *def) {
  cls *c = prepare(ctx, def);
  if (!c) return;
  xb_object *o = xb_object_new(&c->base);
  xb_set_object(r, o); /* rooted there from now on */
  xb_item copy;
  xb_set_nil(&copy);
  xb_gc_root_push(ctx, &copy);
  for (uint32_t k = 0; k < c->nline; ++k) {
    const cls *x = runtime_of(c->line[k]);
    for (uint32_t i = 0; i < c->line[k]->nvars; ++i) {
      const xb_item *v = &x->initial[i];
      if (v->type == XB_T_ARRAY || v->type == XB_T_HASH) xb_clone_deep(&copy, v); /* each object its own */
      else copy = *v;
      o->vars[c->area[k] + i] = copy;
      xb_gc_write(o);
    }
  }
  xb_gc_root_pop(ctx, 1);
}

static member *lookup(cls *c, const char *message) {
  uint32_t id;
  return xb_names_find(&c->messages, message, strlen(message), &id) ? &c->members[id] : NULL;
}

/* The member a message reaches among the classes after `from` in its line
 * (Super:message). */
static member *lookup_super(cls *from, const char *message) {
  for (uint32_t k = 1; k < from->nline; ++k) {
    member *m = lookup(runtime_of(from->line[k]), message);
    if (m && m->owner == from->line[k]) return m;
  }
  return NULL;
}

/* A scope violation: BASE/41 (HIDDEN) or BASE/42 (PROTECTED, READONLY). */
static void violation(xb_ctx *ctx, xb_item *r, const cls *c, const char *message, int hidden) {
  char op[XB_MESSAGE_TEXT_SIZE];
  xb_error(ctx, r, EG_NOMETHOD, hidden ? 41 : 42, xb_message_text(op, c->def->name, message), XB_ERR_SUBSTITUTE, 0,
           NULL);
}

/* A message the class does not answer: BASE/1004, or BASE/1005 to assign, with the object. */
static void not_found(xb_ctx *ctx, xb_item *r, const cls *c, const char *message, xb_item *self) {
  char op[XB_MESSAGE_TEXT_SIZE];
  int assign = message[0] == '_';
  xb_error_described(ctx, r, assign ? EG_NOVARMETHOD : EG_NOMETHOD, assign ? 1005 : 1004, "Message not found",
                     xb_message_text(op, c->def->name, message), XB_ERR_SUBSTITUTE, 1, self);
}

static int is_hidden(const member *m) { return (m->flags & XB_SCOPE_MASK) == XB_SCOPE_HIDDEN; }

/* The member a message from a method of `f` reaches in an object of `c`, a
 * class of `f`'s kind (see the top of the file). */
static member *lookup_from(cls *c, cls *f, const char *message) {
  member *mf = lookup(f, message), *mc = lookup(c, message);
  if (mf && is_hidden(mf) && mf->owner == f->def) return mf;
  if (mc && mc->kind == M_METHOD) return mc;
  if (mf && mf->kind != M_METHOD && !is_hidden(mf)) return mf;
  return mc ? mc : mf;
}

int xb_class_send(xb_ctx *ctx, xb_item *r, const char *message, xb_class_def *from, uint32_t super, uint32_t argc,
                  xb_item *argv) {
  xb_object *o = argv[0].v.p;
  cls *c = o->cls->compiled;
  cls *f = from ? runtime_of(from) : NULL;
  int related = f && kind_of(c, from); /* Self is of the method's class (it may have been assigned another object) */
  if (super && !related) {
    not_found(ctx, r, c, message, &argv[0]);
    return 1;
  }
  member *m = super ? lookup_super(f, message) : related ? lookup_from(c, f, message) : lookup(c, message);
  if (!m) {
    if (strcmp(message, "NEW") == 0) { /* New: Init with the arguments, as the class's own method sends it */
      xb_item self = argv[0];
      xb_gc_root_push(ctx, &self);
      if (super) xb_class_send(ctx, r, "INIT", from, 1, argc, argv);
      else xb_class_send(ctx, r, "INIT", c->def, 0, argc, argv);
      xb_gc_root_pop(ctx, 1);
      if (!(ctx->pending & XB_PENDING_UNWIND)) *r = self;
      return 1;
    }
    if (strcmp(message, "INIT") == 0) {
      *r = argv[0];
      return 1;
    }
    if (super && f->nline > 1 && strcmp(message, "CLASSNAME") == 0) { /* Super:ClassName() is the parent's */
      const char *name = f->line[1]->name;
      xb_set_str(r, xb_str_new(name, strlen(name)));
      return 1;
    }
    if (strcmp(message, "CLASSNAME") == 0) xb_object_message(ctx, r, message, argc, argv);
    else not_found(ctx, r, c, message, &argv[0]);
    return 1;
  }
  uint32_t scope = m->flags & XB_SCOPE_MASK;
  int inside = f && kind_of(f, m->owner); /* a method of the member's class or of a subclass */
  if ((scope == XB_SCOPE_HIDDEN && from != m->owner) || (scope == XB_SCOPE_PROTECTED && !inside) ||
      (m->kind == M_SET && (m->flags & XB_MEMBER_READONLY) && !inside)) {
    violation(ctx, r, c, message, scope == XB_SCOPE_HIDDEN);
    return 1;
  }
  if (m->kind == M_METHOD) {
    m->fn(ctx, r, argc, argv);
    return 1;
  }
  uint32_t slot = c->area[position(c, m->owner)] + m->index;
  if (m->kind == M_GET) {
    *r = o->vars[slot]; /* arguments are ignored */
  } else {
    xb_item nil;
    xb_set_nil(&nil);
    o->vars[slot] = argc > 1 ? argv[1] : nil;
    xb_gc_write(o);
    *r = o->vars[slot];
  }
  return 1;
}

void xb_op_send_from(xb_ctx *ctx, xb_item *r, const char *message, xb_class_def *from, uint32_t super, uint32_t argc,
                     xb_item *argv) {
  if (argv[0].type == XB_T_OBJECT && ((const xb_object *)argv[0].v.p)->cls->compiled)
    xb_class_send(ctx, r, message, from, super, argc, argv);
  else
    xb_op_send(ctx, r, message, argc, argv);
}
