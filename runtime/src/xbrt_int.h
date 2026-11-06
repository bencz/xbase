/* xbrt - internal declarations, not part of the compiler ABI. */
#ifndef XBRT_INT_H
#define XBRT_INT_H

#include "xbrt.h"

#include "core/context.h"
#include "core/error.h"
#include "core/numeric.h"
#include "core/text.h"
#include "gc/gc.h"
#include "platform/platform.h"
#include "types/types.h"

/* Functions with the fixed xBase calling convention do not use every parameter. */
#define XB_UNUSED(x) ((void)(x))

#endif
