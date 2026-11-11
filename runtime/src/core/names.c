#include "xbrt_int.h"

#include <stdlib.h>
#include <string.h>

struct xb_name_slot {
  const char *name;         /* NULL: free */
  size_t len;
  uint32_t id;
};

/* FNV-1a. */
static size_t hash(const char *name, size_t len) {
  uint64_t h = UINT64_C(14695981039346656037);
  for (size_t i = 0; i < len; ++i) h = (h ^ (unsigned char)name[i]) * UINT64_C(1099511628211);
  return (size_t)h;
}

static xb_name_slot *probe(const xb_names *t, const char *name, size_t len) {
  for (size_t i = hash(name, len) & (t->capacity - 1);; i = (i + 1) & (t->capacity - 1)) {
    xb_name_slot *s = &t->slots[i];
    if (!s->name || (s->len == len && memcmp(s->name, name, len) == 0)) return s;
  }
}

static void grow(xb_names *t) {
  xb_name_slot *old = t->slots;
  size_t n = t->capacity;
  t->capacity = n ? n * 2 : 64;
  t->slots = calloc(t->capacity, sizeof *t->slots);
  if (!t->slots) xb_gc_fatal("out of memory");
  for (size_t i = 0; i < n; ++i)
    if (old[i].name) *probe(t, old[i].name, old[i].len) = old[i];
  free(old);
}

uint32_t xb_names_add(xb_names *t, const char *name, size_t len) {
  if (2 * ((size_t)t->count + 1) > t->capacity) grow(t);
  xb_name_slot *s = probe(t, name, len);
  if (!s->name) {
    if (t->count == UINT32_MAX) xb_gc_fatal("too many names");
    *s = (xb_name_slot){name, len, t->count++};
  }
  return s->id;
}

int xb_names_find(const xb_names *t, const char *name, size_t len, uint32_t *id) {
  if (!t->capacity) return 0;
  const xb_name_slot *s = probe(t, name, len);
  if (!s->name) return 0;
  *id = s->id;
  return 1;
}

int xb_names_find_upper(const xb_names *t, const char *name, size_t len, uint32_t *id) {
  if (len == 0) return xb_names_find(t, name, 0, id); /* nothing to turn upper case */
  char small[64];
  char *upper = len <= sizeof small ? small : malloc(len);
  if (!upper) xb_gc_fatal("out of memory");
  for (size_t i = 0; i < len; ++i) upper[i] = xb_ascii_upper(name[i]);
  int found = xb_names_find(t, upper, len, id);
  if (upper != small) free(upper);
  return found;
}
