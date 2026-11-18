// Values whose type changes in the middle of a loop, where the compiler types loops (ranges of integers, integers
// speculated with a guard, typed copies of functions): a FOR counter assigned a double, then an integer again, a string
// (the error at NEXT, handled), a value past the limit; a limit and a step that change in the loop; a sum and a product
// that overflow into doubles mid-loop; Int( x / k ) of large and negative numbers; elements of an array of integers,
// doubles, a string (the error, handled) and NIL; a WHILE variable that turns double; a comparison bound by an IF on a
// double and on a string; a recursive function called with an integer, a double and a string; the same loop with an
// integer start and then a double one; counters at the ends of the integers, whose own increment, and a sum with them,
// overflow into doubles, up and down (a counter past the end is a double equal to the limit, which adding 1 no longer
// changes: the loop leaves with EXIT after a few passes), and a comparison at those ends; a product that fits on an
// IF's branch that bounds its operand and overflows on the other.
PROCEDURE Main()
   LOCAL i, x, n, s, a, k, r
   s := ""
   FOR i := 1 TO 10
      x := i
      i := x * 0.10
      i := x
      s += LTrim( Str( i ) ) + " "
   NEXT
   ? "reassigned:", s, i
   s := ""
   FOR i := 1 TO 6
      s += Str( i, 6, 2 ) + " "
      i := i + 0.5
   NEXT
   ? "turned double:", s, i
   ? "string counter:", Try( {|| StringCounter() } )
   s := ""
   FOR i := 1 TO 100
      s += LTrim( Str( i ) ) + " "
      IF i == 3
         i := 1000
      ENDIF
   NEXT
   ? "past the limit:", s, i
   n := 10
   s := ""
   FOR i := 1 TO n
      s += LTrim( Str( i ) ) + " "
      n--
   NEXT
   ? "limit changed:", s, i, n
   k := 1
   s := ""
   FOR i := 1 TO 20 STEP k
      s += LTrim( Str( i ) ) + " "
      k++
   NEXT
   ? "step changed:", s, i
   n := 0
   FOR i := 1 TO 6
      n := n + 4611686018427387904
   NEXT
   ? "sum overflows:", n, ValType( n ), n == 6 * 4611686018427387904
   x := 1
   FOR i := 1 TO 5
      x := x * 1000000
   NEXT
   ? "product overflows:", x, ValType( x )
   s := ""
   FOR EACH x IN { 7, -7, 2 ^ 60, -( 2 ^ 60 ), 9007199254740993, 1000000000000000000 }
      s += LTrim( Str( Int( x / 16 ) ) ) + " "
   NEXT
   ? "Int( x / 16 ):", s
   ? "elements:", Elements( { 1, 2, 3 } ), Elements( { 1, 2.5, 3 } ), Elements( { 1.5, 2.5 } ), ;
     Try( {|| Elements( { 1, "a", 3 } ) } ), Try( {|| Elements( { 1, NIL } ) } )
   i := 1
   s := ""
   DO WHILE i < 10
      s += LTrim( Str( i ) ) + " "
      i := iif( i == 5, 5.5, i + 1 )
   ENDDO
   ? "while turns double:", s, i
   ? "bounded by IF:", Halves( 7 ), Halves( 7.5 ), Try( {|| Halves( "7" ) } )
   ? "recursive:", Fib( 20 ), Fib( 20.5 ), Try( {|| Fib( "20" ) } )
   r := {}
   FOR EACH x IN { 1, 1.5 }
      n := 0
      FOR i := x TO 4
         n := n + i * 2
      NEXT
      AAdd( r, n )
   NEXT
   ? "integer start, then double:", r[ 1 ], r[ 2 ]
   s := ""
   k := 0
   FOR i := 9223372036854775805 TO 9223372036854775807
      x := i + 1
      s += ValType( i ) + ValType( x ) + LTrim( Str( x - i ) ) + " " // 1 between integers, 0 past the end
      IF ++k == 5
         EXIT
      ENDIF
   NEXT
   ? "up to the end:", s, k
   s := ""
   k := 0
   FOR i := -9223372036854775806 TO -9223372036854775807 - 1 STEP -1
      s += LTrim( Str( i + 9223372036854775800 ) ) + " "
      IF ++k == 5
         EXIT
      ENDIF
   NEXT
   ? "down to the end:", s, k
   n := 9223372036854775806
   s := ""
   DO WHILE n < 9223372036854775807
      n := n + 1
      s += Str( n - 9223372036854775800, 3 )
   ENDDO
   ? "compared at the end:", s, n + 1 > n
   ? "bounded on one branch:", Bounded()
RETURN

STATIC FUNCTION Bounded()
   LOCAL k, x, n, s := ""
   FOR k := -2 TO Val( "2" ) // a limit the optimizer does not know
      x := k * 9999999999 + 5
      IF x > -1000
         IF x < 1000
            n := x * 1000000000000000
         ELSE
            n := x * 1000000000000000
         ENDIF
      ELSE
         n := x * 1000000000000000
      ENDIF
      n := n - 7
      s += iif( n > 0, "+", "-" ) + LTrim( Str( n / 1000000000000000, 20, 1 ) ) + " "
   NEXT
RETURN s

STATIC FUNCTION StringCounter()
   LOCAL i, n := 0
   FOR i := 1 TO 10
      n++
      IF i == 2
         i := "abc"
      ENDIF
   NEXT
RETURN n

// The sum of the elements, each times 3, with a loop the compiler keeps typed.
STATIC FUNCTION Elements( a )
   LOCAL i, n := 0
   FOR i := 1 TO Len( a )
      n := n + a[ i ] * 3
   NEXT
RETURN n

// Halves n while it is at least 2, counting the steps.
STATIC FUNCTION Halves( n )
   LOCAL c := 0
   DO WHILE !( n < 2 )
      n := n - Int( n / 2 )
      c++
   ENDDO
RETURN Str( c ) + Str( n, 6, 2 )

STATIC FUNCTION Fib( n )
   IF n < 2
      RETURN n
   ENDIF
RETURN Fib( n - 1 ) + Fib( n - 2 )

STATIC FUNCTION Try( b )
   LOCAL x
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      x := Eval( b )
   RECOVER USING x
      x := x:subsystem + "/" + LTrim( Str( x:subCode ) ) + " " + x:description + " " + x:operation
   END SEQUENCE
RETURN x
