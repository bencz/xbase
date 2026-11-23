/* The rules of compiled classes, shared by the runtime and the compiler (xbclass.h). */
#include "xbclass.h"

#include <stdio.h>
#include <string.h>

void xb_line_members(const xb_class_def *const *line, uint32_t n, xb_member_visit visit, void *data) {
  for (uint32_t k = 0; k < n; ++k) {
    const xb_class_def *x = line[k];
    for (uint32_t i = 0; i < x->nmethods; ++i) {
      xb_member m = {XB_MK_METHOD, x->methods[i].flags, x, i, x->methods[i].fn};
      if (visit(data, x->methods[i].message, 0, &m)) return;
    }
    for (uint32_t i = 0; i < x->nvars; ++i) {
      xb_member get = {XB_MK_GET, x->vars[i].flags, x, i, NULL}, set = {XB_MK_SET, x->vars[i].flags, x, i, NULL};
      if (visit(data, x->vars[i].name, 0, &get) || visit(data, x->vars[i].name, 1, &set)) return;
    }
    for (uint32_t i = 0; i < x->nclassVars; ++i) {
      xb_member get = {XB_MK_CLASS_GET, x->classVars[i].flags, x, i, NULL};
      xb_member set = {XB_MK_CLASS_SET, x->classVars[i].flags, x, i, NULL};
      if (visit(data, x->classVars[i].name, 0, &get) || visit(data, x->classVars[i].name, 1, &set)) return;
    }
  }
}

typedef struct found {
  const char *message;
  xb_member *out;
  int hit;
} found;

static int match(void *data, const char *name, int setter, const xb_member *m) {
  found *f = data;
  const char *msg = f->message;
  if (setter) {
    if (msg[0] != '_') return 0;
    ++msg;
  }
  if (strcmp(msg, name) != 0) return 0;
  *f->out = *m;
  f->hit = 1;
  return 1;
}

int xb_line_lookup(const xb_class_def *const *line, uint32_t n, const char *message, xb_member *out) {
  found f = {message, out, 0};
  xb_line_members(line, n, match, &f);
  return f.hit;
}

void xb_line_merge(const xb_class_def **line, uint32_t *n, const xb_class_def *const *parentLine, uint32_t pn) {
  for (uint32_t k = 0; k < pn; ++k) {
    int seen = 0;
    for (uint32_t j = 0; j < *n && !seen; ++j) seen = line[j] == parentLine[k];
    if (!seen) line[(*n)++] = parentLine[k];
  }
}

static int is_hidden(const xb_member *m) { return (m->flags & XB_SCOPE_MASK) == XB_SCOPE_HIDDEN; }

const xb_member *xb_member_from(const xb_member *mf, const xb_member *mc, const xb_class_def *f) {
  if (mf && is_hidden(mf) && mf->owner == f) return mf;
  if (mc && mc->kind == XB_MK_METHOD) return mc;
  if (mf && mf->kind != XB_MK_METHOD && !is_hidden(mf)) return mf;
  return mc ? mc : mf;
}

const xb_member *xb_member_super(const xb_class_def *const *fline, uint32_t fn, const char *message,
                                 xb_class_lookup lookup, void *data) {
  for (uint32_t k = 1; k < fn; ++k) {
    const xb_member *m = lookup(data, fline[k], message);
    if (m && m->owner == fline[k]) return m;
  }
  return NULL;
}

const xb_member *xb_member_reach(const xb_class_def *c, const xb_class_def *f, const xb_class_def *const *fline,
                                 uint32_t fn, int super, int related, const char *message, xb_class_lookup lookup,
                                 void *data) {
  if (super) return xb_member_super(fline, fn, message, lookup, data);
  if (related) return xb_member_from(lookup(data, f, message), lookup(data, c, message), f);
  return lookup(data, c, message);
}

uint32_t xb_line_named(const xb_class_def *const *line, uint32_t n, const char *message) {
  uint32_t k = 0;
  while (k < n && strcmp(line[k]->name, message) != 0) ++k;
  return k;
}

uint32_t xb_line_area(const xb_class_def *const *line, uint32_t k) {
  uint32_t first = 0;
  for (uint32_t j = 0; j < k; ++j) first += line[j]->nvars;
  return first;
}

uint32_t xb_member_scope(const xb_member *m) {
  uint32_t scope = m->flags & XB_SCOPE_MASK;
  if ((m->kind == XB_MK_SET || m->kind == XB_MK_CLASS_SET) && (m->flags & XB_MEMBER_READONLY))
    scope = scope == XB_SCOPE_EXPORTED ? XB_SCOPE_PROTECTED : XB_SCOPE_HIDDEN;
  return scope;
}

int xb_member_scope_ok(const xb_member *m, const xb_class_def *from, int inside) {
  uint32_t scope = xb_member_scope(m);
  return !((scope == XB_SCOPE_HIDDEN && from != m->owner) || (scope == XB_SCOPE_PROTECTED && !inside));
}

/* Whether class `x` declares something answering `message` (a method, or a variable or class variable read or
 * assigned). */
static int declares(const xb_class_def *x, const char *message) {
  for (uint32_t i = 0; i < x->nmethods; ++i)
    if (strcmp(x->methods[i].message, message) == 0) return 1;
  int setter = message[0] == '_';
  for (uint32_t i = 0; i < x->nvars; ++i)
    if (strcmp(x->vars[i].name, message + setter) == 0) return 1;
  for (uint32_t i = 0; i < x->nclassVars; ++i)
    if (strcmp(x->classVars[i].name, message + setter) == 0) return 1;
  return 0;
}

int xb_member_by_runtime(const char *message) {
  return strcmp(message, "NEW") == 0 || strcmp(message, "INIT") == 0 || strcmp(message, "CLASSNAME") == 0;
}

/* Whether the line answers `message` with a member that is no ABSTRACT method, or the runtime does by itself. */
static int answers(const xb_class_def *const *line, uint32_t n, const char *message) {
  xb_member m;
  if (!xb_line_lookup(line, n, message, &m)) return xb_member_by_runtime(message);
  return !(m.kind == XB_MK_METHOD && (m.flags & XB_MEMBER_ABSTRACT));
}

/* The interfaces xb_class_check goes through, at most (an interface's parents may form a cycle). */
#define CHECK_SEEN 64

typedef struct check {
  const xb_class_def *const *line;
  uint32_t n;
  xb_interface_find find;
  void *data;
  int strict;
  char *out;
  size_t size;
  const xb_class_def *seen[CHECK_SEEN];
  uint32_t nseen;
} check;

/* Interface `name` (its def when the module knows it), and those it inherits, against the line: 1 when one breaks. */
static int check_interface(check *c, const char *name, const xb_class_def *def) {
  if (!def) def = c->find(c->data, name);
  if (!def) {
    if (!c->strict) return 0;
    snprintf(c->out, c->size, "IMPLEMENTS %s, which is no interface", name);
    return 1;
  }
  if (!(def->flags & XB_CLASS_INTERFACE)) {
    snprintf(c->out, c->size, "%s %s, which is a class", c->nseen ? "an interface inherits from" : "IMPLEMENTS", name);
    return 1;
  }
  for (uint32_t k = 0; k < c->nseen; ++k)
    if (c->seen[k] == def) return 0; /* gone through: interfaces inheriting from one another require each other's */
  if (c->nseen == CHECK_SEEN) return 0;  /* as many as that: what is past them is not checked */
  c->seen[c->nseen++] = def;
  for (uint32_t i = 0; i < def->nmethods; ++i)
    if (!answers(c->line, c->n, def->methods[i].message)) {
      snprintf(c->out, c->size, "%s does not implement %s:%s", c->line[0]->name, def->name, def->methods[i].message);
      return 1;
    }
  for (uint32_t p = 0; p < def->nparents; ++p)
    if (check_interface(c, def->parents[p].name, def->parents[p].def)) return 1;
  return 0;
}

int xb_class_check(const xb_class_def *const *line, uint32_t n, xb_interface_find find, void *data, int strict,
                   char *out, size_t size) {
  for (uint32_t k = 1; k < n; ++k) {
    if (line[k]->flags & (XB_CLASS_FINAL | XB_CLASS_INTERFACE)) {
      snprintf(out, size, "%s inherits from %s, %s", line[0]->name, line[k]->name,
               line[k]->flags & XB_CLASS_FINAL ? "a FINAL class" : "an interface (IMPLEMENTS names interfaces)");
      return 1;
    }
    for (uint32_t i = 0; i < line[k]->nmethods; ++i) {
      if (!(line[k]->methods[i].flags & XB_MEMBER_FINAL)) continue;
      for (uint32_t j = 0; j < k; ++j)
        if (declares(line[j], line[k]->methods[i].message)) {
          snprintf(out, size, "%s:%s overrides the FINAL method of %s", line[j]->name, line[k]->methods[i].message,
                   line[k]->name);
          return 1;
        }
    }
  }
  if (line[0]->flags & (XB_CLASS_ABSTRACT | XB_CLASS_SCALAR | XB_CLASS_INTERFACE)) return 0;
  for (uint32_t k = 0; k < n; ++k)
    for (uint32_t i = 0; i < line[k]->nmethods; ++i)
      if ((line[k]->methods[i].flags & XB_MEMBER_ABSTRACT) && !answers(line, n, line[k]->methods[i].message)) {
        snprintf(out, size, "%s does not implement the ABSTRACT method %s:%s", line[0]->name, line[k]->name,
                 line[k]->methods[i].message);
        return 1;
      }
  check c = {line, n, find, data, strict, out, size, {0}, 0};
  for (uint32_t k = 0; k < n; ++k)
    for (uint32_t i = 0; i < line[k]->ninterfaces; ++i)
      if (check_interface(&c, line[k]->interfaces[i].name, line[k]->interfaces[i].def)) return 1;
  return 0;
}
