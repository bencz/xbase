// The file functions: FCreate, FWrite (a count, all of the buffer for a count past it or below 0), FSeek from each
// origin (before the start: 25 from any origin, the position kept; an unknown origin is the start), FRead into a
// buffer passed by reference (none past its length), FReadStr up to a NUL, FClose, FError after each; MemoWrit (with
// the end mark) and MemoRead (without it); File and Directory with masks ("*.*" also takes a name without a dot, as in
// DOS, once: "*.*.*" does not; a very long mask; a directory's name with its separator lists it), attributes and
// hidden files; ADir (0 without a mask); FRename (never onto another existing file: 5; onto itself it succeeds) and
// FErase (not a directory: 5); FOpen of a directory (5), with access 3 (5) or of a missing file (2); a directory on
// the way that is missing (3, on every system); DirChange, CurDir, GetEnv and DiskSpace. Its files start with "files_"
// (".files_" for the one hidden where a leading dot hides a name) and are deleted at the end.
#include "fileio.ch"
#include "directry.ch"

#ifdef __PLATFORM__WINDOWS
#define DOT_HIDES .F. // the attribute is the file system's
#else
#define DOT_HIDES .T.
#endif

PROCEDURE Main()
   LOCAL h, c, a, x
   h := FCreate( "files_t1.txt" )
   ? "create:", h > 0, FError()
   ? "write:", FWrite( h, "hello" + Chr( 0 ) + "world" ), FWrite( h, "abcdef", 3 ), FWrite( h, "xy", 10 ), ;
      FWrite( h, "xy", -1 ), FError()
   ? "seek:", FSeek( h, 0, FS_END ), FSeek( h, 2 ), FSeek( h, 3, FS_RELATIVE ), FSeek( h, -100 ), FError(), ;
      FSeek( h, 0, FS_RELATIVE ), FSeek( h, -100, FS_RELATIVE ), FError(), FSeek( h, 1, 9 )
   ? "close:", FClose( h ), FError(), FClose( h ), FError()
   h := FOpen( "files_t1.txt" )
   c := Space( 4 )
   ? "read:", FRead( h, @c, 2 ), "[" + c + "]", FRead( h, @c, 10 ), "[" + c + "]", FRead( h, c, 2 ), FError()
   FSeek( h, 0 )
   ? "readstr:", "[" + FReadStr( h, 30 ) + "]", FSeek( h, 0, 1 ), "[" + FReadStr( h, 5 ) + "]"
   ? "write read-only:", FWrite( h, "zz" ), FError() != 0
   FClose( h )
   ? "memo:", MemoWrit( "files_t2.txt", "abc" ), Len( MemoRead( "files_t2.txt" ) ), MemoRead( "files_t2.txt" ), ;
      Len( MemoRead( "files_t1.txt" ) ), "[" + MemoRead( "files_none" ) + "]", FError()
   h := FOpen( "files_t2.txt", FO_READWRITE + FO_SHARED )
   c := Space( 8 )
   ? "end mark:", FRead( h, @c, 8 ), Asc( SubStr( c, 4, 1 ) ), FSeek( h, 0 ), FWrite( h, "Q" ), FClose( h )
   ? "missing:", FOpen( "files_none.txt" ), FError(), FOpen( "." ), FError(), FErase( "." ), FError(), ;
      FOpen( "files_t1.txt", 3 ), FError()
   ? "missing directory:", FCreate( "files_no/x.txt" ), FError(), FOpen( "files_no/x.txt" ), FError(), ;
      FErase( "files_no/x.txt" ), FError(), FRename( "files_no/x.txt", "files_y.txt" ), FError(), ;
      FRename( "files_t1.txt", "files_no/x.txt" ), FError(), File( "files_t1.txt" )
   MemoWrit( "files_noext", "x" )
   MemoWrit( ".files_hid", "" )
   ? "file:", File( "files_t1.txt" ), File( "files_t*.txt" ), File( "files_t?.txt" ), File( "files_zz*" ), ;
      File( ".files_hid" ), File( "." ), File( "" )
   a := Directory( "files_*" )
   ASort( a, , , {| x, y | x[ 1 ] < y[ 1 ] } )
   FOR EACH x IN a
      ?? "", x[ F_NAME ], x[ F_SIZE ], ValType( x[ F_DATE ] ), Len( x[ F_TIME ] ), x[ F_ATTR ], Len( x ) == F_LEN
   NEXT
   ? "odd masks:", Len( Directory( "files_noe*.*.*" ) ), Len( Directory( "files_noext.*" ) ), ;
      Len( Directory( Replicate( ".*", 100000 ) ) ), Len( Directory( "files_t?.*" ) ), Len( Directory( "*s_t1*" ) ), ;
      AScan( Directory( "./" ), {| x | x[ F_NAME ] == "files_t1.txt" } ) > 0
   ? "masks:", Len( Directory( "files_noe*.*" ) ), Len( Directory( "files_*.txt" ) ), ;
      Len( Directory( ".files_*" ) ) == iif( DOT_HIDES, 0, 1 ), Len( Directory( ".files_*", "H" ) ), ;
      Directory( ".files_*", "H" )[ 1 ][ 5 ] == iif( DOT_HIDES, "HA", "A" ), Len( Directory( "files_*", "V" ) )
   ? "dirs:", Len( Directory( "." ) ), Directory( ".", "D" )[ 1 ][ 1 ], Directory( ".", "D" )[ 1 ][ 5 ], ;
      Len( Directory( "*", "D" ) ) > Len( Directory( "*" ) )
   a := Array( 3 )
   ? "adir:", ADir(), ADir( "files_*" ), ADir( "files_*", a ), ASort( a )[ 1 ], a[ 3 ], ADir( "files_*", { 1 } )
   ? "rename:", FRename( "files_t2.txt", "files_t1.txt" ), FError(), FRename( "files_t2.txt", "files_t3.txt" ), ;
      FError(), File( "files_t3.txt" ), File( "files_t2.txt" ), FRename( "files_none", "files_x" ), FError(), ;
      FRename( "files_t3.txt", "files_t3.txt" ), FError()
   ? "dirchange:", DirChange( "." ), DirChange( "files_none" ), ValType( CurDir() )
   ? "env:", GetEnv( "" ), Len( GetEnv( "PATH" ) ) > 0, DiskSpace() > 0
   AEval( Directory( "files_*" ), {| x | FErase( x[ 1 ] ) } )
   FErase( ".files_hid" )
   ? "left:", Len( Directory( "files_*" ) ) + Len( Directory( ".files_*", "H" ) ), FErase( "files_t1.txt" ), FError()
RETURN
