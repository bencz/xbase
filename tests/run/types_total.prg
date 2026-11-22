// The RESULT types, ranges and TOTAL of include/rtl.ch where a call's arguments are not of its parameters' types: the
// call fails and the error block's value goes on, of any type or outside the RANGE, through what computes with it in
// a loop the compiler types (Asc( SubStr() ) as base64 reads, a product the range would keep from overflowing, a FOR
// limit, a concatenation split into appends, a comparison), and calls whose arguments are of their types in between.
PROCEDURE Main()
   LOCAL bOld := ErrorBlock( {|| 1099511627776 } )
   ? "codes:", Codes( { "abc", 12, NIL, "xyz", "" } )
   ? "products:", Products( { "a", .T., "b" } )
   ? "lengths:", Lengths( { "ab", { 1, 2, 3 }, 7, { "k" => 1 } } )
   ? "dates:", Dates( { SToD( "20240229" ), "x", SToD( "19991231" ) } )
   ErrorBlock( {|| -3 } )
   ? "limit:", Limit( { "abcd", 5, "xy" } )
   ? "signs:", Signs( { 5, "n", -2.5, 0 } )
   ErrorBlock( {|| "zz" } )
   ? "joined:", Joined( { 65, "B", 67 } )
   ? "codes:", Codes( { "abc", 12 } )
   ErrorBlock( {|| 2.5 } )
   ? "products:", Products( { "a", 1, "b" } )
   ErrorBlock( bOld )
RETURN

STATIC FUNCTION Codes( a )
   LOCAL i, n := 0, s := ""
   FOR i := 1 TO Len( a )
      n += Asc( SubStr( a[ i ], 1, 1 ) ) * 65536 + Asc( SubStr( a[ i ], 2, 1 ) )
      s += Str( n ) + ";"
   NEXT
RETURN s

STATIC FUNCTION Products( a )
   LOCAL i, x, s := ""
   FOR i := 1 TO Len( a )
      x := Asc( a[ i ] ) * 16777216 * 16777216
      s += LTrim( Str( x / 281474976710656, 20, 2 ) ) + iif( Asc( a[ i ] ) > 255, ">", "<" ) + " "
   NEXT
RETURN s

STATIC FUNCTION Lengths( a )
   LOCAL i, n := 0
   FOR i := 1 TO Len( a )
      n += Len( a[ i ] ) * 4611686018427387904 / 4611686018427387904
   NEXT
RETURN n

STATIC FUNCTION Dates( a )
   LOCAL i, s := ""
   FOR i := 1 TO Len( a )
      s += Str( Day( a[ i ] ) * 100 + Month( a[ i ] ) ) + Str( Year( a[ i ] ) + DoW( a[ i ] ) )
   NEXT
RETURN s

STATIC FUNCTION Limit( a )
   LOCAL i, j, n := 0
   FOR i := 1 TO Len( a )
      FOR j := 1 TO Len( a[ i ] )
         n++
      NEXT
      n += Len( a[ i ] )
   NEXT
RETURN n

STATIC FUNCTION Signs( a )
   LOCAL i, n := 0
   FOR i := 1 TO Len( a )
      n := n * 10 + Sign( a[ i ] ) + 5
   NEXT
RETURN n

STATIC FUNCTION Joined( a )
   LOCAL i, s := ""
   FOR i := 1 TO Len( a )
      s += Chr( a[ i ] ) + Upper( Chr( a[ i ] + 32 ) ) + Lower( Left( Chr( a[ i ] ), 1 ) )
   NEXT
RETURN s
