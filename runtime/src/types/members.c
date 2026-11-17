/* The rules of compiled classes, shared by the runtime and the compiler (xbclass.h). */
#include "xbclass.h"

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
