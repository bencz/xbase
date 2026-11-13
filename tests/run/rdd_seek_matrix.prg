// SEEK over orders of a character, a number and a date, ascending and DESCENDING, in NTX and CDX files, of a table
// with equal keys, blank and empty values, negative and fractional numbers: each key of a list (whole, partial,
// longer, blank, between and past the keys, of another case) sought plainly, with SOFTSEEK, with LAST and with
// both; each shows the record found (0 past the end), Found() and Eof(), then the next record SKIP gives. The files
// start with "rdd_seek_matrix" and are deleted at the end.
REQUEST DBFCDX

PROCEDURE Main()
   LOCAL cRdd, cKey, lDesc
   FOR EACH cRdd IN { "DBFNTX", "DBFCDX" }
      dbCreate( "rdd_seek_matrix", { { "C", "C", 6, 0 }, { "N", "N", 8, 2 }, { "D", "D", 8, 0 } }, cRdd )
      USE rdd_seek_matrix VIA cRdd EXCLUSIVE
      APPEND BLANK
      REPLACE C WITH "abc", N WITH 10, D WITH SToD( "20240229" )
      APPEND BLANK
      REPLACE C WITH "ab", N WITH -5.5, D WITH SToD( "" )
      APPEND BLANK
      REPLACE C WITH "abc", N WITH 0, D WITH SToD( "19991231" )
      APPEND BLANK
      REPLACE C WITH " a", N WITH 10, D WITH SToD( "20240229" )
      APPEND BLANK
      REPLACE C WITH "", N WITH 1000.25, D WITH SToD( "20000101" )
      APPEND BLANK
      REPLACE C WITH "abd", N WITH -5.5, D WITH SToD( "" )
      APPEND BLANK
      REPLACE C WITH "b", N WITH 0.01, D WITH SToD( "21000101" )
      APPEND BLANK
      REPLACE C WITH "ABC", N WITH -1000, D WITH SToD( "19000101" )
      APPEND BLANK
      REPLACE C WITH "abc", N WITH 10, D WITH SToD( "20240301" )
      FOR EACH cKey IN { "C", "N", "D" }
         FOR EACH lDesc IN { .F., .T. }
            Probe( cRdd, cKey, lDesc )
         NEXT
      NEXT
      CLOSE
      FErase( "rdd_seek_matrix.dbf" )
      FErase( "rdd_seek_matrix.ntx" )
      FErase( "rdd_seek_matrix.cdx" )
   NEXT
RETURN

STATIC PROCEDURE Probe( cRdd, cKey, lDesc )
   LOCAL x, aKeys
   IF lDesc
      INDEX ON &cKey TO rdd_seek_matrix DESCENDING
   ELSE
      INDEX ON &cKey TO rdd_seek_matrix
   ENDIF
   DO CASE
   CASE cKey == "C"
      aKeys := { "abc", "ab", "a", "abcd", " ", "", "aa", "c", "ABC", " a", "abc   " }
   CASE cKey == "N"
      aKeys := { 10, -5.5, 0, 5, -1001, 2000, 0.01, 10.001, -0 }
   OTHERWISE
      aKeys := { SToD( "20240229" ), SToD( "" ), SToD( "20000101" ), ;
         SToD( "20240300" ), SToD( "20250101" ), SToD( "18000101" ) }
   ENDCASE
   ? cRdd, cKey, iif( lDesc, "descending", "ascending" ), Order()
   FOR EACH x IN aKeys
      ? ""
      ?? Sought( x, .F., .F. ), Sought( x, .T., .F. ), Sought( x, .F., .T. ), Sought( x, .T., .T. )
   NEXT
   ordListClear()
RETURN

// The records in the order.
STATIC FUNCTION Order()
   LOCAL c := ""
   GO TOP
   DO WHILE ! Eof()
      c += LTrim( Str( RecNo() ) ) + " "
      SKIP
   ENDDO
RETURN c

STATIC FUNCTION Sought( x, lSoft, lLast )
   LOCAL c
   dbSeek( x, lSoft, lLast )
   c := LTrim( Str( iif( Eof(), 0, RecNo() ) ) ) + iif( Found(), "F", "-" ) + iif( Eof(), "E", "-" )
   SKIP
RETURN c + ">" + LTrim( Str( iif( Eof(), 0, RecNo() ) ) )
