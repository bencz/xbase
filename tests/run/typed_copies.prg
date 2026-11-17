// Calls whose arguments are numbers, logicals or dates go to copies of the function typed for them; the results, the
// errors and the call stack are those of the function itself. Recursion on integers, on doubles and across the
// integer limit (a double from there on); a parameter assigned in the body; the same function called with numbers and
// with strings, by reference, through a macro and through a function reference; logicals and dates; an error inside
// a copy, its call stack and a value an error block substitutes (any type, from a typed copy); mutual recursion; a
// copy that returns strings (the collector runs while their results wait); PCount(), which keeps the function whole.
PROCEDURE Main()
   LOCAL x := 10, b, c
   ? "fib", Fib( 20 ), Fib( 20.0 ), Fib( 1.5 ), Fib( 0 ), Fib( -3 ), ValType( Fib( 20.0 ) ), Str( Fib( 25 ) )
   ? "big", Twice( 9223372036854775000, 3 ) > 70000000000000000000, ValType( Twice( 9223372036854775000, 3 ) ), ;
      Twice( 7, 3 ), Twice( 1073741824, 31 ) == 2305843009213693952
   ? "assigned", Digits( 1234567 ), Digits( 0 ), Digits( 98765.4 )
   ? "strings", Repeat( "ab", 3 ), Repeat( 3, 3 ), Repeat( .T., 1 )
   ? "by ref", Bump( @x ), x, Bump( x ), x
   ? "macro", &( "Fib( 10 )" ), Eval( @Fib(), 12 ), Eval( {| n | Fib( n ) }, 11 )
   ? "logical", Flip( .T., 3 ), Flip( .F., 4 ), Flip( NIL, 2 )
   ? "date", DToS( Later( SToD( "20261116" ), 3 ) ), DToS( Later( SToD( "20261231" ), 1 ) )
   ? "mutual", IsEven( 10 ), IsOdd( 7 ), IsEven( 7.0 )
   ? "text", Len( Grow( 2000 ) ), Right( Grow( 2000 ), 4 )
   ? "pcount", Count( 1, 2 ), Count( 1 )
   b := ErrorBlock( {| e | Break( e ) } )
   BEGIN SEQUENCE
      Deep( 3 )
   RECOVER USING c
      ? "error", c:SubCode, c:Operation, c:Description
   END SEQUENCE
   ErrorBlock( {| e | iif( e:canSubstitute, "sub", Break( e ) ) } )
   ? "substituted", Divide( 10, 0 ), Divide( 10, 4 ), ValType( Divide( 1, 0 ) )
   ErrorBlock( b )
   ? "stack", Stack( 2 )
RETURN

FUNCTION Fib( n )
   IF n < 2
      RETURN n
   ENDIF
RETURN Fib( n - 1 ) + Fib( n - 2 )

// Doubles n k times: past the largest integer, a double.
STATIC FUNCTION Twice( n, k )
   IF k == 0
      RETURN n
   ENDIF
RETURN Twice( n * 2, k - 1 )

// The sum of the digits, the parameter divided in the loop.
STATIC FUNCTION Digits( n )
   LOCAL s := 0
   n := Int( n )
   DO WHILE n > 0
      s += n % 10
      n := Int( n / 10 )
   ENDDO
RETURN s

STATIC FUNCTION Repeat( x, k )
   IF k <= 1
      RETURN x
   ENDIF
RETURN x + Repeat( x, k - 1 )

STATIC FUNCTION Bump( n )
   n++
RETURN n

STATIC FUNCTION Flip( l, k )
   IF k == 0
      RETURN l
   ENDIF
RETURN Flip( iif( l == NIL, NIL, ! l ), k - 1 )

STATIC FUNCTION Later( d, k )
   IF k == 0
      RETURN d
   ENDIF
RETURN Later( d + 1, k - 1 )

STATIC FUNCTION IsEven( n )
RETURN iif( n == 0, .T., IsOdd( n - 1 ) )

STATIC FUNCTION IsOdd( n )
RETURN iif( n == 0, .F., IsEven( n - 1 ) )

// A string of n characters, built by halves: each half waits while the other is made.
STATIC FUNCTION Grow( n )
   IF n <= 1
      RETURN Chr( 65 + n )
   ENDIF
RETURN Grow( Int( n / 2 ) ) + Space( 0 ) + Grow( n - Int( n / 2 ) )

STATIC FUNCTION Count( a, b )
   Either( a, b )
RETURN PCount()

STATIC FUNCTION Either( a, b )
RETURN iif( a == NIL, b, a )

STATIC FUNCTION Deep( n )
   IF n == 0
      RETURN n + "x"
   ENDIF
RETURN Deep( n - 1 )

STATIC FUNCTION Divide( a, b )
RETURN a / b

// The call stack inside a typed copy, as the function's own.
STATIC FUNCTION Stack( n )
   LOCAL c := "", k := 0
   IF n > 0
      RETURN Stack( n - 1 )
   ENDIF
   DO WHILE ! Empty( ProcName( k ) )
      c += ProcName( k ) + "(" + LTrim( Str( ProcLine( k ) ) ) + ") "
      k++
   ENDDO
RETURN c
