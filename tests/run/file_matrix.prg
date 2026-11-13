// The low-level file functions over their limits: FCreate() and FOpen() with each mode and sharing flag, of files that
// exist or not; FWrite() of all of a string, part of it, more than it has, 0 and negative counts, NUL and bytes above
// 127; FSeek() from each origin to before the start, the end and past it; FRead() and FReadStr() of more than the file
// holds, into a shorter buffer, at the end; operations on closed and invalid handles; FError() after each; File(),
// FRename(), FErase(), MemoRead() and MemoWrit(); Directory() of the files made. Files start with "file_matrix". As
// the reference, but where docs/runtime.md tells: a seek before the start is error 25 from any origin, an unknown
// origin is the start, an access of 3 is error 5, and FRename() does not replace an existing file.
#include "fileio.ch"

PROCEDURE Main()
   LOCAL h, c, n, x, h2
   FErase( "file_matrix_a" )
   FErase( "file_matrix_b" )
   ? "open missing", FOpen( "file_matrix_a" ), FError(), File( "file_matrix_a" )
   h := FCreate( "file_matrix_a" )
   ? "create", h > 0, FError(), File( "file_matrix_a" )
   FOR EACH x IN { { "abc" }, { "defgh", 2 }, { "ij", 5 }, { "kl", 0 }, { "mn", -1 }, ;
         { "o" + Chr( 0 ) + "p" + Chr( 200 ) }, { "", 3 }, { "qrs", 2.9 } }
      IF Len( x ) == 1
         n := FWrite( h, x[ 1 ] )
      ELSE
         n := FWrite( h, x[ 1 ], x[ 2 ] )
      ENDIF
      ?? "", n, FError()
   NEXT
   ? "seek", FSeek( h, 0, FS_END ), FSeek( h, 0 ), FSeek( h, 3, FS_SET ), FSeek( h, 2, FS_RELATIVE ), ;
      FSeek( h, -2, FS_END ), FSeek( h, -100, FS_SET ), FError(), FSeek( h, 100, FS_SET ), ;
      FSeek( h, 0, FS_END ), FSeek( h, -100, FS_RELATIVE ), FError(), FSeek( h, 0, 7 )
   ? "close", FClose( h ), FError(), FClose( h ), FError(), FWrite( h, "x" ), FError(), FSeek( h, 0 ), FError()
   FOR EACH n IN { FO_READ, FO_WRITE, FO_READWRITE, FO_READ + FO_SHARED, FO_READ + FO_DENYWRITE, 99 }
      h := FOpen( "file_matrix_a", n )
      c := Space( 4 )
      ? "mode", n, h > 0, FError(), FRead( h, @c, 4 ), FError(), Shown( c ), FWrite( h, "Z" ), FError()
      FClose( h )
   NEXT
   h := FOpen( "file_matrix_a", FO_READWRITE )
   c := Space( 3 )
   ? "read", FRead( h, @c, 3 ), Shown( c ), FRead( h, @c, 2 ), Shown( c ), FRead( h, @c, 100 ), Shown( c ), ;
      FRead( h, @c, 0 ), FRead( h, @c, -1 ), FError()
   FSeek( h, 0 )
   ? "readstr", Shown( FReadStr( h, 4 ) ), Shown( FReadStr( h, 100 ) ), Shown( FReadStr( h, 3 ) ), ;
      Len( FReadStr( h, 0 ) )
   FSeek( h, 0 )
   c := ""
   ? "empty buffer", FRead( h, @c, 5 ), Shown( c ), FError()
   FClose( h )
   ? "invalid", FRead( -1, @c, 1 ), FError(), FWrite( 9999, "x" ), FError(), FClose( 9999 ), FError(), ;
      FSeek( -1, 0 ), FError(), Shown( FReadStr( -1, 2 ) ), FError()
   ? "memo", Shown( MemoRead( "file_matrix_a" ) ), ;
      MemoWrit( "file_matrix_b", "line" + Chr( 13 ) + Chr( 10 ) + "two" ), Shown( MemoRead( "file_matrix_b" ) ), ;
      Shown( MemoRead( "file_matrix_none" ) )
   ? "rename", FRename( "file_matrix_b", "file_matrix_c" ), FError(), File( "file_matrix_b" ), ;
      File( "file_matrix_c" ), FRename( "file_matrix_none", "file_matrix_d" ), FError(), ;
      FRename( "file_matrix_a", "file_matrix_c" ), FError()
   ? "directory"
   FOR EACH x IN ASort( Directory( "file_matrix*" ),,, {| a, b | a[ 1 ] < b[ 1 ] } )
      ?? "", x[ 1 ], x[ 2 ], ValType( x[ 3 ] ), ValType( x[ 4 ] ), x[ 5 ]
   NEXT
   h2 := FCreate( "file_matrix_c" )
   ? "recreate", h2 > 0, FError(), FSeek( h2, 0, FS_END )
   FClose( h2 )
   ? "erase", FErase( "file_matrix_a" ), FError(), FErase( "file_matrix_a" ), FError(), FErase( "file_matrix_c" ), ;
      File( "file_matrix_a" ), File( "file_matrix_c" ), Len( Directory( "file_matrix*" ) )
RETURN

// The bytes of a string, those outside 32-126 as <n>.
STATIC FUNCTION Shown( c )
   LOCAL r := "", i, n
   FOR i := 1 TO Len( c )
      n := Asc( SubStr( c, i, 1 ) )
      r += iif( n < 32 .OR. n > 126, "<" + LTrim( Str( n ) ) + ">", Chr( n ) )
   NEXT
RETURN "[" + r + "]"
