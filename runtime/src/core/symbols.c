/* The symbol table: the names (core/names.h) and, by number, what each one
 * designates; filled while modules register and completed with the runtime's
 * functions before the program runs; lookups only read it. */
#include "xbrt_int.h"

#include <stdlib.h>
#include <string.h>

static xb_names names;
static xb_symbol_entry *entries; /* by number */
static uint32_t entryCap;

static void add(const char *name, xb_func fn, int native, uint32_t byref, int reserved) {
  size_t len = strlen(name);
  uint32_t before = names.count;
  uint32_t id = xb_names_add(&names, name, len);
  if (names.count == before) return; /* there already: the first function stays */
  if (id == entryCap) {
    uint32_t cap = entryCap ? entryCap * 2 : 256;
    xb_symbol_entry *e = realloc(entries, cap * sizeof *e);
    if (!e) xb_gc_fatal("out of memory");
    entries = e;
    entryCap = cap;
  }
  entries[id] = (xb_symbol_entry){{name, fn, (uint32_t)native, byref}, len, reserved};
}

void xb_symbols_add(const xb_symbol *symbols, uint32_t n) {
  for (uint32_t k = 0; k < n; ++k) add(symbols[k].name, symbols[k].fn, 0, 0, 0);
}

void xb_symbols_seal(void) {
  for (uint32_t k = 0; k < xb_runtime_symbol_count; ++k)
    add(xb_runtime_symbols[k].name, xb_runtime_symbols[k].fn, 1, xb_runtime_symbols[k].byref,
        xb_runtime_symbols[k].reserved);
}

const xb_symbol_entry *xb_symbol_find(const char *name, size_t len) {
  uint32_t id;
  return xb_names_find_upper(&names, name, len, &id) ? &entries[id] : NULL;
}
