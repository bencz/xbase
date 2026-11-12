// Date type: arithmetic, comparisons, functions, SET DATE FORMAT pictures, CToD parsing, SET EPOCH.
PROCEDURE Main()
   LOCAL d := SToD( "20240305" ), e := CToD( "" ), f, a := { "mm/dd/yy", "dd/mm/yyyy", "yyyy.mm.dd", "ddd mmm yyy", ;
      "dddd/mmmm/yyyyy", "d/m/y", "yy-mm-dd", "dd.mm.yyyy xx" }, i
   ? d, e, ValType( d ), DToS( d ), DToS( e ), Len( DToC( e ) )
   ? d + 1, d - 1, 1 + d, d - SToD( "20240101" ), d + 1.9, d > e, Empty( e )
   ? Year( d ), Month( d ), Day( d ), DoW( d ), CDoW( d ), CMonth( d ), Year( e ), DoW( e ), CDoW( e ), ;
      "|" + CMonth( e ) + "|"
   FOR i := 1 TO Len( a )
      Set( 4, a[ i ] )
      ? a[ i ], "[" + DToC( d ) + "]", "[" + DToC( e ) + "]", DToS( CToD( DToC( d ) ) )
   NEXT
   Set( 4, "mm/dd/yy" )
   ? CToD( "02/29/24" ), CToD( "02/30/24" ), CToD( "13/01/24" ), CToD( "1/2/3" ), CToD( "12/31/49" ), ;
      CToD( "12/31/50" ), CToD( "  /  /  " )
   Set( 5, 1980 )
   ? CToD( "12/31/49" ), CToD( "12/31/79" ), CToD( "12/31/80" ), Set( 5 )
   Set( 4, "mm/dd/yyyy" )
   ? CToD( "3/5/2024" ), CToD( "03/05/24" ), CToD( "3-5-2024" ), CToD( "3 5 2024" ), SToD( "2024" ), SToD( "20241301" )
   f := SToD( "00010101" )
   ? DToS( f ), DToS( SToD( "99991231" ) ), DToS( SToD( "99991231" ) + 1 ), DToS( f - 1 )
   RETURN
