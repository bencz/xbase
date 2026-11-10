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

static const xb_class error_class = {"ERROR", ERR_COUNT_, error_vars};

void xbf_ERRORNEW(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  xb_set_object(ret, xb_object_new(&error_class));
}

/* The description of each generic code the runtime raises (Clipper's texts). */
static const struct {
  int32_t gencode;
  const char *text;
} descriptions[] = {
  {EG_ARG, "Argument error"},          {EG_BOUND, "Bound error"},    {EG_STROVERFLOW, "String overflow"},
  {EG_ZERODIV, "Zero divisor"},        {EG_NOFUNC, "Undefined function"},  {EG_NOMETHOD, "No exported method"},
  {EG_NOVARMETHOD, "No exported variable"},
};

static const char *description_of(int32_t gencode) {
  for (size_t i = 0; i < sizeof descriptions / sizeof descriptions[0]; ++i)
    if (descriptions[i].gencode == gencode) return descriptions[i].text;
  return "";
}

static void set_text(xb_item *it, const char *s) { xb_set_str(it, xb_str_new(s, strlen(s))); }

/* An error the program cannot recover from: reported, and the program ends
 * with exit code 1. */
static void unrecoverable(xb_ctx *ctx, int code, const char *text) {
  fflush(stdout);
  fprintf(stderr, "\nUnrecoverable error %d: %s", code, text);
  xb_print_call_stack(ctx, "\nCalled from %s(%u)");
  fputc('\n', stderr);
  ctx->exitcode = 1;
  ctx->pending |= XB_PENDING_QUIT;
}

void xb_print_call_stack(const xb_ctx *ctx, const char *format) {
  for (const xb_frame *f = ctx->frames; f; f = f->prev)
    if (xb_frame_shown(f)) fprintf(stderr, format, f->proc->name, (unsigned)f->line);
}

int xb_error(xb_ctx *ctx, xb_item *result, int32_t gencode, int32_t subcode, const char *operation, uint32_t flags,
             uint32_t argc, const xb_item *args) {
  return xb_error_in(ctx, NULL, result, gencode, subcode, operation, flags, argc, args);
}

int xb_error_in(xb_ctx *ctx, const char *frame, xb_item *result, int32_t gencode, int32_t subcode,
                const char *operation, uint32_t flags, uint32_t argc, const xb_item *args) {
  xb_proc proc = {frame, 0, 0};
  xb_frame inside;
  if (frame) xb_frame_push(ctx, &inside, &proc);
  if (ctx->error_depth >= XB_ERROR_MAX_DEPTH) {
    unrecoverable(ctx, 9003, "Too many recursive error handler calls");
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
  xb_array *a = xb_array_alloc(argc);
  for (uint32_t i = 0; i < argc; ++i) a->items[i] = args[i];
  xb_set_array(&e->vars[ERR_ARGS], a);
  xb_gc_write(e);
  set_text(&e->vars[ERR_DESCRIPTION], description_of(gencode));
  xb_gc_write(e);
  set_text(&e->vars[ERR_OPERATION], operation);
  xb_gc_write(e);
  set_text(&e->vars[ERR_SUBSYSTEM], "BASE");
  xb_gc_write(e);
  xb_set_int(&e->vars[ERR_GENCODE], gencode);
  xb_set_int(&e->vars[ERR_SUBCODE], subcode);
  xb_set_int(&e->vars[ERR_SEVERITY], ES_ERROR);
  xb_set_logical(&e->vars[ERR_CANSUBSTITUTE], (flags & XB_ERR_SUBSTITUTE) != 0);

  call[2] = ctx->error_block;
  ++ctx->error_depth;
  xb_block_call(ctx, &call[1], &call[2], 1, &call[0]);
  --ctx->error_depth;
  if (frame) xb_frame_pop(ctx, &inside);
  int substituted = 0;
  if (!(ctx->pending & XB_PENDING_UNWIND)) {
    if (flags & XB_ERR_SUBSTITUTE) {
      *result = call[1];
      substituted = 1;
    } else {
      unrecoverable(ctx, 9001, "Error recovery failure");
    }
  }
  xb_gc_root_pop(ctx, 3 + argc);
  return substituted;
}

/* The error block a program starts with: a division by zero gives 0, as in
 * Clipper's default handler; any other error is reported ("Error
 * <subsystem>/<subcode>  <description>: <operation>", or the file name instead
 * of the operation when there is one, then the call stack) and ends the
 * program with exit code 1. */
static void default_handler(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv, xb_block *self) {
  XB_UNUSED(self);
  if (argc < 1 || argv[0].type != XB_T_OBJECT) return;
  const xb_object *e = argv[0].v.p;
  const xb_item *v = e->vars;
  if (e->cls != &error_class) return;
  if (v[ERR_GENCODE].v.i == EG_ZERODIV && v[ERR_CANSUBSTITUTE].v.l) {
    xb_set_int(ret, 0);
    return;
  }
  fflush(stdout);
  fprintf(stderr, "\n%s %s/%lld  %s", v[ERR_SEVERITY].v.i > ES_WARNING ? "Error" : "Warning",
          v[ERR_SUBSYSTEM].v.s->data, (long long)v[ERR_SUBCODE].v.i, v[ERR_DESCRIPTION].v.s->data);
  const xb_str *where = v[ERR_FILENAME].v.s->len ? v[ERR_FILENAME].v.s : v[ERR_OPERATION].v.s;
  if (where->len) fprintf(stderr, ": %s", where->data);
  xb_print_call_stack(ctx, XB_CALLED_FROM);
  ctx->exitcode = 1;
  ctx->pending |= XB_PENDING_QUIT;
}

void xb_error_init(xb_ctx *ctx) { xb_block_new(ctx, &ctx->error_block, default_handler, 0); }

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
