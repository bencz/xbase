/* The rules of compiled classes (xb_class_def in xbrt.h) that the runtime applies when a message is sent, and that
 * the compiler applies when it resolves a send whose receiver's class it knows: one implementation (types/members.c),
 * compiled into both. Not part of the ABI: nothing here is a layout compiled code reads.
 *
 * A class's line is the class, then each parent's line in order, each class once. The members a class answers are
 * those of its line in table order: for each class of the line, its methods, then each variable read ("NAME") and
 * assigned ("_NAME"), then each class variable likewise; a message is the first member of that order that answers
 * it. */
#ifndef XBCLASS_H
#define XBCLASS_H

#include "xbrt.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum xb_member_kind { XB_MK_GET, XB_MK_SET, XB_MK_CLASS_GET, XB_MK_CLASS_SET, XB_MK_METHOD } xb_member_kind;

/* What a message reaches: a variable of `owner` (its `index` among the owner's variables or class variables) read or
 * assigned, or a method of `owner` (its `index` among the owner's methods, and its function). `flags`: the scope and
 * XB_MEMBER_READONLY. */
typedef struct xb_member {
  xb_member_kind kind;
  uint32_t flags;
  const xb_class_def *owner;
  uint32_t index;
  xb_func fn;
} xb_member;

/* Calls `visit` with each member of the line in table order, with its message as `name` and `setter` (1: the message
 * is "_" followed by `name`), until it returns nonzero. */
typedef int (*xb_member_visit)(void *data, const char *name, int setter, const xb_member *m);
void xb_line_members(const xb_class_def *const *line, uint32_t n, xb_member_visit visit, void *data);

/* The member of the line that answers `message`: 1 and *out, or 0 when none does. */
int xb_line_lookup(const xb_class_def *const *line, uint32_t n, const char *message, xb_member *out);

/* Appends to the line (`*n` classes, with room for `pn` more) the classes of a parent's line it does not hold yet. */
void xb_line_merge(const xb_class_def **line, uint32_t *n, const xb_class_def *const *parentLine, uint32_t pn);

/* What a message sent from a method of class `f` reaches in an object of a class of `f`'s kind, given what `f`
 * answers (`mf`) and what the object's class answers (`mc`), either NULL: `f`'s own HIDDEN member, else a method of the
 * object's class (an override, an ACCESS or ASSIGN over an inherited variable), else a variable `f` sees, else what the
 * object's class answers. */
const xb_member *xb_member_from(const xb_member *mf, const xb_member *mc, const xb_class_def *f);

/* Super:message from a method of `f` (whose line is `fline`): the member the first ancestor after `f` answers with a
 * member of its own; `lookup` gives what a class answers (in its own line). NULL when none does. */
typedef const xb_member *(*xb_class_lookup)(void *data, const xb_class_def *c, const char *message);
const xb_member *xb_member_super(const xb_class_def *const *fline, uint32_t fn, const char *message,
                                 xb_class_lookup lookup, void *data);

/* What a message sent to an object of class `c` reaches, from a method of class `f` (NULL: elsewhere) whose line is
 * `fline`, as Super:message with `super`, when the object is of `f`'s kind (`related`; the caller rejects Super:message
 * otherwise): the member Super reaches (xb_member_super), else what a message from `f` reaches (xb_member_from), else
 * what `c` answers; `lookup` gives what a class answers. NULL when nothing does: then the message names a class of the
 * line (a cast, xb_line_named), or New or Init the runtime answers, or it is not found. */
const xb_member *xb_member_reach(const xb_class_def *c, const xb_class_def *f, const xb_class_def *const *fline,
                                 uint32_t fn, int super, int related, const char *message, xb_class_lookup lookup,
                                 void *data);

/* The position in the line of the class named `message` (upper case), the line's length when none is: a message no
 * member answers that names one is a cast. */
uint32_t xb_line_named(const xb_class_def *const *line, uint32_t n, const char *message);

/* The first slot of the variables of the class at position `k` of the line, in an object of the line's class: each
 * class of the line has an area of its own, in the line's order. */
uint32_t xb_line_area(const xb_class_def *const *line, uint32_t k);

/* The scope a message reaching member `m` is checked against: its own, one narrower to assign a READONLY variable
 * (EXPORTED as PROTECTED, PROTECTED as HIDDEN). */
uint32_t xb_member_scope(const xb_member *m);

/* Whether code running in a method of `from` (NULL: elsewhere) reaches member `m` by its scope alone, `inside` telling
 * whether `from` is the member's class or a subclass of it: HIDDEN members answer their class's own methods,
 * PROTECTED ones the methods of their class and its subclasses. When it does not, only a friend of the member's class
 * reaches it (the runtime's check). */
int xb_member_scope_ok(const xb_member *m, const xb_class_def *from, int inside);

#ifdef __cplusplus
}
#endif

#endif
