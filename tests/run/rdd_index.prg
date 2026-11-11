// Orders (NTX): INDEX ON a character, a number, a date (DESCENDING), UNIQUE; the record order they give, GO TOP and
// BOTTOM, SKIP past either end, GOTO then SKIP; SEEK of a whole and a partial key, of a number and a date, not found
// (EOF), with SET SOFTSEEK and dbSeek( key, lSoft, lLast ); SET EXACT does not change a SEEK; a record whose key
// changes moves (SKIP goes on from where it is now); a record appended comes in with its blank key; SET DELETED; SET
// INDEX TO, SET ORDER TO n and 0, USE ... INDEX, REINDEX; IndexOrd() and IndexKey(). The files start with "rdd_index",
// deleted at the end.
FIELD NAME, N, D

PROCEDURE Main()
   LOCAL i, a := { "pear", "apple", "fig", "kiwi", "apple", "date", "lime", "banana" }
   dbCreate( "rdd_index", { { "NAME", "C", 8, 0 }, { "N", "N", 5, 1 }, { "D", "D", 8, 0 } } )
   USE rdd_index NEW
   FOR i := 1 TO Len( a )
      APPEND BLANK
      REPLACE NAME WITH a[ i ], N WITH ( 5 - i ) * 1.5, D WITH SToD( "2024010" + Str( 9 - i, 1 ) )
   NEXT
   GO 5
   INDEX ON NAME TO rdd_index_name
   ? "after index:", RecNo(), NAME, IndexOrd(), IndexKey( 0 ), IndexKey( 1 ), Bof(), Eof()
   Walk()
   GO BOTTOM
   ? "bottom:", RecNo(), NAME
   SKIP -1
   ? "skip -1:", RecNo(), NAME
   SKIP 2
   ? "skip past:", RecNo(), Eof(), "[" + NAME + "]"
   SKIP -1
   ? "back:", RecNo(), NAME
   GO 3
   SKIP
   ? "goto 3, skip:", RecNo(), NAME
   SEEK "apple"
   ? "seek apple:", Found(), RecNo(), Eof()
   SKIP
   ? "next:", RecNo(), NAME
   SEEK "ki"
   ? "seek ki:", Found(), RecNo(), NAME
   SEEK "zzz"
   ? "seek zzz:", Found(), RecNo(), Eof()
   SEEK "c"
   ? "seek c:", Found(), RecNo(), Eof()
   SET SOFTSEEK ON
   SEEK "c"
   ? "softseek c:", Found(), RecNo(), NAME, Eof()
   SEEK "zzz"
   ? "softseek zzz:", Found(), RecNo(), Eof()
   SET SOFTSEEK OFF
   ? "dbseek last:", dbSeek( "apple", .F., .T. ), RecNo()
   ? "dbseek soft:", dbSeek( "cherry", .T. ), RecNo(), NAME
   SEEK ""
   ? "seek empty:", Found(), RecNo()
   SET EXACT ON
   SEEK "ki"
   ? "exact on seek ki:", Found(), RecNo()
   SET EXACT OFF
   GO 1
   REPLACE NAME WITH "aaa"
   ? "replaced:", RecNo(), NAME
   SKIP
   ? "skip after replace:", RecNo(), NAME
   GO TOP
   ? "top:", RecNo(), NAME
   APPEND BLANK
   ? "appended:", RecNo(), "[" + NAME + "]"
   GO TOP
   ? "top after append:", RecNo(), "[" + NAME + "]"
   SKIP
   ? RecNo(), NAME
   GO 2
   DELETE
   SET DELETED ON
   Walk()
   SET DELETED OFF
   INDEX ON N TO rdd_index_n
   ? "num index:", IndexOrd(), IndexKey(), IndexKey( 1 ), IndexKey( 2 )
   Walk()
   SEEK 1.5
   ? "seek 1.5:", Found(), RecNo(), N
   SEEK -3
   ? "seek -3:", Found(), RecNo(), N
   SEEK 0
   ? "seek 0:", Found(), RecNo()
   SET INDEX TO rdd_index_name, rdd_index_n
   ? "set index:", IndexOrd(), IndexKey(), RecNo()
   SET ORDER TO 2
   ? "order 2:", IndexOrd(), IndexKey(), RecNo()
   GO TOP
   ? "top n:", RecNo(), N
   SET ORDER TO 0
   GO TOP
   ? "order 0:", IndexOrd(), RecNo()
   SET ORDER TO 1
   INDEX ON D TO rdd_index_d DESCENDING
   Walk()
   SEEK SToD( "20240105" )
   ? "seek date desc:", Found(), RecNo(), DToS( D )
   INDEX ON NAME TO rdd_index_u UNIQUE
   Walk()
   CLOSE
   USE rdd_index INDEX rdd_index_name, rdd_index_n NEW
   ? "use index:", IndexOrd(), IndexKey(), RecNo(), NAME
   SET ORDER TO 2
   REINDEX
   ? "reindex:", IndexOrd(), RecNo()
   Walk()
   CLOSE
   FErase( "rdd_index.dbf" )
   AEval( { "name", "n", "d", "u" }, {| c | FErase( "rdd_index_" + c + ".ntx" ) } )
RETURN

// The records in the controlling order: number and name.
STATIC PROCEDURE Walk()
   GO TOP
   ?? Chr( 10 ) + "  "
   DO WHILE ! Eof()
      ?? LTrim( Str( RecNo() ) ) + ":" + Trim( NAME ) + " "
      SKIP
   ENDDO
RETURN
