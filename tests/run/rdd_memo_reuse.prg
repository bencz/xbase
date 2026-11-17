// DBT memos written over the memo they replace in a table open exclusively: a matrix of old and new lengths around
// the room of the old memo's blocks (the blocks up to its first end mark, and the next when a text one byte short of
// whole blocks has its second end mark there: a new text and its two end marks that fit are written over it, one
// byte more goes after the last memo; "=" the file kept its size, "+" it grew), each change checked against every
// other record's memo (a rewrite never reaches the next memo); 6000 changes of random lengths over 20 records (blocks
// freed by memos that shrink or move are taken again: the file stays within three times what the memos need, every
// memo as last written, after reopening and after PACK too); the same changes in a table open shared, where a value
// is always a new memo (the file grows). Long memos (5000, 100000 and 300000 bytes, read in several chunks) between
// short ones, read back, rewritten shorter and longer, and after PACK, exclusive and shared. The files start with
// "rdd_memo_reuse", deleted at the end.
FIELD NOTE

PROCEDURE Main()
   LOCAL aOld := { 1, 509, 510, 511, 512, 1021, 1022, 1023, 3000 }, aNew, nOld, nNew, nRoom, nSize, cLine, lOk := .T.
   LOCAL aTexts, i, k, x, nSeed := 7
   ErrorBlock( {| e | QOut( "ERR", e:subSystem, e:subCode, e:description ), Break( e ) } )
   FOR EACH nOld IN aOld
      nRoom := ( Int( nOld / 512 ) + 1 ) * 512
      aNew := { 1, nOld - 1, nOld, nRoom - 2, nRoom - 1, nRoom + 600 }
      cLine := ""
      FOR EACH nNew IN aNew
         IF nNew < 1
            LOOP
         ENDIF
         Fresh( { Text( 1, 100 ), Text( 2, nOld ), Text( 3, 50 ) } )
         nSize := Size()
         GO 2
         REPLACE NOTE WITH Text( 4, nNew )
         cLine += " " + LTrim( Str( nNew ) ) + IIf( Size() == nSize, "=", "+" )
         lOk := lOk .AND. Same( { Text( 1, 100 ), Text( 4, nNew ), Text( 3, 50 ) } )
         CLOSE
      NEXT
      ? "old", nOld, "room", nRoom, cLine
   NEXT
   ? "neighbors intact:", lOk

   FOR EACH k IN { .T., .F. }
      aTexts := Array( 20 )
      AFill( aTexts, "" )
      Fresh( aTexts, k )
      FOR i := 1 TO 6000
         nSeed := ( nSeed * 16807 ) % 2147483647
         GO nSeed % 20 + 1
         aTexts[ RecNo() ] := Text( i, nSeed % 1500 + 1 )
         IF ! k
            RLock()
         ENDIF
         REPLACE NOTE WITH aTexts[ RecNo() ]
         IF ! k .AND. i == 300
            EXIT
         ENDIF
      NEXT
      ? IIf( k, "exclusive:", "shared:" ), Same( aTexts ), IIf( k, Size() <= 3 * 20 * 3 * 512, Size() > 100 * 512 )
      CLOSE
      USE rdd_memo_reuse NEW EXCLUSIVE
      ? "reopened:", Same( aTexts )
      PACK
      ? "packed:", Same( aTexts ), Size() <= 20 * 3 * 512 + 512
      CLOSE
   NEXT
   FOR EACH k IN { .T., .F. }
      aTexts := { "short", Text( 1, 5000 ), "mid", Text( 2, 100000 ), Text( 3, 300000 ), "end" }
      Fresh( aTexts, k )
      x := Same( aTexts )
      GO 4
      RLock()
      aTexts[ 4 ] := Text( 4, 99999 )
      REPLACE NOTE WITH aTexts[ 4 ]
      GO 2
      RLock()
      aTexts[ 2 ] := Text( 5, 70000 )
      REPLACE NOTE WITH aTexts[ 2 ]
      dbUnlock()
      ? IIf( k, "long, exclusive:", "long, shared:" ), x, Same( aTexts )
      CLOSE
      USE rdd_memo_reuse NEW EXCLUSIVE
      PACK
      ? "packed:", Same( aTexts )
      CLOSE
   NEXT
   FErase( "rdd_memo_reuse.dbf" )
   FErase( "rdd_memo_reuse.dbt" )
RETURN

// A table whose records hold the texts, open exclusively (or shared with .F.).
STATIC PROCEDURE Fresh( aTexts, lExclusive )
   LOCAL c
   dbCreate( "rdd_memo_reuse", { { "NOTE", "M", 10, 0 } } )
   IF lExclusive == NIL .OR. lExclusive
      USE rdd_memo_reuse NEW EXCLUSIVE
   ELSE
      USE rdd_memo_reuse NEW SHARED
   ENDIF
   FOR EACH c IN aTexts
      APPEND BLANK
      REPLACE NOTE WITH c
   NEXT
   dbUnlock()
RETURN

// Whether every record holds its text.
STATIC FUNCTION Same( aTexts )
   LOCAL i
   FOR i := 1 TO Len( aTexts )
      GO i
      IF ! NOTE == aTexts[ i ]
         RETURN .F.
      ENDIF
   NEXT
RETURN .T.

// A text of n bytes, different for each k.
STATIC FUNCTION Text( k, n )
RETURN Left( Replicate( Chr( 65 + k % 26 ) + Chr( 97 + k % 23 ) + LTrim( Str( k ) ), Int( n / 2 ) + 1 ), n )

STATIC FUNCTION Size()
   LOCAL a := Directory( "rdd_memo_reuse.dbt" )
RETURN IIf( Empty( a ), -1, a[ 1 ][ 2 ] )
