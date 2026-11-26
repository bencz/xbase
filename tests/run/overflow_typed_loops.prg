// Integer operators that overflow into doubles in loops whose operands are known numbers (guarded where they enter):
// + - * ++ -- and unary minus past the 64-bit range on every pass, each giving the double the operator gives (the
// same computed from the numbers' text by the macro compiler); and a loop that overflows on every pass while another
// thread allocates and collects: the loop reaches the collector's safepoints through its back edge's polls, its
// overflow's call needing no test of its own (no request can come of two numbers).
PROCEDURE Main( cPasses )
   LOCAL n := IIf( Empty( cPasses ), 20000, Val( cPasses ) ), big := 9223372036854775000, x, i, cWant, oTask, nDone

   x := Sums( big, n )
   cWant := Str( &( LTrim( Str( big ) ) + " + " + LTrim( Str( n ) ) + " * 1000" ), 30, 0 )
   ? "add:", Str( x, 30, 0 ) == cWant, ValType( x )
   ? "sub:", Str( Differences( -big, n ), 30, 0 ) == Str( -big - n * 1000, 30, 0 )
   ? "mul:", Str( Products( 3037000500, n ), 30, 0 ) == Str( 3037000500 * 3037000500, 30, 0 )
   ? "inc:", Str( Steps( big + 800, 100, .T. ), 30, 0 ), Str( Steps( -big - 800, 100, .F. ), 30, 0 )
   ? "neg:", Str( Negated( n ), 30, 0 ) == Str( 9223372036854775808, 30, 0 )

   oTask := TaskRun( {|| Churn( 300 ) } )
   nDone := 0
   FOR i := 1 TO 200
      nDone += IIf( Sums( big, 2000 ) > big, 1, 0 )
   NEXT
   ? "with a collector:", nDone, oTask:Result
RETURN

STATIC FUNCTION Sums( base, n )
   LOCAL i, s
   FOR i := 1 TO n
      s := base + i * 1000
   NEXT
RETURN s

STATIC FUNCTION Differences( base, n )
   LOCAL i, s
   FOR i := 1 TO n
      s := base - i * 1000
   NEXT
RETURN s

STATIC FUNCTION Products( k, n )
   LOCAL i, p
   FOR i := 1 TO n
      p := k * k
   NEXT
RETURN p

STATIC FUNCTION Steps( x, n, lUp )
   LOCAL i
   FOR i := 1 TO n
      IF lUp
         x++
      ELSE
         x--
      ENDIF
   NEXT
RETURN x

STATIC FUNCTION Negated( n )
   LOCAL i, m := -9223372036854775807 - 1, r
   FOR i := 1 TO n
      r := -m
   NEXT
RETURN r

STATIC FUNCTION Churn( n )
   LOCAL i, a
   FOR i := 1 TO n
      a := Array( 1000 )
      AFill( a, Replicate( "x", 20 ) )
      IF i % 50 == 0
         __GcCollect()
      ENDIF
   NEXT
RETURN n
