/* Memory variables as the runtime's own functions see them (core/memvar.c);
 * compiled code uses the xb_mv_* entry points of xbrt.h. */
#ifndef XB_MEMVAR_H
#define XB_MEMVAR_H

#include "core/context.h"

/* Numbers the memory variables of a registering module: their names are permanent (see core/memvar.c). */
void xb_memvars_add(xb_memvar *memvars, uint32_t n);

/* Holds a name (`len` bytes, any letter case): its number, kept for it until xb_memvar_unhold, new when it had none.
 * xb_memvar_find_hold holds it only when it has one: 0 when not. A permanent name needs no hold (it costs nothing). */
uint32_t xb_memvar_hold(const char *name, size_t len);
int xb_memvar_find_hold(const char *name, size_t len, uint32_t *id);
void xb_memvar_unhold(uint32_t id);

/* A name's number for one operation of the context running (a macro's, MemVarBlock()'s, RESTORE's), from the names
 * the context keeps held (XB_NAME_CACHE of them, those it used last: a name used again and again costs no lock and no
 * atomic); with `number`, new when the name had none, else UINT32_MAX then. xb_memvar_done ends the operation (with
 * the `slot` given). */
uint32_t xb_memvar_use(xb_ctx *ctx, const char *name, size_t len, int number, uint32_t *slot);
void xb_memvar_done(xb_ctx *ctx, uint32_t id, uint32_t slot);

/* Whether `name` (any letter case) is a permanent name, and its number. */
int xb_memvar_permanent(const char *name, size_t len, uint32_t *id);

/* The name numbered `id` (held, or permanent; upper case, NUL-terminated), and its length in `*len`. */
const char *xb_memvar_name(uint32_t id, size_t *len);

/* The value of the variable visible by number `id` (held) into `out`: 0 with none (a reach that raised an error
 * leaves a request to unwind pending). */
int xb_memvar_value(xb_ctx *ctx, uint32_t id, xb_item *out);

/* Whether a variable of the name `name` (any letter case) is visible. */
int xb_memvar_exists_named(xb_ctx *ctx, const char *name, size_t len);

/* The value of the variable visible by the name `name` (any letter case) into
 * `out`, read without raising anything: 0 when there is none (or it designates
 * an array element). */
int xb_memvar_peek(xb_ctx *ctx, const char *name, size_t len, xb_item *out);

/* The variable visible by the name `name` (`len` bytes, any letter case)
 * takes `value`; with none, a new PRIVATE of the function calling the runtime
 * holds it (that function ends it: PRIVATES in rtl.ch). */
void xb_memvar_assign(xb_ctx *ctx, const char *name, size_t len, const xb_item *value);

/* The numbers of the variables visible (the environment's bindings, then the PUBLICs it does not hide), held, in
 * number order: `*count` of them, in memory the caller frees after letting each go (xb_memvar_unhold). */
uint32_t *xb_memvar_visible(xb_ctx *ctx, uint32_t *count);

/* Calls `fn` on each binding of the environment (roots of the collector: `fn` may change one only while every other
 * thread is stopped); xb_memvar_bindings_free lets them go (the environment ends). */
void xb_memvar_bindings_each(xb_env *e, void (*fn)(xb_item *it, void *data), void *data);
void xb_memvar_bindings_free(xb_env *e);

/* Shares the memory variables of the context's environments (the one running and those set aside): the bindings
 * visible and those PRIVATEs hid (publication, every other thread stopped). */
void xb_memvar_share(xb_ctx *ctx);

/* Shares the PUBLIC variables (publication, every other thread stopped). */
void xb_memvar_share_publics(void);

/* Calls fn on each item of the PUBLICs' table (roots of the collector): `fn` may change an item only while every other
 * thread is stopped (a collection, the publication); otherwise it reads the item as public_cell does. */
void xb_memvar_publics_each(void (*fn)(xb_item *it, void *data), void *data);

/* The bindings `ctx` sees in its environment (PRIVATEs, its own or its creator's), in `*r` (rooted): an array of pairs,
 * the name (its number when it is permanent, else its text: a snapshot holds no number) and the cell, or NIL for
 * none. xb_memvar_restore makes them visible in `ctx`'s environment, new (a snapshot's: xb_env_capture): the same
 * variables. */
void xb_memvar_capture(xb_ctx *ctx, xb_item *r);
void xb_memvar_restore(xb_ctx *ctx, const xb_item *pairs);
/* Makes `to` (new) see the bindings `from` sees: the same variables (their table shared until either changes it). */
void xb_memvar_inherit(xb_env *to, const xb_env *from);

/* CLEAR MEMORY: no PRIVATE of the environment, and no PUBLIC of the program, exists any more. */
void xb_memvar_clear(xb_ctx *ctx);

/* Whether `name` (upper case) matches the skeleton `skel`: `*` stands for any
 * characters, `?` for one, letters match in any case. */
int xb_memvar_like(const xb_str *skel, const char *name, size_t len);

#endif
