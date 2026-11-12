/* xbrt - internal declarations, not part of the compiler ABI. */
#ifndef XBRT_INT_H
#define XBRT_INT_H

#include "xbrt.h"

#include "core/bytes.h"
#include "core/context.h"
#include "core/error.h"
#include "core/memvar.h"
#include "core/numeric.h"
#include "core/names.h"
#include "core/symbols.h"
#include "core/sync.h"
#include "core/text.h"
#include "gc/gc.h"
#include "platform/platform.h"
#include "types/types.h"

/* Milliseconds since the program started (its first context), from the monotonic clock (core/ctx.c): ElapsedMs(). */
int64_t xb_program_elapsed_ms(void);

/* Functions with the fixed xBase calling convention do not use every parameter. */
#define XB_UNUSED(x) ((void)(x))

#endif
