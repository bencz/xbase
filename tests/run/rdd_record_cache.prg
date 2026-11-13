// An exclusive table read out of order keeps pages of its records in memory: what is read is always the table as it
// is now: records changed after their page was read (the last one too, which an append keeps behind), records
// appended after the last page was read, a deleted record, and the table after PACK and ZAP; also a table whose
// records are wider than a page, and one whose file is shorter than its header says. The files start with "rdd_rc",
// deleted at the end.
FIELD K, N, W
PROCEDURE Main()
   LOCAL i, s
   dbCreate( "rdd_rc", { { "K", "C", 6, 0 }, { "N", "N", 8, 0 } } )
   USE rdd_rc NEW EXCLUSIVE
   FOR i := 1 TO 3000
      APPEND BLANK
      REPLACE K WITH StrZero( ( i * 7919 ) % 3001, 6 ), N WITH i
   NEXT
   INDEX ON K TO rdd_rc
   ? "sum", Walk()
   GO 1500
   REPLACE N WITH -1
   GO 3000
   REPLACE N WITH -2
   GO 10
   ? "changed", Walk(), Seen( 1500 ), Seen( 3000 )
   FOR i := 1 TO 5
      APPEND BLANK
      REPLACE K WITH "Z" + Str( i, 1 ), N WITH 100000 * i
   NEXT
   ? "appended", Walk(), Seen( 3003 ), LastRec()
   GO 3002
   DELETE
   ? "deleted", Seen( 3002 ), Deleted()
   SET INDEX TO
   PACK
   ? "packed", LastRec(), Walk()
   GO 3002
   ? N, K
   ZAP
   APPEND BLANK
   REPLACE K WITH "only", N WITH 7
   ? "zapped", LastRec(), Walk()
   CLOSE
   dbCreate( "rdd_rcw", { { "W", "C", 5000, 0 }, { "N", "N", 6, 0 } } )
   USE rdd_rcw NEW EXCLUSIVE
   FOR i := 1 TO 20
      APPEND BLANK
      REPLACE W WITH Replicate( Chr( 64 + i ), 5000 ), N WITH i
   NEXT
   s := ""
   FOR i := 20 TO 1 STEP -3
      GO i
      s += Left( W, 1 ) + Str( N, 3 )
   NEXT
   GO 7
   REPLACE W WITH "changed"
   GO 20
   GO 7
   ? "wide", s, Trim( W ), N
   CLOSE
   Truncated()
   FErase( "rdd_rc.dbf" )
   FErase( "rdd_rc.ntx" )
   FErase( "rdd_rcw.dbf" )
   FErase( "rdd_rct.dbf" )
RETURN

// A table whose file lost the end of its last record: the records before it still read, out of order.
STATIC PROCEDURE Truncated()
   LOCAL i, s
   dbCreate( "rdd_rct", { { "N", "N", 6, 0 } } )
   USE rdd_rct NEW EXCLUSIVE
   FOR i := 1 TO 10
      APPEND BLANK
      REPLACE N WITH i * 11
   NEXT
   CLOSE
   s := MemoRead( "rdd_rct.dbf" )
   MemoWrit( "rdd_rct.dbf", Left( s, Len( s ) - 7 ) )
   USE rdd_rct NEW EXCLUSIVE
   s := ""
   FOR EACH i IN { 5, 2, 9, 1 }
      GO i
      s += Str( N, 4 )
   NEXT
   ? "truncated", s
   CLOSE

// The sum of N in the current order, from its top.
STATIC FUNCTION Walk()
   LOCAL nSum := 0
   GO TOP
   DO WHILE ! Eof()
      nSum += N
      SKIP
   ENDDO
RETURN nSum

STATIC FUNCTION Seen( nRec )
   GO nRec
RETURN N
