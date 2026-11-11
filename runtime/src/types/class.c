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
 * Init (the object).
 *
 * A message naming a class of the object's line that no member answers (SUPER:
 * its first parent) is a cast: the object, its item's `width` holding the
 * class's position in the line. A message to a cast reaches what that class
 * answers (its methods, its variables in their areas of the object); a method
 * still receives the object itself as Self. */
#include "xbrt_int.h"

#include "rtl/collections.h"

#include <stdlib.h>
#include <string.h>

typedef enum member_kind { M_GET, M_SET, M_CLASS_GET, M_CLASS_SET, M_METHOD } member_kind;

typedef struct member {
  const char *message;
  member_kind kind;
  uint32_t flags;           /* scope, XB_MEMBER_READONLY */
  xb_class_def *owner;
  uint32_t index;           /* the variable (or class variable) among its owner's */
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
  xb_item *initial;         /* its own variables' initial values, then its class variables' (a root) */
  uint32_t *classArea;      /* the first class variable of each class of the line */
  xb_item *classValues;     /* the values of the class variables it holds itself (a root) */
  xb_item **classSlots;     /* where each class variable of the line is: here, or an ancestor's when SHARED */
  cls_state state;
  int finalized;            /* a class of its line has a destructor */
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
  m.message = message;
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
      table(c, x->methods[i].message, (member){NULL, M_METHOD, x->methods[i].flags, x, 0, x->methods[i].fn});
    for (uint32_t i = 0; i < x->nvars; ++i) {
      table(c, x->vars[i].name, (member){NULL, M_GET, x->vars[i].flags, x, i, NULL});
      uint32_t id;
      const char *set = setter(x->vars[i].name);
      if (xb_names_find(&c->messages, set, strlen(set), &id)) free((char *)set);
      else table(c, set, (member){NULL, M_SET, x->vars[i].flags, x, i, NULL});
    }
    for (uint32_t i = 0; i < x->nclassVars; ++i) {
      table(c, x->classVars[i].name, (member){NULL, M_CLASS_GET, x->classVars[i].flags, x, i, NULL});
      uint32_t id;
      const char *set = setter(x->classVars[i].name);
      if (xb_names_find(&c->messages, set, strlen(set), &id)) free((char *)set);
      else table(c, set, (member){NULL, M_CLASS_SET, x->classVars[i].flags, x, i, NULL});
    }
  }
  c->classArea = malloc(c->nline * sizeof *c->classArea);
  if (!c->classArea) xb_gc_fatal("out of memory");
  uint32_t nclass = 0;
  for (uint32_t k = 0; k < c->nline; ++k) {
    c->classArea[k] = nclass;
    nclass += c->line[k]->nclassVars;
  }
  c->classValues = malloc((nclass ? nclass : 1) * sizeof *c->classValues);
  c->classSlots = malloc((nclass ? nclass : 1) * sizeof *c->classSlots);
  if (!c->classValues || !c->classSlots) xb_gc_fatal("out of memory");
  for (uint32_t k = 0; k < c->nline; ++k) {
    const xb_class_def *x = c->line[k];
    for (uint32_t i = 0; i < x->nclassVars; ++i) { /* an ancestor's shared one is in its own area (the first) */
      uint32_t at = c->classArea[k] + i;
      xb_set_nil(&c->classValues[at]);
      c->classSlots[at] = k && (x->classVars[i].flags & XB_MEMBER_SHARED) ? &runtime_of(x)->classValues[i]
                                                                          : &c->classValues[at];
    }
  }
  if (nclass) xb_gc_add_root(c->classValues, nclass);
  for (uint32_t k = 0; k < c->nline; ++k) c->finalized |= c->line[k]->destructor != NULL;
  c->base.name = c->def->name;
  c->base.nvars = slots;
  c->base.vars = c->slots;
  c->base.compiled = c;
  uint32_t ninitial = c->def->nvars + c->def->nclassVars;
  c->initial = malloc((ninitial ? ninitial : 1) * sizeof *c->initial);
  if (!c->initial) xb_gc_fatal("out of memory");
  for (uint32_t i = 0; i < ninitial; ++i) xb_set_nil(&c->initial[i]);
  if (ninitial) xb_gc_add_root(c->initial, ninitial);
}

/* The class variables' values once the INIT values are known: its own, and a
 * copy of each ancestor's initial value for those it does not share. */
static void start_class_vars(cls *c) {
  for (uint32_t k = 0; k < c->nline; ++k) {
    const xb_class_def *x = c->line[k];
    const cls *owner = runtime_of(x);
    for (uint32_t i = 0; i < x->nclassVars; ++i)
      if (!k || !(x->classVars[i].flags & XB_MEMBER_SHARED))
        c->classValues[c->classArea[k] + i] = owner->initial[x->nvars + i];
  }
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
    for (uint32_t i = 0; i < def->nvars + def->nclassVars && i < a->len; ++i) c->initial[i] = a->items[i];
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
  if (!ok) return NULL;
  start_class_vars(c);
  return c;
}

void xb_class_instance(xb_ctx *ctx, xb_item *r, xb_class_def *def) {
  cls *c = prepare(ctx, def);
  if (!c) return;
  xb_object *o = xb_object_new(&c->base);
  xb_set_object(r, o); /* rooted there from now on */
  if (c->finalized) xb_gc_finalizable(o);
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

/* Whether what runs is a friend of class `owner`: a method of a class it
 * names (`from`), or a function it names, or a codeblock that one made. */
static int friend_of(const xb_ctx *ctx, const xb_class_def *owner, const xb_class_def *from) {
  for (uint32_t k = 0; from && k < owner->nfriendClasses; ++k)
    if (strcmp(owner->friendClasses[k], from->name) == 0) return 1;
  if (!owner->nfriendFunctions) return 0;
  const xb_frame *fr = ctx->frames; /* a macro's frame is its caller's code */
  while (fr && (!fr->proc || (fr->proc->flags & XB_PROC_TRANSPARENT))) fr = fr->prev;
  if (!fr || from) return 0;
  const char *name = fr->proc->name;
  while (strncmp(name, XB_BLOCK_PREFIX, strlen(XB_BLOCK_PREFIX)) == 0) name += strlen(XB_BLOCK_PREFIX);
  for (uint32_t k = 0; k < owner->nfriendFunctions; ++k)
    if (strcmp(owner->friendFunctions[k], name) == 0) return 1;
  return 0;
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

/* The object seen as the class of its line named `message` (also SUPER: the
 * first parent of the class it is seen as, `view`), a cast (see the top of the
 * file); 0 when there is none. */
static int cast(xb_item *r, const cls *c, uint32_t view, const xb_item *self, const char *message) {
  uint32_t k = 0;
  if (strcmp(message, "SUPER") == 0) {
    const cls *seen = runtime_of(c->line[view]);
    if (seen->nline < 2) return 0;
    k = position(c, seen->line[1]); /* the first parent's line comes first after the class */
  } else {
    while (k < c->nline && strcmp(c->line[k]->name, message) != 0) ++k;
  }
  if ((k == 0 && strcmp(message, c->def->name) != 0) || k >= c->nline || k > UINT16_MAX) return 0;
  *r = *self;
  r->width = (uint16_t)k;
  return 1;
}

/* What member `m` does for argv[0] (`slot`: the object's variable, or the
 * address of a class variable's value). */
static void perform(xb_ctx *ctx, xb_item *r, const member *m, uintptr_t slot, uint32_t argc, xb_item *argv) {
  xb_item nil;
  xb_set_nil(&nil);
  switch (m->kind) {
    case M_METHOD: m->fn(ctx, r, argc, argv); return;
    case M_CLASS_GET: *r = *(xb_item *)slot; return;
    case M_CLASS_SET: /* a root: no barrier */
      *(xb_item *)slot = argc > 1 ? argv[1] : nil;
      *r = *(xb_item *)slot;
      return;
    case M_GET: *r = ((xb_object *)argv[0].v.p)->vars[slot]; return; /* arguments are ignored */
    case M_SET: {
      xb_object *o = argv[0].v.p;
      o->vars[slot] = argc > 1 ? argv[1] : nil;
      xb_gc_write(o);
      *r = o->vars[slot];
      return;
    }
  }
}

/* The message to argv[0], an object of class `c` or a value of a type `c` is
 * associated with (see the top of the file); with `cache`, the member reached
 * is stored there when the next message to an object of the same class can
 * take it without looking (not through a cast, nor for a scalar class). */
static void dispatch(xb_ctx *ctx, xb_item *r, cls *c, const char *message, xb_class_def *from, uint32_t super,
                     xb_send_cache *cache, uint32_t argc, xb_item *argv) {
  int object = argv[0].type == XB_T_OBJECT; /* else a value of a scalar class, which has no variables */
  cls *f = from ? runtime_of(from) : NULL;
  uint32_t view = object && argv[0].width < c->nline ? argv[0].width : 0; /* a cast: the class of the line it sees */
  if (object) argv[0].width = 0; /* a method's Self is the object itself */
  int related = f && kind_of(c, from); /* Self is of the method's class (it may have been assigned another object) */
  if (super && !related) {
    not_found(ctx, r, c, message, &argv[0]);
    return;
  }
  member *m = view       ? lookup(runtime_of(c->line[view]), message)
              : super    ? lookup_super(f, message)
              : related  ? lookup_from(c, f, message)
                         : lookup(c, message);
  if (!m) {
    if (object && cast(r, c, view, &argv[0], message)) return;
    if (strcmp(message, "CLASSNAME") == 0) { /* a cast's is the class it sees; Super:ClassName() the parent's */
      const char *name = view ? c->line[view]->name : super && f->nline > 1 ? f->line[1]->name : c->def->name;
      xb_set_str(r, xb_str_new(name, strlen(name)));
      return;
    }
    if (strcmp(message, "NEW") == 0) { /* New: Init with the arguments, as the class's own method sends it */
      xb_item self = argv[0];
      xb_gc_root_push(ctx, &self);
      if (super) dispatch(ctx, r, c, "INIT", from, 1, NULL, argc, argv);
      else dispatch(ctx, r, c, "INIT", c->def, 0, NULL, argc, argv);
      xb_gc_root_pop(ctx, 1);
      if (!(ctx->pending & XB_PENDING_UNWIND)) *r = self;
      return;
    }
    if (strcmp(message, "INIT") == 0) {
      *r = argv[0];
      return;
    }
    not_found(ctx, r, c, message, &argv[0]);
    return;
  }
  uint32_t scope = m->flags & XB_SCOPE_MASK;
  int inside = f && kind_of(f, m->owner); /* a method of the member's class or of a subclass */
  int assigns = m->kind == M_SET || m->kind == M_CLASS_SET;
  if (((scope == XB_SCOPE_HIDDEN && from != m->owner) || (scope == XB_SCOPE_PROTECTED && !inside) ||
       (assigns && (m->flags & XB_MEMBER_READONLY) && !inside)) &&
      !friend_of(ctx, m->owner, from)) {
    violation(ctx, r, c, message, scope == XB_SCOPE_HIDDEN);
    return;
  }
  uintptr_t slot = 0;
  if (m->kind == M_CLASS_GET || m->kind == M_CLASS_SET) {
    const cls *holder = view ? runtime_of(c->line[view]) : c; /* a cast reads the class it sees */
    slot = (uintptr_t)holder->classSlots[holder->classArea[position(holder, m->owner)] + m->index];
  } else if (m->kind != M_METHOD) {
    slot = c->area[position(c, m->owner)] + m->index;
  }
  if (cache && object && !view) {
    cache->cls = ((const xb_object *)argv[0].v.p)->cls;
    cache->member = m;
    cache->slot = slot;
  }
  perform(ctx, r, m, slot, argc, argv);
}

int xb_class_send(xb_ctx *ctx, xb_item *r, const char *message, xb_class_def *from, uint32_t super, uint32_t argc,
                  xb_item *argv) {
  const xb_object *o = argv[0].v.p;
  dispatch(ctx, r, o->cls->compiled, message, from, super, NULL, argc, argv);
  return 1;
}

/* The classes associated with the types of values (ClassAssociate). */
static cls *scalars[XB_T_COUNT_];

int xb_class_scalar_send(xb_ctx *ctx, xb_item *r, const char *message, xb_class_def *from, uint32_t super,
                         uint32_t argc, xb_item *argv) {
  cls *c = argv[0].type < XB_T_COUNT_ ? scalars[argv[0].type] : NULL;
  if (!c) return 0;
  dispatch(ctx, r, c, message, from, super, NULL, argc, argv);
  return 1;
}

int xb_class_associate(const xb_object *o, const char *type) {
  cls *c = o->cls->compiled;
  if (c->base.nvars) return -1;
  int found = 0;
  for (uint32_t t = 0; t < XB_T_COUNT_; ++t) {
    const char *name = xb_type_class_name((xb_type)t);
    if (name && strcmp(name, type) == 0) {
      scalars[t] = c;
      found = 1;
    }
  }
  return found;
}

void xb_op_send_from(xb_ctx *ctx, xb_item *r, const char *message, xb_class_def *from, uint32_t super,
                     xb_send_cache *cache, uint32_t argc, xb_item *argv) {
  if (argv[0].type == XB_T_OBJECT) {
    const xb_object *o = argv[0].v.p;
    if (o->cls == cache->cls && argv[0].width == 0) { /* the class the cache holds the member for */
      perform(ctx, r, cache->member, cache->slot, argc, argv);
      return;
    }
    if (o->cls->compiled) {
      dispatch(ctx, r, o->cls->compiled, message, from, super, cache, argc, argv);
      return;
    }
  }
  if (!xb_class_scalar_send(ctx, r, message, from, super, argc, argv)) xb_op_send(ctx, r, message, argc, argv);
}

int xb_class_kind_of(const xb_object *o, const char *name) {
  const cls *c = o->cls->compiled;
  for (uint32_t k = 0; k < c->nline; ++k)
    if (strcmp(c->line[k]->name, name) == 0) return 1;
  return 0;
}

int xb_class_has(const xb_object *o, const char *message, int method) {
  const cls *c = o->cls->compiled;
  uint32_t id;
  if (!xb_names_find(&c->messages, message, strlen(message), &id)) return 0;
  member_kind kind = c->members[id].kind;
  return method ? kind == M_METHOD : kind == M_GET || kind == M_CLASS_GET;
}

uint32_t xb_class_names(const xb_object *o, int methods, const char **out) {
  const cls *c = o->cls->compiled;
  uint32_t n = 0;
  for (uint32_t id = 0; id < c->nmembers; ++id) {
    const member *m = &c->members[id];
    int named = m->message[0] >= 'A' && m->message[0] <= 'Z'; /* not "_NAME", nor an operator's */
    if ((m->flags & XB_SCOPE_MASK) != XB_SCOPE_EXPORTED || !named) continue;
    if (methods ? m->kind != M_METHOD : m->kind != M_GET && m->kind != M_CLASS_GET) continue;
    if (out) out[n] = m->message;
    ++n;
  }
  return n;
}

int xb_class_operator(xb_ctx *ctx, xb_item *r, const char *op, const xb_item *a, const xb_item *b,
                      const xb_item *c) {
  const xb_object *o = a->v.p;
  cls *k = o->cls->compiled;
  if (!k) return 0;
  if (a->width && a->width < k->nline) k = runtime_of(k->line[a->width]); /* a cast: the class it is seen as */
  const member *m = lookup(k, op);
  if (!m || m->kind != M_METHOD) return 0;
  xb_item argv[3] = {*a};
  uint32_t argc = 1;
  if (b) argv[argc++] = *b;
  if (c) argv[argc++] = *c;
  for (uint32_t i = 0; i < argc; ++i) xb_gc_root_push(ctx, &argv[i]);
  xb_class_send(ctx, r, op, NULL, 0, argc, argv);
  xb_gc_root_pop(ctx, argc);
  return 1;
}

void xb_classes_finalize(xb_ctx *ctx) {
  static int running; /* a destructor's own pending tests, or a collection it makes */
  if (running) return;
  running = 1;
  xb_item obj, self, ret, breakValue;
  xb_set_nil(&obj);
  xb_set_nil(&ret);
  xb_gc_root_push(ctx, &obj);
  xb_gc_root_push(ctx, &ret);
  xb_gc_root_push(ctx, &breakValue);
  int quit = 0;
  while (!quit && xb_gc_next_finalized(&obj)) {
    const cls *c = ((const xb_object *)obj.v.p)->cls->compiled;
    for (uint32_t k = 0; k < c->nline && !quit; ++k) {
      xb_func destructor = c->line[k]->destructor;
      if (!destructor) continue;
      uint32_t before = ctx->pending; /* the requests already pending wait, with their BREAK value */
      breakValue = ctx->break_value;
      ctx->pending &= ~(uint32_t)XB_PENDING_UNWIND;
      self = obj;
      destructor(ctx, &ret, 1, &self);
      quit = (ctx->pending & XB_PENDING_QUIT) != 0;
      ctx->pending = before | (ctx->pending & XB_PENDING_QUIT);
      ctx->break_value = breakValue;
    }
  }
  xb_gc_root_pop(ctx, 3);
  running = 0;
}
