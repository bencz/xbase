// PACK of tables that span many of the blocks it reads and writes at once (64 KB of records): records of 3 and of
// 200 bytes, deleted in runs that cross the blocks' edges (the first record, the last, every third, a run of 700),
// with memos (DBT and FPT: their copies and the records' blocks moved down after them) and without, and an index
// rebuilt after. What is left is checked whole: each record kept, in order, with its own memo. The files start with
// "rdd_pack_blocks", deleted at the end.
REQUEST DBFCDX
FIELD ID, PAD, NOTE

PROCEDURE Main()
   Pack( "DBFNTX", 3, .F. )
   Pack( "DBFNTX", 200, .F. )
   Pack( "DBFNTX", 200, .T. )
   Pack( "DBFCDX", 200, .T. )
   Pack( "DBFCDX", 3, .T. )
   AEval( Directory( "rdd_pack_blocks*.*" ), {| a | FErase( a[ 1 ] ) } )
RETURN

STATIC PROCEDURE Pack( cRdd, nPad, lMemo )
   LOCAL i, n := 3000, aStruct := { { "ID", "N", 6, 0 }, { "PAD", "C", nPad, 0 } }, nKept := 0, nBad := 0, nNext
   IF lMemo
      AAdd( aStruct, { "NOTE", "M", 10, 0 } )
   ENDIF
   dbCreate( "rdd_pack_blocks", aStruct, cRdd )
   USE rdd_pack_blocks VIA cRdd EXCLUSIVE NEW
   FOR i := 1 TO n
      APPEND BLANK
      REPLACE ID WITH i, PAD WITH Replicate( Chr( 65 + i % 26 ), nPad )
      IF lMemo .AND. i % 4 != 0
         REPLACE NOTE WITH "note " + LTrim( Str( i ) ) + Replicate( "*", i % 300 )
      ENDIF
   NEXT
   INDEX ON ID TO rdd_pack_blocks
   DELETE FOR ID == 1 .OR. ID == n .OR. ID % 3 == 0 .OR. ( ID > 1000 .AND. ID <= 1700 )
   PACK
   GO TOP
   nNext := 1
   DO WHILE !Eof()
      DO WHILE nNext == 1 .OR. nNext == n .OR. nNext % 3 == 0 .OR. ( nNext > 1000 .AND. nNext <= 1700 )
         ++nNext
      ENDDO
      IF ID != nNext .OR. PAD != Replicate( Chr( 65 + nNext % 26 ), nPad ) .OR. ;
         ( lMemo .AND. !( FieldGet( 3 ) == IIf( nNext % 4 != 0, "note " + LTrim( Str( nNext ) ) + ;
         Replicate( "*", nNext % 300 ), "" ) ) )
         ++nBad
      ENDIF
      ++nKept
      ++nNext
      SKIP
   ENDDO
   ? cRdd, nPad, lMemo, "kept:", nKept, LastRec(), ordKeyCount(), "wrong:", nBad
   CLOSE
   AEval( Directory( "rdd_pack_blocks*.*" ), {| a | FErase( a[ 1 ] ) } )
RETURN
