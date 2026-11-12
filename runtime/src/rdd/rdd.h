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
  uint32_t offset;               /* from the start of the record, whose first byte is the deletion flag */
} xb_field_info;

typedef struct xb_area xb_area;
typedef struct xb_order xb_order;

/* A relation (SET RELATION) from a parent area into a child: when the parent's record changes, the child goes where
 * the expression, evaluated in the parent, says (a SEEK of its value in the child's controlling order, within a scope
 * of that key when `scoped`; without an order, the record of that number). The block and its text are roots. */
typedef struct xb_relation {
  uint64_t id;    /* among the parent's relations, never reused */
  uint32_t child;
  xb_item block, text;
  int scoped;
} xb_relation;

/* Clipper's file protocol of shared tables and NTX indexes (docs/rdd.md): record n's lock is the byte XB_DB_LOCK + n,
 * the file lock the XB_DB_LOCK bytes after XB_DB_LOCK, and XB_DB_LOCK itself a table's append lock and an index's
 * read and write lock. FoxPro's (DBFCDX) places a table's locks from XB_FOX_LOCK by the records' offsets; while the
 * table's production order file is open, below XB_FOX_HIGH_LOCK: record n's is the byte XB_FOX_HIGH_LOCK - n, the
 * file lock the XB_FOX_HIGH_FILE bytes before XB_FOX_HIGH_LOCK, which is the append lock (and a CDX file's lock). */
#define XB_DB_LOCK 1000000000u
#define XB_FOX_LOCK 0x40000000u
#define XB_FOX_HIGH_LOCK 0x7FFFFFFEu
#define XB_FOX_HIGH_FILE 0x07FFFFFFu
enum { XB_LOCKS_CLIPPER, XB_LOCKS_FOXPRO };

/* ordCondSet()'s settings for the next order made in an area: the condition (block and text), WHILE, the EVAL block
 * and its interval (roots), the record to start at, NEXT, RECORD (0: none), REST, DESCENDING, ADDITIVE and
 * USECURRENT. */
typedef struct xb_order_conditions {
  xb_item cond, condText, whileBlock, evalBlock;
  int64_t every, start, next, record;
  int rest, descend, additive, current;
} xb_order_conditions;

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
  const char *ext;                   /* its files' extension (dbTableExt()), added to a name without one */
  int text;                          /* a text file's (SDF, DELIM): it keeps no structure, records are lines */
  int delimited;                     /* DELIM's: fields separated, character ones delimited */
  const struct xb_memo_format *memo; /* the memos of the tables it creates (rdd/memo.h) */
  const struct xb_index_format *index; /* their format (rdd/index.h); NULL: not yet there */
  int lockScheme;                    /* XB_LOCKS_CLIPPER, XB_LOCKS_FOXPRO */
  /* Whether the driver can make a table of these fields (types, lengths, decimals, sizes), completing them as it
   * stores them (the length of a date, ...); the caller has checked the names. */
  int (*check)(const struct xb_rdd *rdd, xb_field_info *fields, uint32_t count);
  /* Creates the table (`count` fields, checked), overwriting a file of the name. */
  int (*create)(const struct xb_rdd *rdd, const char *path, const xb_field_info *fields, uint32_t count,
                int32_t *err);
  /* Opens the table into `a` (its state, fields and field count), `exclusive` or shared with other users (a sharing
   * they do not allow: 32), with its memo file. */
  int (*open)(xb_area *a, const char *path, int readonly, int exclusive, xb_open_failure *why);
  /* A text driver's open: the file with the fields given (offsets set here), `create`d to be written in record
   * order, or read; `delimiter` DELIM's delimiter and separator (NIL or NULL: the defaults). */
  int (*open_with)(xb_area *a, const char *path, const xb_field_info *fields, uint32_t count, int create,
                   const xb_item *delimiter, xb_open_failure *why);
  /* Writes what is pending and frees the state. */
  int (*close)(xb_area *a, int32_t *err);
  uint32_t (*lastrec)(xb_area *a);
  /* Makes record `recno` current (outside 1 .. lastrec: the blank record after the last), its bytes unread. */
  int (*go)(xb_area *a, uint32_t recno, int32_t *err);
  /* Writes the current record when it changed; with `header`, the header too when the table changed (COMMIT). */
  int (*flush)(xb_area *a, int header, int32_t *err);
  /* Adds a blank record after the last and makes it current (its number in `recno`); shared, it is written at once
   * and locked (unless the file is). */
  int (*append)(xb_area *a, uint32_t *recno, int32_t *err);
  /* Takes (`on`) or releases record `recno`'s lock, 0 the file's, not waiting: held by another user, 0 with 33. An
   * exclusive table's locks cost nothing (always 1). */
  int (*lock)(xb_area *a, uint32_t recno, int on, int32_t *err);
  int (*get)(xb_area *a, uint32_t field, xb_item *r, int32_t *err);
  int (*put)(xb_area *a, uint32_t field, const xb_item *v, int32_t *err);
  /* The bytes of a field in the current record, as stored (`len` of them; after load). */
  const unsigned char *(*field_bytes)(xb_area *a, uint32_t field);
  /* The fields' bytes of the current record, side by side past the deleted mark (record_size() - 1 of them, a field's
   * at its offset - 1; a text file's memos take none; after load), and all of them replaced at once: a record copied
   * between two tables of the same fields. */
  const unsigned char *(*record_bytes)(xb_area *a);
  int (*put_record_bytes)(xb_area *a, const unsigned char *bytes, int32_t *err);
  /* Reads the current record, which navigation leaves unread until it is used: before deleted(), set_deleted() and
   * field_bytes() (get() and put() read it themselves). A record that cannot be read leaves the blank one. */
  int (*load)(xb_area *a, int32_t *err);
  /* Shared: the current record is read again at its next use (another program may have changed it), unless it has
   * changes not yet written. */
  void (*refresh)(xb_area *a);
  int (*deleted)(xb_area *a);
  void (*set_deleted)(xb_area *a, int deleted);
  /* Removes the deleted records; removes every record. */
  int (*pack)(xb_area *a, int32_t *err);
  int (*zap)(xb_area *a, int32_t *err);
  uint32_t (*header_size)(xb_area *a);
  uint32_t (*record_size)(xb_area *a);
  int64_t (*last_update)(xb_area *a); /* a day number, 0 when the table says none */
  /* Marks in the table's header whether it has a production order file (FoxPro's flag), written at once. */
  int (*set_production)(xb_area *a, int on, int32_t *err);
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
  int shared;               /* opened shared: writes need the record's or the file's lock */
  uint32_t *locks;          /* shared: the records this area locked, in the order it locked them (malloc'ed) */
  uint32_t nlocks, lockCap;
  int flocked;              /* shared: the file lock is held */
  uint32_t pins;            /* code that runs blocks over the area (xb_area_pin) */
  int closed;               /* closed while pinned: no driver state, freed by the last unpin */
  uint64_t generation;      /* a different number for each table opened (from 1): field caches compare it */
  xb_item filter;           /* SET FILTER: a codeblock or NIL (a root) */
  xb_item filter_text;      /* its text, "" without one (a root) */
  xb_item locate;           /* the FOR block of the last LOCATE, for CONTINUE (a root) */
  xb_order **orders;        /* the orders open (rdd/order.h), malloc'ed */
  uint32_t norders, order;  /* order: the controlling one, from 1; 0 for the record order */
  int hot;                  /* the current record changes: the orders keep its keys from before (xb_area_hot) */
  uint32_t hotRecno;
  xb_order_conditions pending; /* ordCondSet()'s, for the next order made */
  uint32_t keying;          /* key or condition blocks running: the orders may not change meanwhile */
  char *production;         /* a tagged order format's: the table's production order file, named as the table and
                               opened with it (malloc'ed); NULL for none */
  xb_relation **relations;  /* the relations from this area, in the order they were set (each malloc'ed) */
  uint32_t nrelations;
  uint32_t parents;         /* the relations into this area */
  uint64_t relationIds;     /* the last relation's id */
  const xb_relation *relPending; /* a parent's record changed: the move the area makes before its next use */
  uint32_t relParent;       /* that parent's number */
  int settling;             /* that move is being made: the expression's own moves make no other */
  int closing;              /* xb_area_close runs: no relation is added from or into the area */
  int snapshot;             /* a copy reads the table in order (__dbTrans): shared, its records are read ahead and
                               counted once, as under the file lock */
};

/* The drivers by name ("DBF", "DBFNTX"); NULL for an unknown one. The default (RddSetDefault()). */
const xb_rdd *xb_rdd_find(const char *name, size_t len);
const xb_rdd *xb_rdd_default(xb_ctx *ctx);

/* The work area numbered `n` (1-based), NULL when none is open there; the current one. */
xb_area *xb_area_at(xb_ctx *ctx, uint32_t n);
xb_area *xb_area_current(xb_ctx *ctx);
/* The same with its pending relational move made (xb_area_settle; an error raised meanwhile stays pending): for what
 * reads or changes the record (its position, fields, deleted mark, locks; the filter, scope walks, order builds). */
xb_area *xb_area_settled(xb_ctx *ctx, uint32_t n);
xb_area *xb_area_record(xb_ctx *ctx);
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
                 int readonly, int exclusive);
/* A text file's opening (xb_area_open_with): its fields, `create`d to be written or read, DELIM's delimiter. */
typedef struct xb_text_open {
  const xb_field_info *fields;
  uint32_t count;
  int create;
  const xb_item *delimiter;
} xb_text_open;
/* Opens a table (`text` NULL) or a text file in area `n` without an alias, for the copies between files (COPY TO,
 * APPEND FROM): as xb_area_open otherwise; a text file that cannot be created is a Create error (1004). */
int xb_area_open_with(xb_ctx *ctx, uint32_t n, const xb_rdd *rdd, const char *path, int readonly, int exclusive,
                      const xb_text_open *text);
/* Writes what is pending and closes the table of area `n`, which is free before a close error is raised. */
void xb_area_close(xb_ctx *ctx, uint32_t n);
void xb_area_close_all(xb_ctx *ctx);

/* While code holding `a` runs a block that may close it: the area stays allocated (`closed` tells whether it was
 * closed meanwhile) until the matching unpin. */
void xb_area_pin(xb_area *a);
void xb_area_unpin(xb_area *a);

/* Whether the table can be written; "Write not allowed" (raised) when it was opened READONLY. Whether the current
 * record can be changed: shared, it needs its lock or the file's ("Lock required", DBFNTX/1022). Whether the table
 * is exclusive, as PACK, ZAP and REINDEX need ("Exclusive required", DBFNTX/1023). */
int xb_area_writable(xb_ctx *ctx, xb_area *a);
int xb_area_record_writable(xb_ctx *ctx, xb_area *a);
int xb_area_exclusive(xb_ctx *ctx, xb_area *a);

/* Locks of a shared table (docs/rdd.md; an exclusive one's cost nothing and are always had): record `recno`'s (0: the
 * file's, which releases the record locks first); `add` keeps the other record locks (dbRLock()), else they go
 * (RLock()). With the file lock held every lock is had. 1 when had, 0 when another user holds it (or after an
 * error, raised). A change of the current record is written before any lock goes. */
int xb_area_lock(xb_ctx *ctx, xb_area *a, uint32_t recno, int add);
/* Releases record `recno`'s lock, or with 0 every lock of the area; its record locks. */
int xb_area_unlock(xb_ctx *ctx, xb_area *a, uint32_t recno);
int xb_area_unlock_records(xb_ctx *ctx, xb_area *a);
/* APPEND BLANK: a new blank record, current; shared, the area's record locks go (unless not `release`) and the new
 * record is locked (when that cannot be: DBFNTX/1024, which NetErr() reports under the default error handler). */
int xb_area_append(xb_ctx *ctx, xb_area *a, int release);

/* The current record is about to change (`fresh`: it is a new one): its keys now, for the orders (rdd/order.c);
 * the change is done: entries move where keys changed. Before the driver writes the record. 0 after an error
 * (raised). */
int xb_area_hot(xb_ctx *ctx, xb_area *a, int fresh);
int xb_area_cold(xb_ctx *ctx, xb_area *a);
/* Writes the orders' pages; closes them all (the record order is controlling then). 0 after an error (raised). */
int xb_area_orders_flush(xb_ctx *ctx, xb_area *a);
int xb_area_orders_close(xb_ctx *ctx, xb_area *a);
/* Whether the table's production order file is open. */
int xb_area_production_open(const xb_area *a);
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
/* Goes to record `recno` (0: the blank record after the last), then to the first that shows from it going `dir` (1
 * forward, -1 back: BOF on the first record when none before it shows). 0 after an error (raised). */
int xb_area_arrive(xb_ctx *ctx, xb_area *a, uint32_t recno, int dir);
/* Raw navigation in the controlling order (the record order without one; the scope applies), all records showing:
 * to its first (`dir` 1) or last (-1) record, or one step (none from a record the order has no entry for). 1 on a
 * record, 0 when there is none (the blank record after the last), -1 after an error (raised); the caller pins the
 * area. */
int xb_area_edge_raw(xb_ctx *ctx, xb_area *a, int dir);
int xb_area_step_raw(xb_ctx *ctx, xb_area *a, int dir);
/* Whether the current record shows (SET DELETED, the filter); -1 after an error in the filter, or when it closed the
 * area (pinned by the caller). */
int xb_area_visible(xb_ctx *ctx, xb_area *a);

/* Reads the current record's bytes when navigation left them unread: 0 after the error (DBFNTX/1010, raised). */
int xb_area_load(xb_ctx *ctx, xb_area *a);
/* Reading a field (a memo that cannot be read: DBFNTX/1010, NIL); writing one, with the errors raised (READONLY, a
 * wrong type, a number too wide, a memo that cannot be written). */
int xb_area_field_get(xb_ctx *ctx, xb_area *a, uint32_t field, xb_item *r);
int xb_area_field_put(xb_ctx *ctx, xb_area *a, uint32_t field, const xb_item *v);
/* A field's value from its bytes as a table stores them, and into them (XB_PUT_OK, XB_PUT_TYPE: nothing written,
 * XB_PUT_WIDTH: stars written): C, N, D (YYYYMMDD), L; not memos (rdd/field.c). */
void xb_field_decode(const xb_field_info *fi, const char *p, xb_item *r);
int xb_field_encode(const xb_field_info *fi, char *p, const xb_item *v);
/* The 0-based position of a field (any letter case), or -1. */
int64_t xb_area_field_index(const xb_area *a, const char *name, size_t len);

/* Relations (rdd/relation.c). The parent's record changed: its children, and theirs, make their move at their next
 * use. Makes the area's pending move: 0 after an error (raised). Makes it when the relation is scoped (its scope is
 * about to be read or bound a move). A move that does not start from the current record (GO, GO TOP, GO BOTTOM,
 * SEEK, APPEND) drops the pending one, after making a scoped one. */
void xb_area_sync_children(xb_ctx *ctx, xb_area *a);
int xb_area_settle(xb_ctx *ctx, xb_area *a);
int xb_area_settle_scoped(xb_ctx *ctx, xb_area *a);
int xb_area_drop_pending(xb_ctx *ctx, xb_area *a);
/* Adds a relation from `a` into `child`, whose move is then pending: refused (0) when `child` is `a` or leads back
 * to it through its own relations, or when either area is closing. */
int xb_area_relation_add(xb_ctx *ctx, xb_area *a, xb_area *child, const xb_item *block, const xb_item *text,
                         int scoped);
/* Removes the area's relations: a child whose move one left pending makes it, a scoped one's scope goes. The relations
 * of other areas into `a` (it closes). */
void xb_area_relations_clear(xb_ctx *ctx, xb_area *a);
void xb_area_relations_detach(xb_ctx *ctx, xb_area *a);

/* The error of a failed driver operation (`gencode`, `subcode`, the file's FError() code). */
void xb_area_error(xb_ctx *ctx, const xb_area *a, int32_t gencode, int32_t subcode, const char *operation,
                   int32_t oscode);
/* "Workarea not in use" (DBCMD/2001) of the function `fn`. */
void xb_area_unused_error(xb_ctx *ctx, const char *fn);

/* The number of the work area an item names (an alias as xb_area_named reads it, or a number from 1; open or not):
 * 1, or 0 after the error for a name of no area (BASE/1002, raised; the error block did not ask to try again). */
int xb_area_lookup(xb_ctx *ctx, const xb_item *area, uint32_t *n);
/* Evaluates `block` with area `a` current: 1 with its value in `r` (rooted by the caller), 0 after an error. The area
 * current before comes back unless the block made another one current. */
int xb_area_eval(xb_ctx *ctx, xb_area *a, const xb_item *block, xb_item *r);

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
/* The driver argument `k` names (none or "": the default), NULL for a name of none. */
const xb_rdd *xb_db_driver_arg(xb_ctx *ctx, uint32_t argc, const xb_item *argv, uint32_t k);
/* A structure as dbCreate() takes it ({ cName, cType, nLength, [nDecimals] } each, names unique): its fields
 * (malloc'ed, not checked by a driver), or 0 when it is none. Creates the file with the driver, the Create error
 * (1004) raised and retried as the error block asks: 1, or 0. */
int xb_db_struct(const xb_item *spec, xb_field_info **fields, uint32_t *count);
int xb_db_create(xb_ctx *ctx, const xb_rdd *rdd, const char *path, const xb_field_info *fields, uint32_t count);
/* The scope of a command (FOR, WHILE, NEXT, RECORD, REST: the arguments from `k`): `visit` for each record of it
 * that the FOR block takes, until it returns 0 (stop) or -1 (error). The scope starts at the top unless WHILE, NEXT,
 * RECORD or REST says otherwise; NEXT n ends on its last record (none for n below 1). It is 0 after an error, or when
 * a block closed the area (`a` pinned by the caller). */
int xb_db_scope(xb_ctx *ctx, xb_area *a, uint32_t argc, const xb_item *argv, uint32_t k,
                int (*visit)(xb_ctx *, xb_area *, void *), void *arg);
/* xb_db_used, with the area's pending relational move made (xb_area_record). */
xb_area *xb_db_record(xb_ctx *ctx, const char *fn);

/* No conditions: every record, ascending, the other orders closed. */
void xb_order_conditions_clear(xb_order_conditions *c);

/* Marks the work areas' items for the collector. */
void xb_areas_mark(xb_ctx *ctx, int (*mark)(xb_item *));

/* The structures a DBF table can have (completing the lengths of dates, logicals and memos), which text files share:
 * the DBF driver's check. */
int xb_dbf_check(const xb_rdd *rdd, xb_field_info *fields, uint32_t count);
/* Whether a byte is a true logical's in a table or a text file: T, t, Y or y. */
static inline int xb_field_true(char c) { return c == 'T' || c == 't' || c == 'Y' || c == 'y'; }
/* Whether `n` bytes are a number's text with `dec` decimals: an optional minus, digits, and with decimals a dot and
 * that many digits (what a number field stores, its leading blanks aside). */
int xb_field_number_text(const char *s, size_t n, uint32_t dec);

/* The DBF driver (rdd/dbf.c), and the names it serves. */
extern const xb_rdd xb_rdd_dbf;    /* DBFNTX: DBT memos, NTX orders */
extern const xb_rdd xb_rdd_dbfcdx; /* DBFCDX: FPT memos, CDX orders */
extern const xb_rdd xb_rdd_sdf;    /* SDF and DELIM text files (rdd/text.c) */
extern const xb_rdd xb_rdd_delim;

#endif
