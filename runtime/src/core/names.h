/* Tables of names: each name added gets a number, in the order added. */
#ifndef XB_NAMES_H
#define XB_NAMES_H

#include <stddef.h>
#include <stdint.h>

typedef struct xb_name_slot xb_name_slot;
typedef struct xb_names {
  xb_name_slot *slots;      /* an open-addressing hash: linear probing, at most half full */
  size_t capacity;          /* a power of two */
  uint32_t count;           /* the names added: numbered 0 .. count - 1 */
} xb_names;

/* The number of `name` (`len` bytes, kept by reference: it must outlive the
 * table), a new one when it was not there. */
uint32_t xb_names_add(xb_names *t, const char *name, size_t len);

/* Whether `name` is there, and its number in `*id`. */
int xb_names_find(const xb_names *t, const char *name, size_t len, uint32_t *id);

/* The same for `name` in any letter case, in a table of upper-cased names. */
int xb_names_find_upper(const xb_names *t, const char *name, size_t len, uint32_t *id);

#endif
