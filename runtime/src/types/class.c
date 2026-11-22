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
 * evaluated again the next time the class is used. Threads prepare classes one
 * at a time (one lock for all, taken again by the thread that holds it as a
 * preparation nests); a prepared class is used without it.
 *
 * A message sent from a method of a class F reaches what F sees when it is
 * F's own HIDDEN member, else a method of the object's class (overridden
 * methods win, ACCESS and ASSIGN over an inherited variable too), else a
 * variable F sees (F's, or its ancestors'), else what the object's class
 * answers. HIDDEN members answer F's own methods only, PROTECTED ones the
 * methods of their class and its subclasses, and a READONLY variable is
 * assigned as if one scope narrower (EXPORTED as PROTECTED, PROTECTED as
 * HIDDEN); anything else is a scope violation (BASE/41, BASE/42). Without a
 * method of its own, a class answers New (it sends Init
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
#include "xbclass.h"

#include <stdlib.h>
#include <string.h>

/* What a message reaches (xbclass.h: the rules shared with the compiler). */
typedef xb_member member;

/* A class's preparation: none yet (or its line failed: tried again), its line
 * while its parents are prepared (meeting it again then means it inherits
 * from itself, unless INIT values evaluated meanwhile need it), tabled, its
 * INIT values while they are evaluated (meeting it then means they need an
 * object of the class), ready. Only the thread preparing it sees the states
 * between none and ready: the others wait for the lock. */
typedef enum cls_state { C_NONE, C_LINE, C_TABLED, C_INIT, C_READY } cls_state;

typedef struct cls {
  xb_class base;            /* what the class's objects point to */
  xb_class_def *def;
  xb_class_def **line;      /* the class, then its ancestors */
  uint32_t nline;
  uint32_t *area;           /* the first slot of each one's variables */
  xb_class_var *slots;      /* every slot's name, for the object */
  xb_names messages;        /* message to its member */
  member *members;
  const char **messageOf;   /* each member's message */
  uint32_t nmembers, capMembers;
  xb_item *initial;         /* its own variables' initial values, then its class variables' (a root) */
  xb_item *model;           /* an object's variables as a new object has them, slot by slot (a root, once ready) */
  uint32_t *deep, ndeep;    /* the slots whose initial value each object copies (an array, a hash) */
  uint32_t *classArea;      /* the first class variable of each class of the line */
  xb_item *classValues;     /* the values of the class variables it holds itself (a root) */
  xb_item **classSlots;     /* where each class variable of the line is: here, or an ancestor's when SHARED */
  uint32_t state;           /* cls_state; ready is published (release) after everything else */
  int finalized;            /* a class of its line has a destructor */
  uint32_t initsBefore;     /* C_LINE: the INIT evaluations in progress when it began */
} cls;

/* The lock of preparations, and the INIT evaluations in progress under it, nested (a class met again in C_LINE with
 * more of them than when it began was reached through INIT values). */
static xb_rlock preparing = XB_RLOCK_INIT;
static uint32_t initsRunning;

/* The classes by name, as modules register them (of one name, the highest level's: XB_LEVEL_PROGRAM, the first of a
 * level). */
static xb_names names;
static xb_class_def **byId;
static uint32_t *levelById;
static uint32_t capById;

/* The SCALAR CLASSes of the types, as modules register them (of a type, the highest level's): the program's
 * (`typeClasses`), then the runtime's (`runtimeTypeClasses`, XB_CLASS_RUNTIME), which answers what the program's does
 * not; used until a class is associated with the type (ClassAssociate). Which registers first depends on the system's
 * order of the modules' constructors, so the two are kept apart. */
static xb_class_def *typeClasses[XB_T_COUNT_];
static uint32_t typeClassLevels[XB_T_COUNT_];
static xb_class_def *runtimeTypeClasses[XB_T_COUNT_];
uint8_t xb_scalar_replaced[XB_T_COUNT_];

void xb_classes_register(xb_class_def *defs, uint32_t n, uint32_t level) {
  for (uint32_t k = 0; k < n; ++k) {
    if (defs[k].flags & XB_CLASS_STATIC) continue;
    if (defs[k].flags & XB_CLASS_SCALAR) {
      int runtime = (defs[k].flags & XB_CLASS_RUNTIME) != 0;
      for (uint32_t t = 0; t < XB_T_COUNT_; ++t) {
        const char *name = xb_type_class_name(t);
        if (!name || strcmp(name, defs[k].name) != 0) continue;
        if (runtime) {
          if (!runtimeTypeClasses[t]) runtimeTypeClasses[t] = &defs[k];
          continue;
        }
        if (!typeClasses[t] || level > typeClassLevels[t]) {
          typeClasses[t] = &defs[k];
          typeClassLevels[t] = level;
        }
        xb_atomic_store8(&xb_scalar_replaced[t], 1, XB_RELEASE);
      }
    }
    uint32_t before = names.count, id = xb_names_add(&names, defs[k].name, strlen(defs[k].name));
    if (names.count == before) {
      if (level > levelById[id]) {
        byId[id] = &defs[k];
        levelById[id] = level;
      }
      continue;
    }
    if (id == capById) {
      capById = capById ? capById * 2 : 32;
      xb_class_def **grown = realloc(byId, capById * sizeof *grown);
      uint32_t *levels = realloc(levelById, capById * sizeof *levels);
      if (!grown || !levels) xb_gc_fatal("out of memory");
      byId = grown;
      levelById = levels;
    }
    byId[id] = &defs[k];
    levelById[id] = level;
  }
}

/* A class's runtime part: made once, never freed, published to the other threads by its preparation. */
static cls *runtime_of(const xb_class_def *def) { return xb_atomic_loadptr(&def->runtime, XB_ACQUIRE); }

static uint32_t position(const cls *c, const xb_class_def *x) {
  for (uint32_t k = 0; k < c->nline; ++k)
    if (c->line[k] == x) return k;
  return UINT32_MAX;
}

/* Whether class `c` is `x` or inherits from it. */
static int kind_of(const cls *c, const xb_class_def *x) { return position(c, x) != UINT32_MAX; }

static void table(cls *c, const char *message, const member *m) {
  uint32_t before = c->messages.count;
  uint32_t id = xb_names_add(&c->messages, message, strlen(message));
  if (c->messages.count == before) return; /* an earlier class of the line has it */
  if (id == c->capMembers) {
    c->capMembers = c->capMembers ? c->capMembers * 2 : 16;
    member *grown = realloc(c->members, c->capMembers * sizeof *grown);
    const char **messages = realloc((void *)c->messageOf, c->capMembers * sizeof *messages);
    if (!grown || !messages) xb_gc_fatal("out of memory");
    c->members = grown;
    c->messageOf = messages;
  }
  c->members[id] = *m;
  c->messageOf[id] = message;
  c->nmembers = id + 1;
}

/* A variable's assigning message ("_NAME"), kept for the life of the program
 * (the table refers to it). */
static const char *setter_of(const char *name) {
  size_t n = strlen(name);
  char *s = malloc(n + 2);
  if (!s) xb_gc_fatal("out of memory");
  s[0] = '_';
  memcpy(s + 1, name, n + 1);
  return s;
}

/* Tables a member (xb_line_members' order: the first one of a message stays). */
static int table_member(void *data, const char *name, int setter, const member *m) {
  cls *c = data;
  if (!setter) {
    table(c, name, m);
    return 0;
  }
  uint32_t id;
  const char *set = setter_of(name);
  if (xb_names_find(&c->messages, set, strlen(set), &id)) free((char *)set);
  else table(c, set, m);
  return 0;
}

/* The tables of a class whose line is known (see the top of the file). */
static void build_tables(cls *c) {
  c->area = malloc(c->nline * sizeof *c->area);
  if (!c->area) xb_gc_fatal("out of memory");
  for (uint32_t k = 0; k < c->nline; ++k) c->area[k] = xb_line_area((const xb_class_def *const *)c->line, k);
  uint32_t slots = c->area[c->nline - 1] + c->line[c->nline - 1]->nvars;
  c->slots = malloc((slots ? slots : 1) * sizeof *c->slots);
  if (!c->slots) xb_gc_fatal("out of memory");
  for (uint32_t k = 0; k < c->nline; ++k) {
    xb_class_def *x = c->line[k];
    for (uint32_t i = 0; i < x->nvars; ++i) c->slots[c->area[k] + i] = (xb_class_var){x->vars[i].name, XB_VAR_ANY};
  }
  xb_line_members((const xb_class_def *const *)c->line, c->nline, table_member, c);
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

/* A new object's variables, once the INIT values are known: the initial value of each variable of the line, in the
 * object's order, which an object copies whole (each array or hash its own copy, from `deep`). */
static void make_model(cls *c) {
  uint32_t n = c->base.nvars;
  c->model = malloc((n ? n : 1) * sizeof *c->model);
  c->deep = malloc((n ? n : 1) * sizeof *c->deep);
  if (!c->model || !c->deep) xb_gc_fatal("out of memory");
  for (uint32_t k = 0; k < c->nline; ++k) {
    const cls *x = runtime_of(c->line[k]);
    for (uint32_t i = 0; i < c->line[k]->nvars; ++i) {
      const xb_item *v = &x->initial[i];
      c->model[c->area[k] + i] = *v;
      if (v->type == XB_T_ARRAY || v->type == XB_T_HASH) c->deep[c->ndeep++] = c->area[k] + i;
    }
  }
  if (n) xb_gc_add_root(c->model, n);
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
  int ok = !(xb_pending_get(ctx) & XB_PENDING_UNWIND);
  if (ok && values.type == XB_T_ARRAY) {
    const xb_array *a = values.v.p;
    for (uint32_t i = 0; i < def->nvars + def->nclassVars && i < a->len; ++i) c->initial[i] = xb_array_items(a)[i];
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
    xb_class_def **grown = realloc(line, (nline + pc->nline) * sizeof *grown);
    if (!grown) xb_gc_fatal("out of memory");
    line = grown;
    xb_line_merge((const xb_class_def **)line, &nline, (const xb_class_def *const *)pc->line, pc->nline);
  }
  *count = nline;
  return line;
}

static void set_state(cls *c, cls_state state) { xb_atomic_store32(&c->state, state, XB_RELEASE); }

/* Prepares the class under the lock of preparations: NULL when an error stopped it, with a request to unwind
 * pending. */
static cls *prepare_locked(xb_ctx *ctx, xb_class_def *def) {
  cls *c = runtime_of(def);
  if (c && c->state == C_READY) return c; /* another thread prepared it meanwhile */
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
    xb_atomic_storeptr(&def->runtime, c, XB_RELEASE);
  }
  if (c->state == C_NONE) {
    set_state(c, C_LINE);
    c->initsBefore = initsRunning;
    c->line = linearize(ctx, def, &c->nline);
    if (!c->line) { /* tried again next time */
      set_state(c, C_NONE);
      return NULL;
    }
    build_tables(c);
  }
  set_state(c, C_INIT);
  int ok = call_init(ctx, c);
  if (!ok) {
    set_state(c, C_TABLED);
    return NULL;
  }
  start_class_vars(c);
  if (!c->model) make_model(c);
  if (xb_storage_is_shared()) { /* threads run: what the class holds is shared before another one can reach it */
    for (uint32_t i = 0; i < c->def->nvars + c->def->nclassVars; ++i) xb_share(&c->initial[i]);
    for (uint32_t k = 0, n = c->classArea[c->nline - 1] + c->line[c->nline - 1]->nclassVars; k < n; ++k)
      xb_share(&c->classValues[k]);
  }
  set_state(c, C_READY);
  return c;
}

/* Prepares the class (see the top of the file): NULL when an error stopped it, with a request to unwind pending. */
static cls *prepare(xb_ctx *ctx, xb_class_def *def) {
  cls *c = runtime_of(def);
  if (c && xb_atomic_load32(&c->state, XB_ACQUIRE) == C_READY) return c;
  xb_rlock_lock(&preparing);
  c = prepare_locked(ctx, def);
  xb_rlock_unlock(&preparing);
  return c;
}

void xb_class_instance(xb_ctx *ctx, xb_item *r, xb_class_def *def) {
  cls *c = prepare(ctx, def);
  if (!c) return;
  uint32_t n = c->base.nvars;
  xb_object *o = xb_gc_alloc(XB_GC_OBJECT, offsetof(xb_object, vars) + n * sizeof(xb_item));
  o->cls = &c->base;
  o->nvars = n;
  for (uint32_t k = 0; k < n; ++k) o->vars[k] = c->model[k]; /* a few items: no call */
  xb_set_object(r, o); /* rooted there from now on */
  xb_gc_write(o);      /* what the model refers to, in an object that may be old (a large one) */
  if (c->finalized) xb_gc_finalizable(o);
  if (!c->ndeep) return;
  xb_item copy;
  xb_set_nil(&copy);
  xb_gc_root_push(ctx, &copy);
  for (uint32_t k = 0; k < c->ndeep; ++k) { /* each object its own array or hash */
    xb_clone_deep(ctx, &copy, &c->model[c->deep[k]]);
    o->vars[c->deep[k]] = copy;
    xb_gc_write(o);
  }
  xb_gc_root_pop(ctx, 1);
}

static member *lookup(cls *c, const char *message) {
  uint32_t id;
  return xb_names_find(&c->messages, message, strlen(message), &id) ? &c->members[id] : NULL;
}

/* What a class answers, for the shared rules (xb_member_reach). */
static const member *lookup_of(void *data, const xb_class_def *c, const char *message) {
  (void)data;
  return lookup(runtime_of(c), message);
}


/* A scope violation: BASE/41 (HIDDEN) or BASE/42 (PROTECTED). */
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
    k = xb_line_named((const xb_class_def *const *)c->line, c->nline, message);
  }
  if ((k == 0 && strcmp(message, c->def->name) != 0) || k >= c->nline || k > UINT16_MAX) return 0;
  *r = *self;
  r->width = (uint16_t)k;
  return 1;
}

_Static_assert(sizeof(size_t) == sizeof(uintptr_t), "a send cache's words are all one size (xbrt.h)");
_Static_assert(sizeof(xb_send_cache) == XB_SEND_CACHE_WORDS * sizeof(void *), "the size the compiler emits (xbrt.h)");
_Static_assert(offsetof(xb_send_cache, cls) == XB_SEND_CACHE_CLS * sizeof(void *) &&
                   offsetof(xb_send_cache, slot) == XB_SEND_CACHE_SLOT * sizeof(void *) &&
                   offsetof(xb_send_cache, kind) == XB_SEND_CACHE_KIND * sizeof(void *) &&
                   offsetof(xb_send_cache, fn) == XB_SEND_CACHE_FN * sizeof(void *),
               "the word positions the compiler reads (xbrt.h)");

/* The send caches are shared by every thread (xbrt.h): a sequence lock. A writer makes the version odd, changes the
 * ways, and makes it even again; a writer that finds another one at work leaves the cache. A reader that sees the
 * version odd, or changed once it has read a way, does the full lookup instead: nothing torn is ever used, and nothing
 * is tried again (unless a reader stops between its two reads of the version while the cache changes 2^31 times on a
 * 32-bit target, and finds it back where it was). */
static int cache_find(xb_send_cache *cache, const void *c, member **m, size_t *slot) {
  size_t version = xb_atomic_loadsz(&cache->version, XB_ACQUIRE);
  if (version & 1) return 0;
  for (size_t k = 0; k < XB_SEND_CACHE_WAYS; ++k) {
    const void *way = xb_atomic_loadptr(&cache->cls[k], XB_RELAXED);
    if (!way) return 0;
    if (way != c) continue;
    *m = xb_atomic_loadptr(&cache->member[k], XB_RELAXED);
    *slot = xb_atomic_loadsz(&cache->slot[k], XB_RELAXED);
    xb_atomic_fence(XB_ACQUIRE);
    return xb_atomic_loadsz(&cache->version, XB_RELAXED) == version;
  }
  return 0;
}

/* What compiled code may do in place for member `m` (XB_SEND_* in xbrt.h). */
static size_t inline_kind(const member *m) {
  switch (m->kind) {
    case XB_MK_GET: return XB_SEND_GET;
    case XB_MK_SET: return XB_SEND_SET;
    case XB_MK_METHOD: return XB_SEND_METHOD;
    default: return XB_SEND_OTHER;
  }
}

/* The member a message reached for class `c`, first in the cache; the oldest of a full cache goes. A runtime class's
 * (`m` NULL) keeps its member in `slot` (xb_runtime_member), and compiled code nothing in place (XB_SEND_OTHER). */
static void cache_put(xb_send_cache *cache, const void *c, const member *m, size_t slot) {
  size_t version = xb_atomic_loadsz(&cache->version, XB_RELAXED);
  if ((version & 1) || !xb_atomic_cassz(&cache->version, &version, version + 1, XB_ACQUIRE)) return;
  xb_atomic_fence(XB_RELEASE); /* the odd version is seen before any way changes */
  for (size_t k = XB_SEND_CACHE_WAYS - 1; k > 0; --k) {
    xb_atomic_storeptr(&cache->cls[k], xb_atomic_loadptr(&cache->cls[k - 1], XB_RELAXED), XB_RELAXED);
    xb_atomic_storeptr(&cache->member[k], xb_atomic_loadptr(&cache->member[k - 1], XB_RELAXED), XB_RELAXED);
    xb_atomic_storesz(&cache->slot[k], xb_atomic_loadsz(&cache->slot[k - 1], XB_RELAXED), XB_RELAXED);
    xb_atomic_storesz(&cache->kind[k], xb_atomic_loadsz(&cache->kind[k - 1], XB_RELAXED), XB_RELAXED);
    xb_atomic_storesz(&cache->fn[k], xb_atomic_loadsz(&cache->fn[k - 1], XB_RELAXED), XB_RELAXED);
  }
  xb_atomic_storeptr(&cache->cls[0], (void *)(uintptr_t)c, XB_RELAXED);
  xb_atomic_storeptr(&cache->member[0], (void *)(uintptr_t)m, XB_RELAXED);
  xb_atomic_storesz(&cache->slot[0], slot, XB_RELAXED);
  xb_atomic_storesz(&cache->kind[0], m ? inline_kind(m) : XB_SEND_OTHER, XB_RELAXED);
  xb_atomic_storesz(&cache->fn[0], m && m->kind == XB_MK_METHOD ? (size_t)(uintptr_t)m->fn : 0, XB_RELAXED);
  xb_atomic_storesz(&cache->version, version + 2, XB_RELEASE);
}

/* What member `m` does for argv[0] (`slot`: the object's variable, or the
 * address of a class variable's value). */
static void perform(xb_ctx *ctx, xb_item *r, const member *m, uintptr_t slot, uint32_t argc, xb_item *argv) {
  xb_item nil;
  switch (m->kind) {
    case XB_MK_METHOD:
      xb_fuse_target(ctx, m->fn);
      m->fn(ctx, r, argc, argv);
      return;
    case XB_MK_CLASS_GET: /* the program's storage: whole under the stripe of its address once it is shared */
      if (xb_storage_is_shared()) xb_seq_get((void *)slot, r, (xb_item *)slot);
      else *r = *(xb_item *)slot;
      return;
    case XB_MK_CLASS_SET: { /* a root: no barrier */
      xb_args_values(ctx, argc, argv); /* the value, not a reference to it */
      xb_set_nil(&nil);
      xb_item v = argc > 1 ? argv[1] : nil;
      if (xb_storage_is_shared()) {
        xb_share(&v);
        xb_seq_put((void *)slot, (xb_item *)slot, &v);
      } else {
        *(xb_item *)slot = v;
      }
      *r = v;
      return;
    }
    case XB_MK_GET: xb_object_get(argv[0].v.p, (uint32_t)slot, r); return; /* arguments are ignored */
    case XB_MK_SET: {
      xb_args_values(ctx, argc, argv); /* the value, not a reference to it */
      if (argc < 2) xb_set_nil(&nil);
      const xb_item *v = argc > 1 ? &argv[1] : &nil;
      xb_object_put(argv[0].v.p, (uint32_t)slot, v);
      *r = *v;
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
  /* A Super send reaches the parent's member of the sending method's class (xb_member_reach): for a value that is no
   * object (a scalar class's method) whatever class its type has now (ClassAssociate() while the method runs). */
  if (super && !related && object) {
    not_found(ctx, r, c, message, &argv[0]);
    return;
  }
  const xb_class_def *const *fline = f ? (const xb_class_def *const *)f->line : NULL;
  const member *m = view ? lookup(runtime_of(c->line[view]), message)
                         : xb_member_reach(c->def, from, fline, f ? f->nline : 0, (int)super, related, message,
                                           lookup_of, NULL);
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
      if (!(xb_pending_get(ctx) & XB_PENDING_UNWIND)) *r = self;
      return;
    }
    if (strcmp(message, "INIT") == 0) {
      *r = argv[0];
      return;
    }
    not_found(ctx, r, c, message, &argv[0]);
    return;
  }
  int inside = f && kind_of(f, m->owner); /* a method of the member's class or of a subclass */
  if (!xb_member_scope_ok(m, from, inside) && !friend_of(ctx, m->owner, from)) {
    violation(ctx, r, c, message, xb_member_scope(m) == XB_SCOPE_HIDDEN);
    return;
  }
  uintptr_t slot = 0;
  if (m->kind == XB_MK_CLASS_GET || m->kind == XB_MK_CLASS_SET) {
    const cls *holder = view ? runtime_of(c->line[view]) : c; /* a cast reads the class it sees */
    slot = (uintptr_t)holder->classSlots[holder->classArea[position(holder, m->owner)] + m->index];
  } else if (m->kind != XB_MK_METHOD) {
    slot = c->area[position(c, m->owner)] + m->index;
  }
  if (cache && object && !view) cache_put(cache, ((const xb_object *)argv[0].v.p)->cls, m, slot);
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
  uint32_t t = argv[0].type;
  if (t >= XB_T_COUNT_) return 0;
  cls *c = xb_atomic_loadptr((void *const *)&scalars[t], XB_ACQUIRE);
  /* the type's own classes, the program's then the runtime's: what neither answers keeps Clipper's error */
  for (int k = 0; !c && k < 2; ++k) {
    xb_class_def *def = k == 0 ? typeClasses[t] : runtimeTypeClasses[t];
    if (!def) continue;
    cls *own = prepare(ctx, def);
    if (!own) return 1; /* the error is pending */
    if (own->base.nvars) { /* inherited from a class of another module, which the compiler could not see */
      xb_class_scalar_vars_error(ctx, r, def->name, argc, argv);
      return 1;
    }
    if (lookup(own, message)) c = own;
  }
  if (!c) return 0;
  dispatch(ctx, r, c, message, from, super, NULL, argc, argv);
  return 1;
}

int xb_class_send_as(xb_ctx *ctx, xb_item *r, xb_func classFunction, xb_class_def **def, const char *message,
                     uint32_t argc, xb_item *argv) {
  xb_class_def *known = xb_atomic_loadptr((void *const *)def, XB_ACQUIRE); /* found once, by any thread */
  if (!known) {
    xb_item made, none;
    xb_set_nil(&made);
    xb_set_nil(&none);
    xb_gc_root_push(ctx, &made);
    classFunction(ctx, &made, 0, &none); /* no arguments; a valid address all the same */
    xb_gc_root_pop(ctx, 1);
    if (xb_pending_get(ctx) & XB_PENDING_UNWIND) return 1;
    if (made.type != XB_T_OBJECT || !((const xb_object *)made.v.p)->cls->compiled) return 0;
    known = ((const xb_object *)made.v.p)->cls->compiled->def;
    xb_atomic_storeptr((void **)def, known, XB_RELEASE);
  }
  cls *c = prepare(ctx, known);
  if (!c) return 1; /* the error is pending */
  if (!lookup(c, message)) return 0;
  dispatch(ctx, r, c, message, NULL, 0, NULL, argc, argv);
  return 1;
}

void xb_class_scalar_vars_error(xb_ctx *ctx, xb_item *ret, const char *operation, uint32_t argc,
                                const xb_item *argv) {
  xb_error_described(ctx, ret, EG_ARG, 3005, "Scalar class cannot have instance variables", operation, 0, argc, argv);
}

int xb_class_associate(const xb_object *o, const char *type) {
  cls *c = o->cls->compiled;
  if (c->base.nvars) return -1;
  int found = 0;
  for (uint32_t t = 0; t < XB_T_COUNT_; ++t) {
    const char *name = xb_type_class_name(t);
    if (name && strcmp(name, type) == 0) {
      xb_atomic_storeptr((void **)&scalars[t], c, XB_RELEASE);
      xb_atomic_store8(&xb_scalar_replaced[t], 1, XB_RELEASE);
      found = 1;
    }
  }
  return found;
}

void xb_op_send_from(xb_ctx *ctx, xb_item *r, const char *message, xb_class_def *from, uint32_t super,
                     xb_send_cache *cache, uint32_t argc, xb_item *argv) {
  if (argv[0].type == XB_T_OBJECT) {
    const xb_object *o = argv[0].v.p;
    member *m;
    size_t slot;
    if (cache && argv[0].width == 0 && cache_find(cache, o->cls, &m, &slot)) { /* a class the cache holds it for */
      if (m) perform(ctx, r, m, slot, argc, argv);
      else xb_runtime_perform(ctx, r, slot, argc, argv);
      return;
    }
    if (o->cls->compiled) {
      dispatch(ctx, r, o->cls->compiled, message, from, super, cache, argc, argv);
      return;
    }
    if (!super && xb_runtime_member(o->cls, message, &slot)) { /* a runtime class's: kept for the next time */
      if (cache) cache_put(cache, o->cls, NULL, slot);
      xb_runtime_perform(ctx, r, slot, argc, argv);
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
  xb_member_kind kind = c->members[id].kind;
  return method ? kind == XB_MK_METHOD : kind == XB_MK_GET || kind == XB_MK_CLASS_GET;
}

uint32_t xb_class_names(const xb_object *o, int methods, const char **out) {
  const cls *c = o->cls->compiled;
  uint32_t n = 0;
  for (uint32_t id = 0; id < c->nmembers; ++id) {
    const member *m = &c->members[id];
    const char *message = c->messageOf[id];
    int named = message[0] >= 'A' && message[0] <= 'Z'; /* not "_NAME", nor an operator's */
    if ((m->flags & XB_SCOPE_MASK) != XB_SCOPE_EXPORTED || !named) continue;
    if (methods ? m->kind != XB_MK_METHOD : m->kind != XB_MK_GET && m->kind != XB_MK_CLASS_GET) continue;
    if (out) out[n] = message;
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
  if (!m || m->kind != XB_MK_METHOD) return 0;
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
  /* One context at a time: also when a destructor's own pending tests, or a collection it makes, come back here. */
  if (!xb_gc_finalizing(1)) return;
  xb_item obj, self, ret, breakValue;
  xb_set_nil(&obj);
  xb_set_nil(&ret);
  xb_set_nil(&breakValue);
  xb_gc_root_push(ctx, &obj);
  xb_gc_root_push(ctx, &ret);
  xb_gc_root_push(ctx, &breakValue);
  int quit = 0;
  while (!quit && xb_gc_next_finalized(&obj)) {
    const xb_class *of = ((const xb_object *)obj.v.p)->cls;
    const cls *c = of->compiled;
    for (uint32_t k = 0; k < (c ? c->nline : 1) && !quit; ++k) {
      xb_func destructor = c ? c->line[k]->destructor : of->destructor;
      if (!destructor) continue;
      uint32_t before = xb_pending_get(ctx) & XB_PENDING_UNWIND; /* the requests pending wait, with their BREAK value */
      breakValue = ctx->break_value;
      xb_pending_clear(ctx, before); /* those alone: a QUIT another thread raises meanwhile stays */
      uint32_t trap = ctx->error_trap; /* a Type() evaluating: the destructor's errors are not its own */
      int32_t trapped = ctx->error_trapped;
      ctx->error_trap = 0;
      self = obj;
      destructor(ctx, &ret, 1, &self);
      quit = (xb_pending_get(ctx) & XB_PENDING_QUIT) != 0;
      xb_pending_clear(ctx, XB_PENDING_UNWIND & ~(uint32_t)XB_PENDING_QUIT);
      xb_pending_set(ctx, before);
      ctx->break_value = breakValue;
      ctx->error_trap = trap;
      ctx->error_trapped = trapped;
    }
  }
  xb_gc_root_pop(ctx, 3);
  if (xb_gc_finalizing(0) && !quit) xb_pending_set(ctx, XB_PENDING_FINALIZE); /* at this context's next test */
}
