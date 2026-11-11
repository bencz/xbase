// Int() keeps its argument's display width: a field's integer columns (N(6,2): 3, N(6): 6, N(20,4): 15), a number's
// own width otherwise (a literal's, a computed double's); an integer comes back as it is. The table is
// "int_width.dbf", deleted at the end.
FIELD A, B, C
PROCEDURE Main()
   LOCAL x := 1234.5678, y := 7, z := 123456789012.75, w := 3 / 2
   dbCreate( "int_width", { { "A", "N", 6, 2 }, { "B", "N", 6, 0 }, { "C", "N", 20, 4 } } )
   USE int_width NEW
   APPEND BLANK
   REPLACE A WITH 12.34, B WITH 77, C WITH 123456789012.5
   ? "[" + Str( Int( 3.7 ) ) + "]", "[" + Str( Int( x ) ) + "]", "[" + Str( Int( y ) ) + "]", ;
     "[" + Str( Int( z ) ) + "]"
   ? "[" + Str( Int( A ) ) + "]", "[" + Str( Int( B ) ) + "]", "[" + Str( Int( C ) ) + "]", ;
     "[" + Str( Int( -3.5 ) ) + "]"
   ? "[" + Str( Int( w ) ) + "]", "[" + Str( Int( 10 ^ 20 ) ) + "]", Int( 3.7 ), Int( A ), Int( C )
   CLOSE
   FErase( "int_width.dbf" )
RETURN
