// A shared NTX index changed exactly 65,536 times by another area between two operations of one area, so that the
// header's 2-byte change counter comes back to the value this area saw: its pages kept are stale all the same (the
// 64-bit counter beside it, and the root, tell), so its SEEK finds the record by its new key, and its REPLACE leaves
// the index whole (every record once, keys in order). The same with 65,535 changes, and 65,537. The files start with
// "rdd_index_wrap", deleted at the end.
FIELD K

PROCEDURE Main()
   LOCAL n
   FOR EACH n IN { 65535, 65536, 65537 }
      ? n, Run( n )
   NEXT
   FErase( "rdd_index_wrap.dbf" )
   FErase( "rdd_index_wrap.ntx" )
RETURN

STATIC FUNCTION Run( nChanges )
   LOCAL i, cSeen, nPrev, nCount := 0, lOrder := .T.
   dbCreate( "rdd_index_wrap", { { "K", "N", 8, 0 } } )
   USE rdd_index_wrap NEW EXCLUSIVE
   FOR i := 1 TO 1000
      APPEND BLANK
      REPLACE K WITH i
   NEXT
   INDEX ON K TO rdd_index_wrap
   CLOSE
   USE rdd_index_wrap ALIAS one NEW SHARED INDEX rdd_index_wrap
   USE rdd_index_wrap ALIAS two NEW SHARED INDEX rdd_index_wrap
   SELECT one
   SEEK 500
   cSeen := LTrim( Str( RecNo() ) )
   SELECT two
   GO 500
   RLock()
   FOR i := 1 TO nChanges
      REPLACE K WITH IIf( i % 2 == 1, 100000 + i, 500 + i )
      dbCommit()
   NEXT
   dbUnlock()
   SELECT one
   SEEK 500
   cSeen += " " + IIf( Found(), LTrim( Str( RecNo() ) ) + "=" + LTrim( Str( K ) ), "not found" )
   SEEK two->K
   cSeen += " " + IIf( Found() .AND. RecNo() == 500, "new key found", "new key lost" )
   GO 10
   RLock()
   REPLACE K WITH 2000000
   dbUnlock()
   GO TOP
   DO WHILE ! Eof()
      lOrder := lOrder .AND. ( nCount == 0 .OR. K >= nPrev )
      nPrev := K
      nCount++
      SKIP
   ENDDO
   CLOSE ALL
RETURN cSeen + " " + LTrim( Str( nCount ) ) + " " + IIf( lOrder, "in order", "OUT OF ORDER" )
