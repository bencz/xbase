#include "xbrt_int.h"

#include <fcntl.h>
#include <io.h>
#include <stdio.h>

/* Program output is byte-exact on every platform: no CRLF translation. */
void xb_platform_init(void) {
  _setmode(_fileno(stdout), _O_BINARY);
  _setmode(_fileno(stderr), _O_BINARY);
}
