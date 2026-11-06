/* Runtime errors: Error objects, the error block and BEGIN SEQUENCE. */
#ifndef XB_ERROR_H
#define XB_ERROR_H

#include "core/context.h"

#include "error.ch"

/* The error block may raise errors itself, nested this deep at most (an
 * unrecoverable error past it). */
#define XB_ERROR_MAX_DEPTH 8

/* What the error block may do with an error (the Error object's can*
 * variables). */
#define XB_ERR_SUBSTITUTE 0x1u /* canSubstitute: its return value becomes the failed operation's result */

/* Raises an error: makes an Error object of the BASE subsystem (severity
 * ES_ERROR, `args` the `argc` items of `args`) and evaluates the error block
 * with it. Returns 1 when the operation goes on with the block's return value
 * in `result` (XB_ERR_SUBSTITUTE); returns 0 when it must fail, with a request
 * to unwind pending: a BREAK or QUIT from the block, or an unrecoverable
 * error when the block returned for an error that allows nothing. `result`
 * (NULL without XB_ERR_SUBSTITUTE) may be one of `args`. */
int xb_error(xb_ctx *ctx, xb_item *result, int32_t gencode, int32_t subcode, const char *operation, uint32_t flags,
             uint32_t argc, const xb_item *args);
/* The same, raised inside the runtime function (or method, "CLASS:NAME")
 * `frame` names, which the call stack shows while the error block runs. */
int xb_error_in(xb_ctx *ctx, const char *frame, xb_item *result, int32_t gencode, int32_t subcode,
                const char *operation, uint32_t flags, uint32_t argc, const xb_item *args);

/* The argument error of an operator (EG_ARG, may be substituted), with its
 * operands. */
static inline int xb_arg_error(xb_ctx *ctx, xb_item *result, int32_t subcode, const char *operation, uint32_t argc,
                               const xb_item *args) {
  return xb_error(ctx, result, EG_ARG, subcode, operation, XB_ERR_SUBSTITUTE, argc, args);
}

/* The argument error of the runtime function `fn` (EG_ARG, may be
 * substituted), with its arguments; `fn` is the operation and shows on the
 * call stack. */
static inline int xb_fn_arg_error(xb_ctx *ctx, xb_item *result, int32_t subcode, const char *fn, uint32_t argc,
                                  const xb_item *args) {
  return xb_error_in(ctx, fn, result, EG_ARG, subcode, fn, XB_ERR_SUBSTITUTE, argc, args);
}

/* The lines of the call stack, from the innermost frame it shows, to standard
 * error: `format` takes the name and the line, as XB_CALLED_FROM (the default
 * error block's lines) does. */
#define XB_CALLED_FROM "\nCalled from %s(%u)  "
void xb_print_call_stack(const xb_ctx *ctx, const char *format);

/* The context's error block (the default one) and BREAK value, set up by
 * xb_ctx_new. */
void xb_error_init(xb_ctx *ctx);

#endif
