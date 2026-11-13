// Every field type and width of a table (characters, numbers with and without decimals, narrow ones, dates,
// logicals, memos) with values at their limits (too long, blank, NUL and bytes above 127, rounding halves, too
// many digits, negatives that do not fit, empty dates, other types): each value written with FieldPut() and with
// REPLACE, then read back, with the bytes the record holds on disk. An error shows as its subsystem and code.
PROCEDURE Main()
   LOCAL aStruct := { { "C10", "C", 10, 0 }, { "C1", "C", 1, 0 }, { "N5", "N", 5, 0 }, ;
      { "N8D2", "N", 8, 2 }, { "N3", "N", 3, 0 }, { "N10D4", "N", 10, 4 }, ;
      { "N19", "N", 19, 0 }, { "N1", "N", 1, 0 }, { "N4D2", "N", 4, 2 }, ;
      { "D", "D", 8, 0 }, { "L", "L", 1, 0 }, { "M", "M", 10, 0 } }
   LOCAL aVal := { "abc", "  padded  ", Replicate( "x", 12 ), "", "é" + Chr( 0 ) + "z", 0, ;
      -1, 1.5, 12.345, 999, 1000, -99, -100, 0.005, ;
      0.0049, -0.004, 123456789, 9999999999999999999, 1 / 3, SToD( "20240229" ), SToD( "" ), .T., .F., NIL, { 1 } }
   LOCAL i, x, nOffset
   ErrorBlock( {| e | Break( e ) } )
   dbCreate( "rdd_field_values", aStruct )
   USE rdd_field_values EXCLUSIVE
   APPEND BLANK
   nOffset := 1
   FOR i := 1 TO Len( aStruct )
      ? aStruct[ i, 1 ] + ":"
      FOR EACH x IN aVal
         ?? " " + Put( i, x, nOffset, aStruct[ i, 3 ] )
      NEXT
      nOffset += aStruct[ i, 3 ]
   NEXT
   ? "replace:"
   ?? " " + Shown( {|| ReplaceNamed( "N8D2", 12.345 ) } ), Shown( {|| ReplaceNamed( "C1", "xyz" ) } ), ;
      Shown( {|| ReplaceNamed( "L", "T" ) } ), Shown( {|| ReplaceNamed( "D", 20240229 ) } )
   CLOSE
   FErase( "rdd_field_values.dbf" )
   FErase( "rdd_field_values.dbt" )
RETURN

// REPLACE of one field, by its name.
FUNCTION ReplaceNamed( cName, x )
   REPLACE &cName WITH x
RETURN FieldGet( FieldPos( cName ) )

// Writes x into field n (which starts at nOffset in the record, nLen bytes long) and shows what it reads back and
// what the file holds, or the error.
STATIC FUNCTION Put( n, x, nOffset, nLen )
   LOCAL cErr := ""
   BEGIN SEQUENCE
      FieldPut( n, x )
   RECOVER USING x
      cErr := x:subSystem + "/" + LTrim( Str( x:subCode ) )
   END SEQUENCE
   dbCommit()
RETURN cErr + Shown( {|| FieldGet( n ) } ) + "=" + Raw( nOffset, nLen )

// The bytes of the field in the first record of the file.
STATIC FUNCTION Raw( nOffset, nLen )
   LOCAL h := FOpen( "rdd_field_values.dbf" ), cHead := Space( 12 ), cField, nHeader
   FRead( h, @cHead, 12 )
   nHeader := Asc( SubStr( cHead, 9, 1 ) ) + 256 * Asc( SubStr( cHead, 10, 1 ) )
   cField := Space( nLen )
   FSeek( h, nHeader + nOffset )
   FRead( h, @cField, nLen )
   FClose( h )
RETURN "'" + Visible( cField ) + "'"

STATIC FUNCTION Shown( b )
   LOCAL x, e
   BEGIN SEQUENCE
      x := Eval( b )
   RECOVER USING e
      RETURN e:subSystem + "/" + LTrim( Str( e:subCode ) )
   END SEQUENCE
   SWITCH ValType( x )
   CASE "N"
      RETURN "N" + Str( x )
   CASE "C"
   CASE "M"
      RETURN "C[" + Visible( x ) + "]"
   CASE "D"
      RETURN "D" + DToS( x )
   CASE "L"
      RETURN iif( x, "T", "F" )
   ENDSWITCH
RETURN ValType( x )

// Bytes outside 32-126 as <n>.
STATIC FUNCTION Visible( c )
   LOCAL r := "", i, n
   FOR i := 1 TO Len( c )
      n := Asc( SubStr( c, i, 1 ) )
      r += iif( n < 32 .OR. n > 126, "<" + LTrim( Str( n ) ) + ">", Chr( n ) )
   NEXT
RETURN r
