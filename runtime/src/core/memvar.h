/* Memory variables as the runtime's own functions see them (core/memvar.c);
 * compiled code uses the xb_mv_* entry points of xbrt.h. */
#ifndef XB_MEMVAR_H
#define XB_MEMVAR_H

#include "core/context.h"

/* Numbers the memory variables of a registering module. */
void xb_memvars_add(xb_memvar *memvars, uint32_t n);

/* The names numbered so far: 0 .. xb_memvar_count() - 1. */
uint32_t xb_memvar_count(void);

/* The name numbered `id` (upper case), and its length in `*len`. */
const char *xb_memvar_name(uint32_t id, size_t *len);

/* The value of the variable visible by number `id`: NULL with none (or when
 * reaching it raised an error, which leaves a request to unwind pending). */
const xb_item *xb_memvar_value(xb_ctx *ctx, uint32_t id);

/* The number of the name `name` (`len` bytes, any letter case), new when no
 * module uses it. */
uint32_t xb_memvar_number(const char *name, size_t len);

/* Whether a variable numbered `id` is visible. */
int xb_memvar_exists(xb_ctx *ctx, uint32_t id);

/* The value of the variable visible by the name `name` (any letter case), read
 * without raising anything: NULL when there is none (or it designates an
 * array element). */
const xb_item *xb_memvar_peek(xb_ctx *ctx, const char *name, size_t len);

/* The variable visible by the name `name` (`len` bytes, any letter case)
 * takes `value`; with none, a new PRIVATE of the function calling the runtime
 * holds it (that function ends it: PRIVATES in rtl.ch). */
void xb_memvar_assign(xb_ctx *ctx, const char *name, size_t len, const xb_item *value);

/* CLEAR MEMORY: no PRIVATE or PUBLIC exists any more. */
void xb_memvar_clear(xb_ctx *ctx);

/* Whether `name` (upper case) matches the skeleton `skel`: `*` stands for any
 * characters, `?` for one, letters match in any case. */
int xb_memvar_like(const xb_str *skel, const char *name, size_t len);

#endif
