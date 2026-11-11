#include "xbrt_int.h"

#include <stdio.h>
#include <string.h>

/* The Error class: what describes a runtime error, in the order of an Error
 * object's items (ERR_* below). */
static const xb_class_var error_vars[] = {
  {"ARGS", XB_VAR_ARRAY},           {"CANDEFAULT", XB_VAR_LOGICAL}, {"CANRETRY", XB_VAR_LOGICAL},
  {"CANSUBSTITUTE", XB_VAR_LOGICAL}, {"CARGO", XB_VAR_ANY},         {"DESCRIPTION", XB_VAR_STRING},
  {"FILENAME", XB_VAR_STRING},      {"GENCODE", XB_VAR_INT32},      {"OPERATION", XB_VAR_STRING},
  {"OSCODE", XB_VAR_INT32},         {"SEVERITY", XB_VAR_UINT16},    {"SUBCODE", XB_VAR_INT32},
  {"SUBSYSTEM", XB_VAR_STRING},     {"TRIES", XB_VAR_UINT16},
};

enum {
  ERR_ARGS, ERR_CANDEFAULT, ERR_CANRETRY, ERR_CANSUBSTITUTE, ERR_CARGO, ERR_DESCRIPTION, ERR_FILENAME, ERR_GENCODE,
  ERR_OPERATION, ERR_OSCODE, ERR_SEVERITY, ERR_SUBCODE, ERR_SUBSYSTEM, ERR_TRIES, ERR_COUNT_
};
_Static_assert(sizeof error_vars / sizeof error_vars[0] == ERR_COUNT_, "one ERR_ index per Error variable");

static const xb_class error_class = {"ERROR", ERR_COUNT_, error_vars, NULL};

void xbf_ERRORNEW(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  xb_set_object(ret, xb_object_new(&error_class));
}

/* The description of each generic code the runtime raises (Clipper's texts),
 * and of particular subcodes. */
static const struct {
  int32_t gencode, subcode; /* subcode 0: any */
  const char *text;
} descriptions[] = {
  {EG_NOMETHOD, 41, "Scope violation (hidden)"}, {EG_NOMETHOD, 42, "Scope violation (protected)"},
  {EG_ARG, 0, "Argument error"},       {EG_BOUND, 0, "Bound error"},            {EG_STROVERFLOW, 0, "String overflow"},
  {EG_ZERODIV, 0, "Zero divisor"},     {EG_NOFUNC, 0, "Undefined function"},    {EG_NOMETHOD, 0, "No exported method"},
  {EG_NOVARMETHOD, 0, "No exported variable"}, {EG_NOVAR, 0, "Variable does not exist"},
  {EG_CREATE, 0, "Create error"},      {EG_OPEN, 0, "Open error"},              {EG_WRITE, 0, "Write error"},
  {EG_SYNTAX, 0, "Syntax error"},
};

static const char *description_of(int32_t gencode, int32_t subcode) {
  for (size_t i = 0; i < sizeof descriptions / sizeof descriptions[0]; ++i)
    if (descriptions[i].gencode == gencode && (descriptions[i].subcode == 0 || descriptions[i].subcode == subcode))
      return descriptions[i].text;
  return "";
}

static void set_text(xb_item *it, const char *s) { xb_set_str(it, xb_str_new(s, strlen(s))); }

/* An error the program cannot recover from: reported, and the program ends
 * with exit code 1. */
static void unrecoverable(xb_ctx *ctx, int code, const char *text) {
  fflush(stdout);
  fprintf(stderr, "\nUnrecoverable error %d: %s", code, text);
  for (const xb_frame *f = ctx->frames; f; f = f->prev) {
    if (!xb_frame_shown(f)) continue;
    fprintf(stderr, "\nCalled from %s(%u)", f->proc->name, (unsigned)f->line);
    if (f->proc->file) fprintf(stderr, " in %s", f->proc->file);
  }
  fputc('\n', stderr);
  ctx->exitcode = 1;
  ctx->pending |= XB_PENDING_QUIT;
}

int xb_error(xb_ctx *ctx, xb_item *result, int32_t gencode, int32_t subcode, const char *operation, uint32_t flags,
             uint32_t argc, const xb_item *args) {
  return xb_error_in(ctx, NULL, result, gencode, subcode, operation, flags, argc, args);
}

/* An error to raise: what the Error object says, and what the block may do. */
typedef struct failure {
  const char *frame;        /* the runtime function the call stack shows while the block runs, or NULL */
  const char *description;  /* NULL: the generic code's */
  int32_t gencode, subcode;
  const char *operation;
  const char *filename;     /* NULL: none */
  int32_t oscode;
  uint32_t flags;           /* XB_ERR_* */
  uint32_t argc;
  const xb_item *args;
  uint16_t tries;           /* the attempt, from 1, of an error that may be retried */
} failure;

/* Raises the error (see xb_error_in). 0: fail; 1: substituted, or the
 * default action; 2: retry. */
static int raise(xb_ctx *ctx, const failure *f, xb_item *result) {
  const char *frame = f->frame;
  uint32_t flags = f->flags, argc = f->argc;
  const xb_item *args = f->args;
  if (ctx->error_trap) { /* Type() evaluating: it ends here */
    ctx->error_trapped = 1;
    ctx->pending |= XB_PENDING_TRAP;
    return 0;
  }
  xb_proc proc = XB_RUNTIME_PROC(frame, 0);
  xb_frame inside;
  if (frame) xb_frame_push(ctx, &inside, &proc);
  if (ctx->error_block.type != XB_T_BLOCK || ctx->error_depth >= XB_ERROR_MAX_DEPTH) {
    if (ctx->error_block.type != XB_T_BLOCK) unrecoverable(ctx, 9002, "No ERRORBLOCK() for error");
    else unrecoverable(ctx, 9003, "Too many recursive error handler calls");
    if (frame) xb_frame_pop(ctx, &inside);
    return 0;
  }
  /* The arguments may be the only references to their values (a substituted
   * result the caller holds in C): rooted until they are in the object. */
  for (uint32_t i = 0; i < argc; ++i) xb_gc_root_push(ctx, (xb_item *)&args[i]);
  /* The block runs from a rooted copy: it may replace the context's error
   * block (ErrorBlock()) while it runs. */
  xb_item call[3]; /* the Error object, the block's result, the block */
  for (int k = 0; k < 3; ++k) {
    xb_set_nil(&call[k]);
    xb_gc_root_push(ctx, &call[k]);
  }
  xb_object *e = xb_object_new(&error_class);
  xb_set_object(&call[0], e);
  /* Each allocation below may collect: the object is rooted, and every store
   * into it is followed by the barrier. */
  if (argc) { /* without arguments, `args` stays NIL, as in the reference */
    xb_array *a = xb_array_alloc(argc);
    for (uint32_t i = 0; i < argc; ++i) a->items[i] = args[i];
    xb_set_array(&e->vars[ERR_ARGS], a);
    xb_gc_write(e);
  }
  set_text(&e->vars[ERR_DESCRIPTION], f->description ? f->description : description_of(f->gencode, f->subcode));
  xb_gc_write(e);
  set_text(&e->vars[ERR_OPERATION], f->operation);
  xb_gc_write(e);
  set_text(&e->vars[ERR_SUBSYSTEM], "BASE");
  xb_gc_write(e);
  if (f->filename) {
    set_text(&e->vars[ERR_FILENAME], f->filename);
    xb_gc_write(e);
  }
  xb_set_int(&e->vars[ERR_GENCODE], f->gencode);
  xb_set_int(&e->vars[ERR_SUBCODE], f->subcode);
  xb_set_int(&e->vars[ERR_OSCODE], f->oscode);
  xb_set_int(&e->vars[ERR_SEVERITY], ES_ERROR);
  xb_set_logical(&e->vars[ERR_CANSUBSTITUTE], (flags & XB_ERR_SUBSTITUTE) != 0);
  xb_set_logical(&e->vars[ERR_CANRETRY], (flags & XB_ERR_RETRY) != 0);
  xb_set_logical(&e->vars[ERR_CANDEFAULT], (flags & XB_ERR_DEFAULT) != 0);
  xb_set_int(&e->vars[ERR_TRIES], f->tries);

  call[2] = ctx->error_block;
  ++ctx->error_depth;
  xb_block_call(ctx, &call[1], &call[2], 1, &call[0]);
  --ctx->error_depth;
  if (frame) xb_frame_pop(ctx, &inside);
  int outcome = 0;
  if (!(ctx->pending & XB_PENDING_UNWIND)) {
    if ((flags & XB_ERR_RETRY) && call[1].type == XB_T_LOGICAL && call[1].v.l) {
      outcome = 2;
    } else if (flags & XB_ERR_SUBSTITUTE) {
      *result = call[1];
      outcome = 1;
    } else if (flags & XB_ERR_DEFAULT) {
      outcome = 1;
    } else {
      unrecoverable(ctx, 9001, "Error recovery failure");
    }
  }
  xb_gc_root_pop(ctx, 3 + argc);
  return outcome;
}

int xb_error_in(xb_ctx *ctx, const char *frame, xb_item *result, int32_t gencode, int32_t subcode,
                const char *operation, uint32_t flags, uint32_t argc, const xb_item *args) {
  const failure f = {frame, NULL, gencode, subcode, operation, NULL, 0, flags, argc, args, 0};
  return raise(ctx, &f, result) == 1;
}

int xb_error_described(xb_ctx *ctx, xb_item *result, int32_t gencode, int32_t subcode, const char *description,
                       const char *operation, uint32_t flags, uint32_t argc, const xb_item *args) {
  const failure f = {NULL, description, gencode, subcode, operation, NULL, 0, flags, argc, args, 0};
  return raise(ctx, &f, result) == 1;
}

int xb_error_retry(xb_ctx *ctx, int32_t gencode, int32_t subcode, const char *operation, uint16_t tries) {
  const failure f = {NULL, NULL, gencode, subcode, operation, NULL, 0, XB_ERR_RETRY, 0, NULL, tries};
  return raise(ctx, &f, NULL) == 2;
}

int xb_error_file(xb_ctx *ctx, const char *frame, int32_t gencode, int32_t subcode, const char *filename,
                  int32_t oscode, uint16_t tries) {
  const failure f = {frame, NULL, gencode, subcode, "", filename, oscode, XB_ERR_RETRY | XB_ERR_DEFAULT, 0, NULL,
                     tries};
  return raise(ctx, &f, NULL);
}

/* ErrorBlock( [bNew] ): the current error block; a codeblock argument
 * replaces it. */
void xbf_ERRORBLOCK(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  *ret = ctx->error_block;
  if (argc >= 1 && argv[0].type == XB_T_BLOCK) ctx->error_block = argv[0];
}

/* Break( [xValue] ): unwinds to the innermost BEGIN SEQUENCE, whose RECOVER
 * USING variable receives xValue; with none active the program ends. */
static void request_break(xb_ctx *ctx, uint32_t argc, const xb_item *argv) {
  if (argc >= 1) ctx->break_value = argv[0];
  else xb_set_nil(&ctx->break_value);
  ctx->pending |= XB_PENDING_BREAK;
}

void xbf_BREAK(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  request_break(ctx, argc, argv);
}

/* Throw( [xValue] ): the same, under the name TRY ... CATCH reads best with. */
void xbf_THROW(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  request_break(ctx, argc, argv);
}

/* The BREAK [xValue] command (std.ch). */
void xbf___BREAK(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  request_break(ctx, argc, argv);
}

int32_t xb_seq_catch(xb_ctx *ctx) {
  if ((ctx->pending & XB_PENDING_UNWIND) != XB_PENDING_BREAK) return 0;
  ctx->pending &= ~(uint32_t)XB_PENDING_BREAK;
  return 1;
}

void xb_seq_value(xb_ctx *ctx, xb_item *r) {
  *r = ctx->break_value;
  xb_set_nil(&ctx->break_value);
}

void xb_seq_with(xb_ctx *ctx, xb_item *r, const xb_item *handler) {
  *r = ctx->error_block;
  if (handler->type == XB_T_BLOCK) ctx->error_block = *handler;
}

void xb_seq_restore(xb_ctx *ctx, const xb_item *old) { ctx->error_block = *old; }

uint32_t xb_seq_suspend(xb_ctx *ctx) {
  uint32_t requests = ctx->pending & XB_PENDING_UNWIND;
  ctx->pending &= ~requests;
  return requests;
}

void xb_seq_resume(xb_ctx *ctx, uint32_t requests, const xb_item *value) {
  if (ctx->pending & XB_PENDING_UNWIND) { /* ALWAYS made its own: it replaces them, but a QUIT stays */
    ctx->pending |= requests & XB_PENDING_QUIT;
    return;
  }
  ctx->pending |= requests;
  ctx->break_value = *value;
}
