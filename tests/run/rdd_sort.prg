// SORT (keys in any letter case, /D, /C, /AD, /DA, several keys, numbers, dates, logicals; equal keys in the order
// of the scope; the scope, SET DELETED; no deleted mark copied; the table left on the record copied last; no key: the
// order of the scope), TOTAL (runs of the same key, the record of the first one the FOR block takes, its deleted mark;
// sums; WHILE; one run without a key; a sum too wide), UPDATE (in the order of the key, and RANDOM with SEEK) and JOIN
// (the pairs the FOR block takes, FIELDS of either table, without FIELDS the current table's). Deliberate differences:
// TOTAL's NEXT and RECORD count the records the scope visits (the reference loses or adds records), its FIELDS must
// name number fields (DBCMD/1015 before anything is made; the reference adds dates and fails half way), FIELD-> names
// a field there too; a new file that cannot be made gives .F. and leaves the tables as they were (the reference writes
// into them and closes them); JOIN fills a field from the table it was chosen from and leaves out a name chosen twice.
// Blocks that close the new file or change what the command was given do no harm. The files start with "rdd_sort_",
// deleted at the end.
FIELD NAME, AMT, QTY, D, L, CODE, SM, NOTE

PROCEDURE Main()
   AEval( Directory( "rdd_sort_*.*" ), {| a | FErase( a[ 1 ] ) } ) // left by a run that failed
   Sorts()
   Totals()
   Updates()
   Joins()
   Deliberate()
   AEval( Directory( "rdd_sort_*.*" ), {| a | FErase( a[ 1 ] ) } )
RETURN

STATIC PROCEDURE Sorts()
   LOCAL i, a := { { "beta", 3.5, 2, "20240105", .T. }, { "Alpha", -1.25, 10, "20231231", .F. }, ;
      { "alpha", 1000, -3, "20240105", .T. }, { "_x", 0, 7, "", .F. }, { "Beta", 3.5, 2, "19990101", .T. }, ;
      { "alpha", -1.25, 10, "20231231", .F. }, { "", NIL, NIL, "", NIL }, { "ab", 12345.67, 9999, "20240105", .T. } }
   dbCreate( "rdd_sort_s", { { "NAME", "C", 6, 0 }, { "AMT", "N", 8, 2 }, { "QTY", "N", 4, 0 }, { "D", "D", 8, 0 }, ;
      { "L", "L", 1, 0 } } )
   USE rdd_sort_s EXCLUSIVE NEW
   FOR i := 1 TO Len( a )
      APPEND BLANK
      REPLACE NAME WITH a[ i, 1 ], D WITH SToD( a[ i, 4 ] )
      IF a[ i, 2 ] != NIL
         REPLACE AMT WITH a[ i, 2 ], QTY WITH a[ i, 3 ], L WITH a[ i, 5 ]
      ENDIF
   NEXT
   GO 3
   DELETE
   SORT TO rdd_sort_1 ON name
   ? "on name:", Show( "rdd_sort_1" ), RecNo()
   SORT TO rdd_sort_1 ON name/D
   ? "name/D:", Show( "rdd_sort_1" ), RecNo()
   SORT TO rdd_sort_1 ON NAME /C
   ? "name/C:", Show( "rdd_sort_1" )
   SORT TO rdd_sort_1 ON name /DC
   ? "name/DC:", Show( "rdd_sort_1" )
   SORT TO rdd_sort_1 ON amt
   ? "amt:", Show( "rdd_sort_1" )
   SORT TO rdd_sort_1 ON l, d /d, name
   ? "l, d/d, name:", Show( "rdd_sort_1" )
   SORT TO rdd_sort_1 ON qty/AD FOR qty > 0
   ? "qty/AD for:", Show( "rdd_sort_1" ), RecNo()
   SORT TO rdd_sort_1 ON qty/DA
   ? "qty/DA:", Show( "rdd_sort_1" )
   SET DELETED ON
   GO 2
   SORT TO rdd_sort_1 ON name NEXT 3
   ? "next 3:", Show( "rdd_sort_1" ), RecNo()
   SET DELETED OFF
   SORT TO rdd_sort_1 ON nosuch
   ? "no key:", Show( "rdd_sort_1" ), RecNo(), Eof()
   SORT TO rdd_sort_1 FOR .F.
   ? "none:", Show( "rdd_sort_1" ), Eof()
   CLOSE
RETURN

STATIC PROCEDURE Totals()
   LOCAL i, a := { { "aa", 1.5, 1, 1, "20240101", .T. }, { "aa", 2.25, 2, 599, "20240102", .F. }, ;
      { "bb", NIL, 3, 1, "", NIL }, { "aa", 10, 4, 2, "20240104", .T. }, { "cc", -1, 5, 1, "20240105", .F. }, ;
      { "cc", 0.01, 6, 2, "20240106", .T. }, { "dd", 7, 7, 5, "20240107", .F. } }
   dbCreate( "rdd_sort_t", { { "NAME", "C", 4, 0 }, { "AMT", "N", 7, 2 }, { "QTY", "N", 4, 0 }, { "SM", "N", 3, 0 }, ;
      { "D", "D", 8, 0 }, { "L", "L", 1, 0 } } )
   USE rdd_sort_t EXCLUSIVE NEW
   FOR i := 1 TO Len( a )
      APPEND BLANK
      REPLACE NAME WITH a[ i, 1 ], QTY WITH a[ i, 3 ], SM WITH a[ i, 4 ], D WITH SToD( a[ i, 5 ] )
      IF a[ i, 2 ] != NIL
         REPLACE AMT WITH a[ i, 2 ], L WITH a[ i, 6 ]
      ENDIF
   NEXT
   GO 2
   DELETE
   GO 5
   DELETE
   TOTAL TO rdd_sort_2 ON name FIELDS amt, qty
   ? "total:", Show( "rdd_sort_2" ), Eof()
   TOTAL TO rdd_sort_2 ON name FIELDS sm FOR qty > 2
   ? "total sm for:", Show( "rdd_sort_2" )
   GO 1
   TOTAL TO rdd_sort_2 ON name FIELDS qty WHILE qty < 5
   ? "total while:", Show( "rdd_sort_2" ), RecNo()
   TOTAL TO rdd_sort_2 FIELDS qty
   ? "total one run:", Show( "rdd_sort_2" )
   ErrorBlock( {| e | QOut( "ERR", e:subSystem, e:genCode, e:subCode, e:description, e:operation ), .F. } )
   GO TOP
   TOTAL TO rdd_sort_2 ON Left( name, 1 ) FIELDS sm, qty WHILE RecNo() < 4
   ? "total of the first letter:", Show( "rdd_sort_2" )
   ErrorBlock( {| e | Break( e ) } )
   CLOSE
RETURN

STATIC PROCEDURE Updates()
   LOCAL i
   dbCreate( "rdd_sort_u", { { "NAME", "C", 4, 0 }, { "AMT", "N", 5, 0 }, { "QTY", "N", 3, 0 } } )
   USE rdd_sort_u EXCLUSIVE NEW ALIAS upd
   FOR i := 1 TO 6
      APPEND BLANK
      REPLACE NAME WITH { "aa", "bb", "bb", "dd", "ee", "aa" }[ i ], AMT WITH i
   NEXT
   dbCreate( "rdd_sort_v", { { "NAME", "C", 4, 0 }, { "AMT", "N", 5, 0 }, { "QTY", "N", 3, 0 } } )
   USE rdd_sort_v EXCLUSIVE NEW
   FOR i := 1 TO 5
      APPEND BLANK
      REPLACE NAME WITH { "aa", "bb", "bb", "cc", "ee" }[ i ], AMT WITH i * 100
   NEXT
   UPDATE FROM upd ON name REPLACE amt WITH amt + upd->amt, qty WITH qty + 1
   ? "update:", RecNo(), upd->( Eof() ), Show( "", .T. )
   INDEX ON name TO rdd_sort_vi
   UPDATE FROM upd ON name RANDOM REPLACE amt WITH amt + upd->amt
   ? "update random:", Show( "", .T. )
   CLOSE ALL
RETURN

STATIC PROCEDURE Joins()
   LOCAL i
   dbCreate( "rdd_sort_m", { { "NAME", "C", 4, 0 }, { "AMT", "N", 5, 0 } } )
   USE rdd_sort_m EXCLUSIVE NEW ALIAS mst
   FOR i := 1 TO 3
      APPEND BLANK
      REPLACE NAME WITH { "aa", "bb", "cc" }[ i ], AMT WITH i
   NEXT
   dbCreate( "rdd_sort_d", { { "CODE", "C", 2, 0 }, { "NAME", "C", 4, 0 }, { "QTY", "N", 2, 0 } } )
   USE rdd_sort_d EXCLUSIVE NEW ALIAS det
   FOR i := 1 TO 3
      APPEND BLANK
      REPLACE CODE WITH "x" + Str( i, 1 ), NAME WITH { "aa", "cc", "aa" }[ i ], QTY WITH i * 10
   NEXT
   SELECT mst
   JOIN WITH det TO rdd_sort_j FIELDS det->qty, name, det->code FOR name = det->name
   ? "join:", Show( "rdd_sort_j" ), Eof(), det->( Eof() ), Alias()
   JOIN WITH det TO rdd_sort_j
   ? "join all:", Show( "rdd_sort_j" )
   CLOSE ALL
RETURN

STATIC PROCEDURE Deliberate()
   LOCAL i, e
   dbCreate( "rdd_sort_t", { { "NAME", "C", 4, 0 }, { "QTY", "N", 4, 0 }, { "NOTE", "M", 10, 0 } } )
   USE rdd_sort_t EXCLUSIVE NEW
   FOR i := 1 TO 7
      APPEND BLANK
      REPLACE NAME WITH { "aa", "aa", "bb", "aa", "cc", "cc", "dd" }[ i ], QTY WITH i, NOTE WITH Replicate( "n", i )
   NEXT
   GO 1
   TOTAL TO rdd_sort_2 ON name FIELDS qty NEXT 3
   ? "total next 3:", Show( "rdd_sort_2" ), RecNo()
   TOTAL TO rdd_sort_2 ON name FIELDS qty RECORD 3
   ? "total record 3:", Show( "rdd_sort_2" ), RecNo()
   ErrorBlock( {| e | Break( e ) } )
   BEGIN SEQUENCE
      TOTAL TO rdd_sort_3 ON name FIELDS name
   RECOVER USING e
      ? "total of a character field:", e:subSystem, e:subCode, File( "rdd_sort_3.dbf" )
   END SEQUENCE
   TOTAL TO rdd_sort_3 ON name FIELDS field->qty
   ? "total field->:", Show( "rdd_sort_3" )
   __dbSort( "rdd_sort_4", { "qty/D" }, "qty > 3" )
   ? "sort memo, for as text:", Show( "rdd_sort_4" )
   ErrorBlock( {| e | QOut( "ERR", e:subSystem, e:genCode, e:subCode ), .F. } )
   ? "sort onto itself:", __dbSort( "rdd_sort_t", { "name" } ), Used(), Alias(), LastRec()
   ? "total onto itself:", __dbTotal( "rdd_sort_t", {|| NAME }, { "qty" } ), Used(), LastRec()
   dbCreate( "rdd_sort_d", { { "CODE", "C", 2, 0 }, { "NAME", "C", 4, 0 } } )
   USE rdd_sort_d EXCLUSIVE NEW ALIAS det
   APPEND BLANK
   REPLACE CODE WITH "x1", NAME WITH "dd"
   SELECT rdd_sort_t
   ? "join onto itself:", __dbJoin( "det", "rdd_sort_d", { "name" } ), Alias(), det->( LastRec() )
   JOIN WITH det TO rdd_sort_5 FIELDS det->name, det->code, qty FOR qty < 3
   ? "join of the other's name:", Show( "rdd_sort_5" )
   JOIN WITH det TO rdd_sort_5 FIELDS name, det->name, det->code FOR qty < 3
   ? "join of a name twice:", Show( "rdd_sort_5" )
   Hostile()
   ErrorBlock( {| e | Break( e ) } )
   BEGIN SEQUENCE
      UPDATE FROM nosuch ON name REPLACE qty WITH 0
   RECOVER USING e
      ? "update from no alias:", e:subSystem, e:subCode, e:canRetry, Alias()
   END SEQUENCE
   CLOSE ALL
RETURN

// Blocks that undo what SORT and TOTAL stand on: one closes the new file, one changes the keys' array, one puts
// another table in the new file's area; a sum written after a memo field the new file leaves out; numbers of 18
// digits as keys.
STATIC PROCEDURE Hostile()
   LOCAL aKeys := { { 1, .F., .F. } }, nOld := Select(), i
   dbCreate( "rdd_sort_h", { { "NOTE", "M", 10, 0 }, { "NAME", "C", 4, 0 }, { "MEMO2", "M", 10, 0 }, ;
      { "QTY", "N", 4, 0 }, { "BIG", "N", 18, 0 } } )
   USE rdd_sort_h EXCLUSIVE NEW
   FOR i := 1 TO 4
      APPEND BLANK
      REPLACE NAME WITH { "aa", "aa", "bb", "bb" }[ i ], QTY WITH i, NOTE WITH "n", ;
         BIG WITH { 100000000000000001, 100000000000000000, -5, 7 }[ i ]
   NEXT
   TOTAL TO rdd_sort_6 ON name FIELDS qty
   ? "sums after a memo:", Show( "rdd_sort_6" )
   GO 3
   REPLACE QTY WITH 9000
   GO 4
   REPLACE QTY WITH 9000
   ErrorBlock( {| e | QOut( "ERR", e:subSystem, e:genCode, e:subCode, e:description, e:operation ), .F. } )
   TOTAL TO rdd_sort_6 ON name FIELDS qty
   ? "a sum too wide:", Show( "rdd_sort_6" )
   ErrorBlock( {| e | Break( e ) } )
   SORT TO rdd_sort_7 ON big
   ? "18 digits:", Show( "rdd_sort_7" )
   TOTAL TO rdd_sort_8 ON ( iif( RecNo() == 3, Select( "" ), NIL ), Closed( "rdd_sort_8" ), name ) FIELDS qty
   ? "a block closes the new file:", Used(), Alias()
   __dbSortTo( "rdd_sort_9", NIL, { { 2, .F., .F. } }, {|| AAdd( aKeys, { 1, .F., .F. } ), .T. } )
   ? "the keys changed meanwhile:", Show( "rdd_sort_9" )
   CLOSE
   SELECT ( nOld )
RETURN

// Closes the area of the new file a command is writing (the lowest one without an alias past the current one).
STATIC FUNCTION Closed( cFile )
   LOCAL n
   FOR n := Select() + 1 TO 250
      IF ( n )->( Used() ) .AND. Empty( ( n )->( Alias() ) )
         ( n )->( dbCloseArea() )
         EXIT
      ENDIF
   NEXT
RETURN cFile

// A table's fields and records, read back (an open one with lOpen).
STATIC FUNCTION Show( cTable, lOpen )
   LOCAL cOut := "", i, nOld := Select()
   IF lOpen == .T.
      nOld := 0
   ELSE
      USE ( cTable ) NEW
   ENDIF
   FOR i := 1 TO FCount()
      cOut += FieldName( i ) + " "
   NEXT
   GO TOP
   DO WHILE ! Eof()
      cOut += "|" + iif( Deleted(), "*", "" )
      FOR i := 1 TO FCount()
         cOut += RTrim( Transform( FieldGet( i ), "" ) ) + ","
      NEXT
      SKIP
   ENDDO
   IF nOld != 0
      CLOSE
      SELECT ( nOld )
   ENDIF
RETURN cOut
