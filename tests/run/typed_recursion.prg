// Recursion whose results feed further calls (a result passed as an argument: Ackermann's and Takeuchi's functions)
// runs on the typed copies the types of each other's results ask for, with the function's own results, errors and call
// stack: an integer that overflows into a double inside the recursion, an error raised deep in it (ProcName() and
// ProcLine() at every level, a value an error block substitutes, which may be of another type, and a BREAK out of it),
// the same functions called with strings, NIL and too few arguments, through Do(), a macro and a function reference.
// Results that hold no heap reference come back to callers that keep them in variables, also when a call unwinds, and
// the lines the call stack shows stay right where calls share a line or run in a loop inside a body run in place.
STATIC s_cTrail := "", s_aErr := {}

PROCEDURE Main()
   LOCAL x, e, i, n := 0
   ? "ack", Ack( 0, 0 ), Ack( 1, 2 ), Ack( 2, 3 ), Ack( 3, 3 ), Ack( 2, 3.0 ), Ack( 2.0, 3 ), Ack( 0, 1.5 )
   ? "tak", Tak( 18, 12, 6 ), Tak( 18.5, 12, 6 ), Tak( 10, 20, 30 )
   ? "grow", Grow( 0, 1 ), Grow( 3, 5 ), Grow( 5, 1 ), ValType( Grow( 5, 1 ) ), ValType( Grow( 6, 1 ) ), ;
      Grow( 6, 1 ) > 9223372036854775807, Grow( 6, 1 ) == 2 ^ 65
   ? "nested", Twice( Twice( Twice( 3 ) ) ), Twice( Twice( 4611686018427387904 ) ) == 2 ^ 64
   ? "lines", Lines( 2, 0 ), s_cTrail
   s_cTrail := ""
   FOR i := 1 TO 2
      n += Inner( i )
   NEXT
   ? "loop", n, s_cTrail
   x := 5
   BEGIN SEQUENCE
      x := Brk( 1 )
   RECOVER USING e
      ? "kept", x, e
   END
   x := Brk( 0 ) + Brk( -4 )
   ? "words", x, Odd( 7 ), Odd( 8.0 ), ValType( Odd( 1 ) ), Later( SToD( "20261130" ), 2 ), Nothing( 1 )
   ErrorBlock( {| e | Trace( e ), Break( e ) } )
   BEGIN SEQUENCE
      Bad( 2, 1 )
   RECOVER USING e
      ? "deep", e:operation, e:subCode, ValType( e:args[ 2 ] ), s_cTrail
   END
   ErrorBlock( {| e | AAdd( s_aErr, e:operation ), "s" } )
   ? "substituted", Bad( 2, 1 ), Bad( 0, 3 ), Len( s_aErr )
   s_aErr := {}
   ErrorBlock( {| e | AAdd( s_aErr, e:operation ), IIf( e:operation $ "== =", .F., 0 ) } )
   ? "strings", Ack( "a", 1 ), Ack( 1 ), Ack( NIL, NIL ), Len( s_aErr ), s_aErr[ 1 ], s_aErr[ 2 ]
   ? "dynamic", Do( "Ack", 2, 3 ), &( "Ack( 2, 2 )" ), Eval( @Ack(), 1, 1 ), Eval( {| a, b | Ack( a, b ) }, 3, 2 )
RETURN

FUNCTION Ack( m, n )
   IF m == 0
      RETURN n + 1
   ELSEIF n == 0
      RETURN Ack( m - 1, 1 )
   ENDIF
RETURN Ack( m - 1, Ack( m, n - 1 ) )

STATIC FUNCTION Tak( x, y, z )
   IF y < x
      RETURN Tak( Tak( x - 1, y, z ), Tak( y - 1, z, x ), Tak( z - 1, x, y ) )
   ENDIF
RETURN z

// 2 ^ ( 2 ^ m ) * ( n + 1 ) - 1: past the largest integer, a double, which the calls on it pass on.
STATIC FUNCTION Grow( m, n )
   IF m == 0
      RETURN n * 2 + 1
   ENDIF
RETURN Grow( m - 1, Grow( m - 1, n ) )

STATIC FUNCTION Twice( n )
RETURN n * 2

// The levels below each call show the line of the call they came from: the inner and the outer one differ.
STATIC FUNCTION Lines( m, n )
   LOCAL r
   IF m == 0
      Mark()
      RETURN n + 1
   ENDIF
   r := Lines( m - 1, n )
RETURN Lines( m - 1, r ) + 1

// Run in place in the loop's body (inlined), its call shows its own line and the loop's, on every pass.
STATIC FUNCTION Inner( i )
RETURN Mark() + i

STATIC FUNCTION Mark()
   LOCAL k := 1
   DO WHILE ! Empty( ProcName( k ) ) .AND. ProcName( k ) != "MAIN"
      s_cTrail += ProcName( k ) + "(" + LTrim( Str( ProcLine( k ) ) ) + ")"
      k++
   ENDDO
   s_cTrail += " "
RETURN 0

STATIC FUNCTION Brk( n )
   IF n > 0
      Break( n )
   ENDIF
RETURN n * 2

STATIC FUNCTION Odd( n )
RETURN n % 2 == 1

STATIC FUNCTION Later( d, k )
   IF k == 0
      RETURN d
   ENDIF
RETURN Later( d + 1, k - 1 )

STATIC PROCEDURE Nothing( n )
   n++
RETURN

// An error at the bottom of the recursion: the levels it went through, then a value of another type goes on.
STATIC FUNCTION Bad( m, n )
   IF m == 0
      RETURN n + "x"
   ENDIF
RETURN Bad( m - 1, Bad( m - 1, n ) )

STATIC PROCEDURE Trace( e )
   LOCAL k := 1
   s_cTrail := e:operation + ":"
   DO WHILE ! Empty( ProcName( k ) ) .AND. ProcName( k ) != "MAIN"
      s_cTrail += " " + ProcName( k ) + "(" + LTrim( Str( ProcLine( k ) ) ) + ")"
      k++
   ENDDO
RETURN
