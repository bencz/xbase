// The names the compiler defines from the target (each run uses --target and -D options, tests/CMakeLists.txt).
#if __XBASE__ >= 100
? "xbase"
#endif
#ifdef __PLATFORM__WINDOWS
? "windows", __PLATFORM__WINDOWS
#endif
#ifdef __PLATFORM__LINUX
? "linux", __PLATFORM__LINUX
#endif
#ifdef __PLATFORM__DARWIN
? "darwin", __PLATFORM__DARWIN
#endif
#ifdef __PLATFORM__BSD
? "bsd", __PLATFORM__BSD
#endif
#ifdef __PLATFORM__UNIX
? "unix", __PLATFORM__UNIX
#endif
#if defined( __ARCH32BIT__ ) .AND. !defined( __ARCH64BIT__ )
? "32-bit", __ARCH32BIT__
#endif
#if defined( __ARCH64BIT__ ) .AND. !defined( __ARCH32BIT__ )
? "64-bit", __ARCH64BIT__
#endif
#if __LITTLE_ENDIAN__ == 1 .AND. !defined( __BIG_ENDIAN__ )
? "little-endian"
#endif
#if __BIG_ENDIAN__ == 1 .AND. !defined( __LITTLE_ENDIAN__ )
? "big-endian"
#endif
? "cpu", __XBASE_CPU__
#undef __XBASE_CPU__
? "undefined", __XBASE_CPU__
