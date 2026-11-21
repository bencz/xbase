// What qualified.xbprj gave: the defines of the target's platform, a quoted one, an include folder.
#include "qualified.ch"

PROCEDURE Main()
   ? GREETING, LEVEL, FROM_INCLUDE
#ifdef __PLATFORM__WINDOWS
   ? SIDE == "windows"
#else
   ? SIDE == "unix"
#endif
#ifdef __PLATFORM__LINUX
#ifdef LINUX_TOO
   ? "linux define"
#endif
#else
#ifndef LINUX_TOO
   ? "linux define"
#endif
#endif
#ifdef DARWIN_TOO
#ifndef __PLATFORM__DARWIN
   ? "darwin define on another platform"
#endif
#endif
RETURN
