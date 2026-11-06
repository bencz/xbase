/* The symbol table: an open-addressing hash of names (linear probing, at most
 * half full), filled while modules register and completed with the runtime's
 * functions before the program runs; lookups only read it. */
#include "xbrt_int.h"

#include <stdlib.h>
#include <string.h>

static xb_symbol_entry *slots; /* fn NULL: free */
static size_t capacity, used;   /* capacity: a power of two */

/* FNV-1a. */
static size_t hash(const char *name, size_t len) {
  uint64_t h = UINT64_C(14695981039346656037);
  for (size_t i = 0; i < len; ++i) h = (h ^ (unsigned char)name[i]) * UINT64_C(1099511628211);
  return (size_t)h;
}

static xb_symbol_entry *probe(const char *name, size_t len) {
  for (size_t i = hash(name, len) & (capacity - 1);; i = (i + 1) & (capacity - 1)) {
    xb_symbol_entry *e = &slots[i];
    if (!e->fn || (e->len == len && memcmp(e->name, name, len) == 0)) return e;
  }
}

static void grow(void) {
  xb_symbol_entry *old = slots;
  size_t n = capacity;
  capacity = capacity ? capacity * 2 : 256;
  slots = calloc(capacity, sizeof *slots);
  if (!slots) xb_gc_fatal("out of memory");
  for (size_t i = 0; i < n; ++i)
    if (old[i].fn) *probe(old[i].name, old[i].len) = old[i];
  free(old);
}

static void add(const char *name, xb_func fn, int native, uint32_t byref) {
  if (2 * (used + 1) > capacity) grow();
  size_t len = strlen(name);
  xb_symbol_entry *e = probe(name, len);
  if (e->fn) return;
  *e = (xb_symbol_entry){name, len, fn, native, byref};
  ++used;
}

void xb_symbols_add(const xb_symbol *symbols, uint32_t n) {
  for (uint32_t k = 0; k < n; ++k) add(symbols[k].name, symbols[k].fn, 0, 0);
}

void xb_symbols_seal(void) {
  for (uint32_t k = 0; k < xb_runtime_symbol_count; ++k)
    add(xb_runtime_symbols[k].name, xb_runtime_symbols[k].fn, 1, xb_runtime_symbols[k].byref);
}

const xb_symbol_entry *xb_symbol_find(const char *name, size_t len) {
  if (!capacity) return NULL;
  xb_symbol_entry *e = probe(name, len);
  return e->fn ? e : NULL;
}
