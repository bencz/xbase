#include "xbrt_int.h"

char xb_type_letter(const xb_item *it) {
  _Static_assert(sizeof XB_TYPE_LETTERS == XB_T_COUNT_ + 1, "a letter per type tag");
  return it->type < XB_T_COUNT_ ? XB_TYPE_LETTERS[it->type] : 'U';
}

void xbf_VALTYPE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  xb_item nil;
  xb_set_nil(&nil);
  unsigned char c = (unsigned char)xb_type_letter(argc ? &argv[0] : &nil);
  xb_set_str(ret, (xb_str *)&xb_str_bytes[c]); /* the shared one-byte string */
}

void xbf_LEN(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (argc && argv[0].type == XB_T_STRING) {
    xb_set_int(ret, (int64_t)argv[0].v.s->len);
    return;
  }
  if (argc && argv[0].type == XB_T_ARRAY) {
    xb_set_int(ret, (int64_t)((const xb_array *)argv[0].v.p)->len);
    return;
  }
  if (argc && argv[0].type == XB_T_HASH) {
    xb_set_int(ret, (int64_t)((const xb_hash *)argv[0].v.p)->len);
    return;
  }
  xb_fn_arg_error(ctx, ret, 1111, "LEN", argc, argv);
}

/* QUIT / CANCEL: unwinds every active function and ends the program. */
void xbf___QUIT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  xb_ctx_raise(ctx, XB_PENDING_QUIT);
}

/* Eval( bBlock [, xArgs...] ) */
void xbf_EVAL(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (argc < 1) {
    xb_error_in(ctx, "(b)EVAL", ret, EG_NOMETHOD, 1004, "EVAL", XB_ERR_SUBSTITUTE, 0, argv);
    return;
  }
  xb_block_call(ctx, ret, &argv[0], argc - 1, argv + 1);
}

/* __GcCollect( [lYoung] ): a full collection (or a young one with .T.);
 * returns the live objects found by the last full collection. */
void xbf___GCCOLLECT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (argc > 0 && argv[0].type == XB_T_LOGICAL && argv[0].v.l) xb_gc_collect_young();
  else xb_gc_collect();
  xb_set_int(ret, (int64_t)xb_gc_live_objects());
  if (ctx->pending) xb_pending_gc(ctx); /* runs the destructors of what it found; the caller tests the rest */
}

/* The call stack level `n` (0: the function calling, 1: its caller, ...)
 * from argument 0; a level below 0 is the asking function itself. */
static const xb_frame *stack_level(const xb_ctx *ctx, uint32_t argc, const xb_item *argv, int64_t *n) {
  *n = argc && xb_is_number(&argv[0]) ? xb_num_to_i64(&argv[0]) : 0;
  int64_t k = *n;
  for (const xb_frame *f = ctx->frames; f; f = f->prev)
    if (xb_frame_shown(f) && k-- == 0) return f;
  return NULL;
}

/* ProcName( [nLevel] ): the name of the function at that level of the call
 * stack ("(b)NAME" for a codeblock of NAME), "" past its end. */
void xbf_PROCNAME(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  int64_t n;
  const xb_frame *f = stack_level(ctx, argc, argv, &n);
  const char *name = n < 0 ? "PROCNAME" : f ? f->proc->name : "";
  xb_set_str(ret, xb_str_new(name, strlen(name)));
}

/* ProcFile( [nLevel] ): the name of the source file (without directories) of
 * the function at that level, "" past the end of the stack, below level 0 or
 * for a runtime function. */
void xbf_PROCFILE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  int64_t n;
  const xb_frame *f = stack_level(ctx, argc, argv, &n);
  const char *file = n >= 0 && f && f->proc->file ? f->proc->file : "";
  xb_set_str(ret, xb_str_new(file, strlen(file)));
}

/* ProcLine( [nLevel] ): the source line running at that level, 0 past the
 * end of the stack or in a runtime function. */
void xbf_PROCLINE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  int64_t n;
  const xb_frame *f = stack_level(ctx, argc, argv, &n);
  xb_set_int(ret, n >= 0 && f ? f->line : 0);
}

/* The frame of the function calling: the innermost one the call stack shows
 * (a codeblock's own; Do's while Do runs); NULL when it keeps no arguments
 * (only a function that calls PCount() or PValue() itself does). */
static xb_frame *caller_frame(const xb_ctx *ctx) {
  xb_frame *f = ctx->frames;
  while (f && !f->proc) f = f->prev;
  return f && (f->proc->flags & XB_PROC_ARGS) ? f : NULL;
}

/* 1 when the frame is a method's: its first argument, Self, is not counted. */
static uint32_t self_of(const xb_frame *f) { return (f->proc->flags & XB_PROC_METHOD) && f->argc ? 1u : 0u; }

/* PCount(): the number of arguments the calling function (or codeblock)
 * received, parameters or not. */
void xbf_PCOUNT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  const xb_frame *f = caller_frame(ctx);
  xb_set_int(ret, f ? f->argc - self_of(f) : 0);
}

/* PValue( nArg, [xNew] ): argument nArg (1-based, a fraction truncated) of the
 * calling function: a parameter's current value, or the argument as passed past
 * the parameters; NIL outside 1 .. PCount(). With xNew, that argument becomes
 * xNew (through the reference when one was passed) and xNew is returned. */
void xbf_PVALUE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_frame *f = caller_frame(ctx);
  int64_t n = argc && xb_is_number(&argv[0]) ? xb_num_to_i64(&argv[0]) : 0;
  if (!f || n < 1 || n > f->argc - self_of(f)) return;
  n += self_of(f);
  xb_item *arg = (uint64_t)n <= f->proc->nparams ? &f->slots[n - 1] : &f->argv[n - 1];
  int ref = arg->type == XB_T_CELL || arg->type == XB_T_REF;
  xb_item *target = ref ? xb_ref_target(ctx, arg) : arg;
  if (ctx->pending && xb_pending(ctx)) return;
  if (argc > 1) {
    *target = argv[1];
    if (ref) xb_ref_written(arg);
  }
  *ret = *target;
}

/* __ArgError( nSubCode, [lSubstitute] ): the argument error (EG_ARG,
 * nSubCode) of the runtime function written in xBase that calls it, as its C
 * counterparts raise one: the operation is its name and the arguments are
 * those it received. With lSubstitute .T. the handler may give a result,
 * which it returns; else it cannot be substituted, and returns NIL. */
void xbf___ARGERROR(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  const xb_frame *f = caller_frame(ctx);
  int32_t subcode = argc && xb_is_number(&argv[0]) ? xb_num_wrap_int32(&argv[0]) : 0;
  int substitute = argc > 1 && argv[1].type == XB_T_LOGICAL && argv[1].v.l;
  xb_error(ctx, substitute ? ret : NULL, EG_ARG, subcode, f ? f->proc->name : "", substitute ? XB_ERR_SUBSTITUTE : 0,
           f ? f->argc : 0, f ? f->argv : NULL);
}

/* ErrorLevel( [nNew] ): the program's exit code; an argument replaces it: a
 * number wrapped to 32 bits, anything else 0. */
void xbf_ERRORLEVEL(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_set_int(ret, ctx->exitcode);
  if (argc) ctx->exitcode = xb_is_number(&argv[0]) ? xb_num_wrap_int32(&argv[0]) : 0;
}

/* Do( cName | bBlock, [xArgs...] ): the result of the function named cName
 * (letter case aside; the name ends at a NUL) or of the block, called with the
 * arguments; references go on as passed where the function takes them, a
 * runtime function receives the value elsewhere. Do shows on the call stack
 * while it runs, with its own arguments (PCount() and PValue() called through
 * Do see them), and ends the PRIVATEs a runtime function created for its
 * caller (RESTORE), as a compiled caller does. */
static void do_frame_push(xb_ctx *ctx, xb_frame *frame, uint32_t argc, xb_item *argv) {
  static const xb_proc proc = XB_RUNTIME_PROC("DO", XB_PROC_ARGS);
  xb_frame_push(ctx, frame, &proc);
  frame->argc = argc;
  frame->argv = argv;
}

void xbf_DO(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_frame frame;
  if (argc && argv[0].type == XB_T_STRING) {
    const char *name = argv[0].v.s->data;
    const xb_symbol_entry *e = xb_symbol_find(name, strlen(name));
    if (!e) {
      xb_error_in(ctx, "DO", NULL, EG_NOFUNC, 1001, name, 0, argc, argv);
      return;
    }
    if (!xb_args_rest(ctx, argv + 1, argv + 1, argc - 1, 0, xb_symbol_refs(e))) return;
    do_frame_push(ctx, &frame, argc, argv);
    uint32_t mark = xb_mv_mark(ctx);
    e->fn(ctx, ret, argc - 1, argv + 1);
    xb_mv_release(ctx, mark);
    xb_frame_pop(ctx, &frame);
  } else if (argc && argv[0].type == XB_T_BLOCK) {
    do_frame_push(ctx, &frame, argc, argv);
    xb_block_call(ctx, ret, &argv[0], argc - 1, argv + 1);
    xb_frame_pop(ctx, &frame);
  } else {
    xb_fn_arg_error(ctx, ret, 3012, "DO", argc, argv);
  }
}
