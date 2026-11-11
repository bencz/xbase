/*
 * fileio.ch - the modes and codes of the low-level file functions (FOpen(),
 * FCreate(), FSeek()).
 *
 * Only #define lines and C comments: the runtime (C) includes this same file,
 * so the numbers are defined once for xBase code and for the runtime. The
 * numbers are the Clipper ones.
 */

#ifndef XB_FILEIO_CH
#define XB_FILEIO_CH

/* FOpen() access, one of */
#define FO_READ        0
#define FO_WRITE       1
#define FO_READWRITE   2

/* FOpen() sharing, added to the access (POSIX systems have none: ignored there) */
#define FO_COMPAT      0
#define FO_EXCLUSIVE   16
#define FO_DENYWRITE   32
#define FO_DENYREAD    48
#define FO_DENYNONE    64
#define FO_SHARED      64

/* FSeek() origins */
#define FS_SET         0
#define FS_RELATIVE    1
#define FS_END         2

/* FCreate() attributes, added together */
#define FC_NORMAL      0
#define FC_READONLY    1
#define FC_HIDDEN      2
#define FC_SYSTEM      4

/* What FOpen() and FCreate() return when they fail */
#define F_ERROR        ( -1 )

#endif
