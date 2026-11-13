// ASort of numbers without a block (all integers, or all doubles: a radix sort from 256 elements) gives the order and
// the stability of the sort with {| x, y | x < y }: equal values (one written with more decimals, -0.0 and 0.0) keep
// their order, negative values and the 64-bit ends sort right, and a range sorts alone. With a NaN the elements keep
// the comparison sort's order.
PROCEDURE Main()
   LOCAL n
   FOR EACH n IN { 10, 300, 5000 }
      ? n, Same( Ints( n ) ), Same( Doubles( n ) ), Same( Ranged( n ) )
   NEXT
   ? Show( Sorted( { 3.10, 3.1, -0.5, 3.100, 0.0, -0.0, -7.25, 3.1000 } ) )
   ? Show( Sorted( Cycle( { 3.10, 3.1, -0.5, 3.100, 0.0, -0.0, -7.25, 3.1000 }, 320 ) ), 200 )
   ? Show( Sorted( Edges() ) )
   ? Show( SortedNaN() )
RETURN

// Whether ASort without a block and with {| x, y | x < y } give the same elements in the same order (their display
// tells equal values written differently apart).
STATIC FUNCTION Same( a )
   LOCAL b := AClone( a ), i
   ASort( a )
   ASort( b, , , {| x, y | x < y } )
   FOR i := 1 TO Len( a )
      IF ! Str( a[ i ], 30, 4 ) == Str( b[ i ], 30, 4 ) .OR. ! Transform( a[ i ], "" ) == Transform( b[ i ], "" )
         RETURN "differ at " + LTrim( Str( i ) )
      ENDIF
   NEXT
RETURN "same"

STATIC FUNCTION Ints( n )
   LOCAL a := Array( n ), i, x := 99
   FOR i := 1 TO n
      x := ( x * 48271 ) % 2147483647
      a[ i ] := Int( x ) - 1073741823
   NEXT
RETURN a

STATIC FUNCTION Doubles( n )
   LOCAL a := Array( n ), i, x := 7
   FOR i := 1 TO n
      x := ( x * 48271 ) % 2147483647
      a[ i ] := iif( i % 3 == 0, Round( ( x % 1000 ) / 8, 3 ), ( x % 2000 ) / 16 - 50 )
   NEXT
RETURN a

STATIC FUNCTION Ranged( n )
   LOCAL a := Doubles( n ), b := AClone( a )
   ASort( a, 3, Int( n / 2 ) )
   ASort( b, 3, Int( n / 2 ), {| x, y | x < y } )
RETURN iif( AEq( a, b ), a, { "range differs" } )

STATIC FUNCTION AEq( a, b )
   LOCAL i
   FOR i := 1 TO Len( a )
      IF ! Transform( a[ i ], "" ) == Transform( b[ i ], "" )
         RETURN .F.
      ENDIF
   NEXT
RETURN .T.

STATIC FUNCTION Edges()
   LOCAL a := {}, i
   FOR i := 1 TO 300
      AAdd( a, iif( i % 3 == 0, 9223372036854775807 - i, iif( i % 3 == 1, -9223372036854775807 + i, i - 150 ) ) )
   NEXT
   AAdd( a, -9223372036854775807 - 1 )
RETURN a

STATIC FUNCTION Sorted( a )
   LOCAL b := AClone( a )
   ASort( a )
   ASort( b, , , {| x, y | x < y } )
RETURN iif( AEq( a, b ), a, { "differs" } )

STATIC FUNCTION SortedNaN()
   LOCAL a := {}, i, nan := Log( -1 )
   FOR i := 1 TO 300
      AAdd( a, iif( i == 150, nan, ( 300 - i ) / 4 ) )
   NEXT
RETURN Sorted( a )

STATIC FUNCTION Cycle( aValues, n )
   LOCAL a := Array( n ), i
   FOR i := 1 TO n
      a[ i ] := aValues[ ( i - 1 ) % Len( aValues ) + 1 ]
   NEXT
RETURN a

STATIC FUNCTION Show( a, nFrom )
   LOCAL s := "", i
   IF nFrom == NIL
      nFrom := 1
   ENDIF
   FOR i := nFrom TO Min( Len( a ), nFrom + 8 )
      s += LTrim( Transform( a[ i ], "" ) ) + " "
   NEXT
RETURN s + "(" + LTrim( Str( Len( a ) ) ) + ")"
