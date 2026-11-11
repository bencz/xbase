/* Work areas and database drivers (docs/rdd.md). A work area belongs to a task (xb_ctx.areas); its driver does what
 * a table does (raw navigation, records, fields), and the work area adds what the language asks on top: deleted and
 * filtered records skipped, BOF and EOF, the errors. */
#ifndef XB_RDD_H
#define XB_RDD_H

#include "xbrt_int.h"

#define XB_FIELD_NAME_SIZE 11 /* 10 characters and a NUL */
#define XB_AREA_MAX 65535      /* work areas are numbered 1 to this */
#define XB_DBCMD "DBCMD"       /* the subsystem of the database functions' errors */

typedef struct xb_field_info {
  char name[XB_FIELD_NAME_SIZE]; /* upper case */
  char type;                     /* C N D L M */
  uint32_t len;                  /* bytes in the record */
  uint32_t dec;
  uint32_t offset;               /* from the start of the record, past the deletion flag */
} xb_field_info;

typedef struct xb_area xb_area;
typedef struct xb_order xb_order;

/* Field writes: done, a value of the wrong type (nothing written), a number too wide (stars written), a file that
 * could not be written (a memo; the error's code given). */
enum { XB_PUT_OK, XB_PUT_TYPE, XB_PUT_WIDTH, XB_PUT_FAILED };

/* Why a table could not be opened: the FError() code, and the file that failed with the generic code and subcode of
 * its error (an Open error, or Corruption detected for a file the driver cannot read; `file` NULL: the table itself,
 * 1001; else a file of the table's, malloc'ed, as its memo file, 1002). */
typedef struct xb_open_failure {
  int32_t oscode;
  int32_t gencode;
  int32_t subcode;
  char *file;
} xb_open_failure;

/* A driver. The operations work on the raw table; those that fail give the FError() code of the failure. */
typedef struct xb_rdd {
  const char *name;
  /* Whether the driver can make a table of these fields (types, lengths, decimals, sizes), completing them as it
   * stores them (the length of a date, ...); the caller has checked the names. */
  int (*check)(xb_field_info *fields, uint32_t count);
  /* Creates the table (`count` fields, checked), overwriting a file of the name. */
  int (*create)(const char *path, const xb_field_info *fields, uint32_t count, int32_t *err);
  /* Opens the table into `a` (its state, fields and field count). */
  int (*open)(xb_area *a, const char *path, int readonly, xb_open_failure *why);
  /* Writes what is pending and frees the state. */
  int (*close)(xb_area *a, int32_t *err);
  uint32_t (*lastrec)(xb_area *a);
  /* Makes record `recno` current (outside 1 .. lastrec: the blank record after the last), its bytes unread. */
  int (*go)(xb_area *a, uint32_t recno, int32_t *err);
  /* Writes the current record when it changed; with `header`, the header too when the table changed (COMMIT). */
  int (*flush)(xb_area *a, int header, int32_t *err);
  /* Adds a blank record after the last and makes it current. */
  int (*append)(xb_area *a, int32_t *err);
  int (*get)(xb_area *a, uint32_t field, xb_item *r, int32_t *err);
  int (*put)(xb_area *a, uint32_t field, const xb_item *v, int32_t *err);
  /* The bytes of a field in the current record, as stored (`len` of them; after load). */
  const unsigned char *(*field_bytes)(xb_area *a, uint32_t field);
  /* Reads the current record, which navigation leaves unread until it is used: before deleted(), set_deleted() and
   * field_bytes() (get() and put() read it themselves). A record that cannot be read leaves the blank one. */
  int (*load)(xb_area *a, int32_t *err);
  int (*deleted)(xb_area *a);
  void (*set_deleted)(xb_area *a, int deleted);
  /* Removes the deleted records; removes every record. */
  int (*pack)(xb_area *a, int32_t *err);
  int (*zap)(xb_area *a, int32_t *err);
  uint32_t (*header_size)(xb_area *a);
  uint32_t (*record_size)(xb_area *a);
  int64_t (*last_update)(xb_area *a); /* a day number, 0 when the table says none */
} xb_rdd;

struct xb_area {
  uint32_t number;          /* in the task's table of areas, from 1 */
  char *alias;              /* upper case, malloc'ed */
  const xb_rdd *rdd;
  void *state;              /* the driver's */
  xb_field_info *fields;    /* the driver's */
  uint32_t nfields;
  uint32_t recno;
  int bof, eof, found;
  int readonly;             /* opened READONLY: writes are "Write not allowed" */
  uint32_t pins;            /* code that runs blocks over the area (xb_area_pin) */
  int closed;               /* closed while pinned: no driver state, freed by the last unpin */
  uint32_t generation;      /* a different number for each table opened: field caches compare it */
  xb_item filter;           /* SET FILTER: a codeblock or NIL (a root) */
  xb_item filter_text;      /* its text, "" without one (a root) */
  xb_item locate;           /* the FOR block of the last LOCATE, for CONTINUE (a root) */
  xb_order **orders;        /* the orders open (rdd/order.h), malloc'ed */
  uint32_t norders, order;  /* order: the controlling one, from 1; 0 for the record order */
  int hot;                  /* the current record changes: the orders keep its keys from before (xb_area_hot) */
  uint32_t hotRecno;
  xb_item pendingCond;      /* ordCondSet(): the FOR block and its text (roots), for the next order made */
  xb_item pendingCondText;
  int pendingDescend, pendingAdditive;
  uint32_t keying;          /* key or condition blocks running: the orders may not change meanwhile */
};

/* The drivers by name ("DBF", "DBFNTX"); NULL for an unknown one. The default (RddSetDefault()). */
const xb_rdd *xb_rdd_find(const char *name, size_t len);
const xb_rdd *xb_rdd_default(xb_ctx *ctx);

/* The work area numbered `n` (1-based), NULL when none is open there; the current one. */
xb_area *xb_area_at(xb_ctx *ctx, uint32_t n);
xb_area *xb_area_current(xb_ctx *ctx);
/* `n` as a work area's number, 0 when it is none (below 1 or past XB_AREA_MAX). */
uint32_t xb_area_number(int64_t n);
/* The area a name gives (as SELECT, Select() and alias-> read it): digits after blanks are its number (0 included;
 * what follows them is ignored), else, trailing blanks aside, one of the letters A to K is area 1 to 11, else an
 * alias (any letter case); -1 for none. */
int64_t xb_area_named(xb_ctx *ctx, const char *s, size_t len);
/* The lowest number with no table open. */
uint32_t xb_area_free(xb_ctx *ctx);
/* Opens a table in area `n` (closing what is open there) under an alias (trailing blanks dropped): a name that is not
 * one is DBCMD/1010, one another area has, or a letter that names an area (A to K, M), DBCMD/1011; the table is then
 * closed again. Returns 1, or 0 after the error (raised). */
int xb_area_open(xb_ctx *ctx, uint32_t n, const xb_rdd *rdd, const char *path, const char *alias, size_t aliasLen,
                 int readonly);
/* Writes what is pending and closes the table of area `n`, which is free before a close error is raised. */
void xb_area_close(xb_ctx *ctx, uint32_t n);
void xb_area_close_all(xb_ctx *ctx);

/* While code holding `a` runs a block that may close it: the area stays allocated (`closed` tells whether it was
 * closed meanwhile) until the matching unpin. */
void xb_area_pin(xb_area *a);
void xb_area_unpin(xb_area *a);

/* Whether the table can be written; "Write not allowed" (raised) when it was opened READONLY. */
int xb_area_writable(xb_ctx *ctx, xb_area *a);

/* The current record is about to change (`fresh`: it is a new one): its keys now, for the orders (rdd/order.c);
 * the change is done: entries move where keys changed. Before the driver writes the record. 0 after an error
 * (raised). */
int xb_area_hot(xb_ctx *ctx, xb_area *a, int fresh);
int xb_area_cold(xb_ctx *ctx, xb_area *a);
/* Writes the orders' pages; closes them all (the record order is controlling then). 0 after an error (raised). */
int xb_area_orders_flush(xb_ctx *ctx, xb_area *a);
int xb_area_orders_close(xb_ctx *ctx, xb_area *a);
/* SEEK in the controlling order: to the first record (`last`: the last) whose key starts with the value's, skipping
 * those that do not show; else, with `soft`, the first after where it would be, or the end. Found() tells. 0 after
 * an error (raised). */
int xb_area_seek(xb_ctx *ctx, xb_area *a, const xb_item *v, int soft, int last);
/* The controlling order, NULL for the record order. */
xb_order *xb_area_order(xb_area *a);

/* Navigation as the language sees it: deleted records skipped under SET DELETED, records the filter rejects skipped;
 * Found() false; 0 after an error (raised), or when the filter closed the area. */
int xb_area_goto(xb_ctx *ctx, xb_area *a, uint32_t recno);
int xb_area_top(xb_ctx *ctx, xb_area *a);
int xb_area_bottom(xb_ctx *ctx, xb_area *a);
int xb_area_skip(xb_ctx *ctx, xb_area *a, int64_t n);
/* Whether the current record shows (SET DELETED, the filter); -1 after an error in the filter, or when it closed the
 * area (pinned by the caller). */
int xb_area_visible(xb_ctx *ctx, xb_area *a);

/* Reads the current record's bytes when navigation left them unread: 0 after the error (DBFNTX/1010, raised). */
int xb_area_load(xb_ctx *ctx, xb_area *a);
/* Reading a field (a memo that cannot be read: DBFNTX/1010, NIL); writing one, with the errors raised (READONLY, a
 * wrong type, a number too wide, a memo that cannot be written). */
int xb_area_field_get(xb_ctx *ctx, xb_area *a, uint32_t field, xb_item *r);
int xb_area_field_put(xb_ctx *ctx, xb_area *a, uint32_t field, const xb_item *v);
/* The 0-based position of a field (any letter case), or -1. */
int64_t xb_area_field_index(const xb_area *a, const char *name, size_t len);

/* The error of a failed driver operation (`gencode`, `subcode`, the file's FError() code). */
void xb_area_error(xb_ctx *ctx, const xb_area *a, int32_t gencode, int32_t subcode, const char *operation,
                   int32_t oscode);
/* "Workarea not in use" (DBCMD/2001) of the function `fn`. */
void xb_area_unused_error(xb_ctx *ctx, const char *fn);

/* Makes the work area an item names current (an alias as xb_area_named reads it, or a number from 1; for alias->(
 * expr )): 1, with the area current before in `before`; 0 after the error (BASE/1002, raised; the error block did not
 * ask to try again). */
int xb_area_enter(xb_ctx *ctx, const xb_item *area, uint32_t *before);

/* The database functions' arguments (rdd/dbcmd.c): argument `k` as a string (NULL otherwise), a number (`dflt`
 * otherwise), whether it is .T.; the current area, or NULL after "Workarea not in use" for the function `fn`. */
const xb_str *xb_db_str_arg(uint32_t argc, const xb_item *argv, uint32_t k);
int64_t xb_db_num_arg(uint32_t argc, const xb_item *argv, uint32_t k, int64_t dflt);
int xb_db_logical_arg(uint32_t argc, const xb_item *argv, uint32_t k);
xb_area *xb_db_used(xb_ctx *ctx, const char *fn);

/* Marks the work areas' items for the collector. */
void xb_areas_mark(xb_ctx *ctx, int (*mark)(const xb_item *));

/* The DBF driver (rdd/dbf.c), and the names it serves. */
extern const xb_rdd xb_rdd_dbf;

#endif
