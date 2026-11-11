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
#define XB_ERR_RETRY      0x2u /* canRetry: when it returns .T., the operation is tried again */
#define XB_ERR_DEFAULT    0x4u /* canDefault: when it returns anything else, the operation does its default */

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

/* An error the operation may retry (canRetry, `tries` the attempt, from 1;
 * no arguments): 1 when the error block returned .T. and the operation is to
 * be tried again; else 0, with a request to unwind pending (the block
 * returning anything else is an unrecoverable error). */
int xb_error_retry(xb_ctx *ctx, int32_t gencode, int32_t subcode, const char *operation, uint16_t tries);

/* The error of a file operation in the runtime function `frame` (canRetry
 * and canDefault, no operation or arguments): `filename` and `oscode` (the
 * FError() code, see platform.h) are the Error object's, `tries` the attempt,
 * from 1. Returns 2 when the error block returned .T. (try again), 1 when it
 * returned anything else (the operation's default: it gives up quietly), 0
 * with a request to unwind pending. */
int xb_error_file(xb_ctx *ctx, const char *frame, int32_t gencode, int32_t subcode, const char *filename,
                  int32_t oscode, uint16_t tries);

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

#endif
