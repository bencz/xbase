// CHOOSES (Max(), Min()): a call that raises no error returns one of its arguments as it received it, so its result
// has their types (integers an integer, an integer and a double either, dates a date) and their range; a value an
// error block substitutes goes on as it is. Int( x / k ) of integers past 2^53 keeps a range (the double rounding's
// slack) that makes what computes with it an integer where it fits, and a double where it does not (Int() of
// 9223372036854775807 / 2 is 2^62: its sum with 2^62 does not fit, though the quotient's exact range would).
PROCEDURE Main()
   LOCAL i, n := 0, a := { 3, 2.5, 7 }, d
   FOR i := 1 TO 5
      n += Max( i, 3 ) * 4611686018427387 + Min( i, 2 )
   NEXT
   ? "integers:", n
   FOR i := 1 TO 3
      ? "mixed:", Max( a[ i ], 2 ), Min( a[ i ], 2.75 ), ValType( Max( a[ i ], 2 ) )
   NEXT
   d := SToD( "20240229" )
   ? "dates:", DToS( Max( d, d + 1 ) ), DToS( Min( d, d - 1 ) )
   ? "limits:", Limits( 9 )
   ErrorBlock( {|| 9223372036854775807 } )
   FOR i := 1 TO 3
      n := Max( iif( i == 2, "x", i ), 1 ) + 1
      ? "substituted:", n
   NEXT
   ? "halves:", Halves( 9223372036854775807 ), Halves( -9223372036854775807 ), Halves( 9007199254740993 )
RETURN

STATIC FUNCTION Limits( n )
   LOCAL i, j, s := 0, nRange := Max( Int( Max( n, 4 ) / 2 ) - 1, 0 )
   FOR i := 1 TO n
      FOR j := Max( 1, i - nRange ) TO Min( n, i + nRange )
         s += j
      NEXT
   NEXT
RETURN s

STATIC FUNCTION Halves( v )
   LOCAL i, x, q, s := ""
   FOR i := 1 TO 1 // x an integer past BitOr()'s check, of any range
      x := BitOr( v, 0 )
      q := Max( Int( x / 2 ) - 1, Int( x / 3 ) + 1 )
      s += Str( q * 2 + 1, 22 ) + Str( Int( x / 4 ) + 4611686018427387904, 22 ) + ;
         Str( Int( x / 2 ) + 4611686018427387904, 22 ) + Str( ( Int( x / 2 ) + 4611686018427387904 ) % 1000, 6 ) + ;
         Str( Int( ( Int( x / 2 ) + 4611686018427387904 ) / 3 ), 22 ) + ;
         Str( Int( x / 2 ) + 4611686018427387904 - 1, 22 ) + Str( ( Int( x / 2 ) + 4611686018427387904 ) * 1, 22 )
   NEXT
RETURN s
