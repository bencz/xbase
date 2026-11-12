// A shared CDX file in two areas of the program: each change is written at once under the file's lock, with the
// file's change counter one higher (header offset 8, big-endian), and the other area sees it at its next operation;
// a cursor whose entry moved finds its place again; record locks of the table follow FoxPro's places while the
// production file is open, and conflict between the areas. The files start with "rdd_cdx_shared", deleted at the end.
REQUEST DBFCDX
FIELD NAME

PROCEDURE Main()
   LOCAL c
   rddSetDefault( "DBFCDX" )
   AEval( Directory( "rdd_cdx_shared*.*" ), {| a | FErase( a[ 1 ] ) } ) // left by a run that failed
   dbCreate( "rdd_cdx_shared", { { "NAME", "C", 10, 0 } } )
   USE rdd_cdx_shared EXCLUSIVE
   FOR EACH c IN { "d", "b", "f", "a", "e", "c" }
      APPEND BLANK
      REPLACE NAME WITH c
   NEXT
   INDEX ON NAME TAG NAME
   CLOSE
   USE rdd_cdx_shared SHARED ALIAS one NEW
   ordSetFocus( "NAME" )
   USE rdd_cdx_shared SHARED ALIAS two NEW
   ordSetFocus( "NAME" )
   ? "counter:", Counter()
   one->( dbGoTop() )
   two->( dbGoTop() )
   two->( dbSkip() )
   ? "two on:", two->NAME, two->( RecNo() )
   one->( dbSeek( "b" ) )
   ? "lock b:", one->( RLock() ), two->( RLock() )
   one->NAME := "g"
   one->( dbUnlock() )
   ? "counter:", Counter()
   two->( dbSkip() )
   ? "two after b moved:", two->NAME, two->( RecNo() )
   two->( dbSkip( -1 ) )
   ? "back:", two->NAME, two->( RecNo() )
   SELECT one
   APPEND BLANK
   REPLACE NAME WITH "bb"
   dbUnlock()
   ? "counter:", Counter()
   SELECT two
   ? "two sees:", ordKeyCount(), LastRec()
   GO TOP
   c := ""
   DO WHILE ! Eof()
      c += AllTrim( NAME ) + " "
      SKIP
   ENDDO
   ? "two walks:", c
   ? "file lock:", one->( FLock() ), two->( RLock() ), two->( FLock() )
   one->( dbUnlock() )
   ? "after unlock:", two->( FLock() )
   CLOSE ALL
   FErase( "rdd_cdx_shared.dbf" )
   FErase( "rdd_cdx_shared.cdx" )
RETURN

// The production file's change counter.
STATIC FUNCTION Counter()
   LOCAL h := FOpen( "rdd_cdx_shared.cdx", 64 ), c := Space( 4 )
   FSeek( h, 8 )
   FRead( h, @c, 4 )
   FClose( h )
RETURN ( ( Asc( c ) * 256 + Asc( SubStr( c, 2 ) ) ) * 256 + Asc( SubStr( c, 3 ) ) ) * 256 + Asc( SubStr( c, 4 ) )
