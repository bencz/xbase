/* Functions by name at run time: the program's public functions, then the
 * runtime's (a program function hides a runtime one of its name). */
#ifndef XB_SYMBOLS_H
#define XB_SYMBOLS_H

#include "xbrt.h"

#include <stddef.h>

/* A runtime function (generated from include/rtl.ch): `byref` has bit k set
 * when parameter k receives references; bit 31 stands for every parameter
 * from 31 on. */
typedef struct xb_runtime_symbol {
  const char *name;
  xb_func fn;
  uint32_t byref;
  int reserved;             /* RESERVED in rtl.ch: one of Clipper's predefined functions */
} xb_runtime_symbol;
extern const xb_runtime_symbol xb_runtime_symbols[];
extern const uint32_t xb_runtime_symbol_count;

/* What a name designates. A compiled function receives references as they
 * are; a runtime function only where `byref` says so. */
typedef struct xb_symbol_entry {
  xb_funcref ref;           /* the name's function, as a function reference designates it (@Name() in a macro) */
  size_t len;
  int reserved;             /* a runtime function declared RESERVED (Type() calls the others "UI") */
} xb_symbol_entry;

/* Adds the program's symbols (names upper-cased); a name already there keeps
 * its first function. Called while modules register, before the program runs. */
void xb_symbols_add(const xb_symbol *symbols, uint32_t n);

/* Adds the runtime's functions: the table is complete. xb_program_run calls
 * it before anything runs, on the program's one thread. */
void xb_symbols_seal(void);

/* The entry of `name` (`len` bytes, in any letter case), or NULL. */
const xb_symbol_entry *xb_symbol_find(const char *name, size_t len);

/* The positions that receive references as passed (xb_refs_take). */
static inline uint32_t xb_symbol_refs(const xb_symbol_entry *e) { return xb_funcref_refs(&e->ref); }

#endif
