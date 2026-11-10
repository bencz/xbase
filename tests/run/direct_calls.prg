// Calls between functions of a module pass the parameters' items in registers to a direct entry; the public symbol
// is a thunk that takes them from argv. Missing arguments are NIL, arguments past the parameters are evaluated (their
// effects happen) and left out, `@` reaches the body versioned for references, a procedure returns NIL, and the call
// stack, errors, Do() and codeblocks see the same function. Values kept in frame slots are read after calls as they
// were (a loop carrying values of changing types, an error's substituted result).
INIT PROCEDURE First()
   ? "init", Pair( "i", 1 )
RETURN

EXIT PROCEDURE Last()
   ? "exit", Pair( 2, 3 )
RETURN

PROCEDURE Main()
   LOCAL x := 1, n := 0, e
   ? "missing", Pair(), Pair( 1 ), Pair( 1, 2 )
   ? "extra", Pair( 1, 2, n++, n++ ), n
   Bump( @x )
   Do( "Bump", @x )
   Short( @x )
   ? "byref", x
   ? "static", Twice( 21 )
   ? "procedure", ValType( Nothing( 5 ) )
   ? "do", Do( "Pair", 3, 4 ), Do( "PAIR" ), Eval( {| a, b | Pair( a, b ) }, "a", "b" )
   ? "stack", Where( 7 )
   ? "fib", Fib( 20 )
   ErrorBlock( {| err | Break( err ) } )
   BEGIN SEQUENCE
      Fail( "x" )
   RECOVER USING e
      ? "error", e:operation, e:args[ 1 ], e:args[ 2 ], ProcName( 0 )
   END
   ? "carried", Carried()
   ? "substituted", Substituted()
RETURN

// A @ argument and a parameter not passed.
PROCEDURE Short( v, w )
   v += IIF( w == NIL, 100, 0 )
RETURN

STATIC FUNCTION Twice( n )
RETURN n * 2

// Loop values of changing types, read after calls in each pass.
FUNCTION Carried()
   LOCAL i, v := 1, s := ""
   FOR i := 1 TO 6
      v := IIF( i % 2 == 0, Pair( v, i ), i * 10 )
      s += ValType( v ) + Pair( i, i )
   NEXT
RETURN s

// The result an error block substitutes goes on to the type tests after the call.
FUNCTION Substituted()
   LOCAL r, t := ""
   ErrorBlock( {| err | 7 } )
   r := "a" * 2
   t += ValType( r ) + Str( r + 1, 2 )
   r := Pair( r, r )
RETURN t + r

FUNCTION Pair( a, b )
RETURN ValType( a ) + ValType( b ) + IIF( ValType( a ) == "N" .AND. ValType( b ) == "N", Str( a + b, 2 ), "" )

PROCEDURE Bump( v )
   v += 10
RETURN

PROCEDURE Nothing( v )
RETURN

FUNCTION Where( n )
RETURN ProcName( 0 ) + "(" + LTrim( Str( ProcLine( 0 ) ) ) + ") " + ProcName( 1 ) + " " + Str( n, 1 )

FUNCTION Fib( n )
   IF n < 2
      RETURN n
   ENDIF
RETURN Fib( n - 1 ) + Fib( n - 2 )

FUNCTION Fail( s )
RETURN s + 1
