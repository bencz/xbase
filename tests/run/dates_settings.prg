// SET DATE / CENTURY / EPOCH commands and more date formats.
PROCEDURE Main()
   LOCAL d := SToD( "20240305" ), f, a := { "yyy", "yyyyyy", "dmy", "mmddyy", "dd/mm/yy/dd", "Y-M-D", "xx" }
   SET DATE BRITISH
   ? d, Set( _SET_DATEFORMAT ), __SetCentury()
   SET CENTURY ON
   ? d, Set( _SET_DATEFORMAT ), __SetCentury()
   SET DATE ANSI
   ? d
   SET CENTURY OFF
   ? d, Set( _SET_DATEFORMAT )
   SET DATE GERMAN
   ? d
   SET DATE TO ITALIAN
   ? d
   SET DATE JAPANESE
   ? d
   SET DATE USA
   ? d
   SET DATE FORMAT TO "dd mmm yyyy"
   ? d, __SetCentury()
   SET DATE AMERICAN
   SET EPOCH TO 1950
   ? DToS( CToD( "01/01/49" ) ), DToS( CToD( "01/01/50" ) ), Set( _SET_EPOCH )
   SET DATE AMERICAN
   ShowFormats( d, a )
   RETURN

STATIC PROCEDURE ShowFormats( d, a )
   LOCAL i
   FOR i := 1 TO Len( a )
      Set( _SET_DATEFORMAT, a[ i ] )
      ? a[ i ], "[" + DToC( d ) + "]", "[" + DToC( CToD( "" ) ) + "]"
   NEXT
   RETURN
