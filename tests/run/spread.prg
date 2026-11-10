// `...` after a function's parameters, and `...` as the last argument of a call or a send or the last element of an
// array literal: the arguments the function received past its parameters (none, one or many; after other arguments).
// A compiled function receives references as passed; a runtime function (but where it takes references), a method and
// an array element the values, and a reference whose element is gone is an error, also for an array nothing reads. The
// items stay rooted while the call runs (PValue() may replace them); a loop of such calls does not grow the stack.
PROCEDURE Main()
   LOCAL x := 1, y := 2
   ? "vari:", Vari( "a", "b", "c" )
   ? "vari0:", Vari()
   ? "fwd:", Fwd( 10, 20, 30 )
   ? "mid:", Mid( 1, 2, 3, 4 )
   ? "arr:", Len( Arr( 1, 2, 3 ) ), Arr( 1, 2, 3 )[ 2 ], Arr( 1, 2, 3 )[ 4 ], Len( Arr( 1 ) )
   Refs( 0, @x, @y )
   ? "refs:", x, y
   ? "native:", Natives( 0, 3 ), Biggest( 0, 3, 9 )
   ? "eval:", EvalFwd( {| a, b | a + b }, 4, 5 )
   ? "qout:"
   OutAll( "one", 2, .T. )
   ? "max:", MaxOf( 3, 9, 4 )
   Rest()
RETURN

FUNCTION Vari( a, ... )
   LOCAL x := { ... }
RETURN Str( PCount(), 2 ) + " " + Str( Len( x ), 2 ) + " " + IIF( Len( x ) > 0, S( x[ 1 ] ), "-" ) + ;
   IIF( Len( x ) > 1, S( ATail( x ) ), "" )

FUNCTION S( v )
RETURN IIF( ValType( v ) == "N", LTrim( Str( v ) ), v )

FUNCTION Fwd( a, ... )
RETURN Vari( ... )

FUNCTION Mid( a, ... )
RETURN Vari( "x", "y", ... )

FUNCTION Arr( ... )
RETURN { 0, 99, ... }

FUNCTION Refs( a, ... )
   SetRefs( ... )
RETURN NIL

FUNCTION SetRefs( p, q )
   p := 10
   q := 20
RETURN NIL

FUNCTION Natives( a, ... )
RETURN Left( "abcdef", ... ) + Replicate( "z", ... )

FUNCTION Biggest( a, ... )
RETURN Max( ... )

FUNCTION EvalFwd( b, ... )
RETURN Eval( b, ... )

PROCEDURE OutAll( ... )
   QOut( ... )
RETURN

FUNCTION MaxOf( ... )
   LOCAL n := 0, v
   FOR EACH v IN { ... }
      n := Max( n, v )
   NEXT
RETURN n

STATIC PROCEDURE Rest()
   LOCAL o := ErrorNew(), a := { 1, 2, 3 }, e, i, n := 0, s := "abc"
   ? "send:", SendFwd( o, 7 ), o:subCode
   ? "byval:", UpperOf( @s ), s
   ErrorBlock( {| err | Break( err ) } )
   BEGIN SEQUENCE
      ? Gone( a, @a[ 3 ] )
   RECOVER USING e
      ? "gone:", e:subCode, e:operation
   END
   FOR i := 1 TO 100000
      n += Count( i, i, i )
   NEXT
   ? "loop:", n
   ? "many:", Many()
   ? "continued:", Continued( "zz", 1, 2, 3 )
   n := 5
   ? "cargo:", Cargo( o, @n ), ValType( o:cargo ), o:cargo
   a := { 1, 2, 3 }
   ErrorBlock( {| err | Break( err ) } )
   BEGIN SEQUENCE
      Unread( a, @a[ 3 ] )
      ? "unread: no error"
   RECOVER USING e
      ? "unread:", e:subCode
   END
   ? "after:", EvalAfter( {|| "ran" } )
   ? "fixed:", Fixed( "a", 1, 2 )
   ? "first:", RefFirst( {| a | a := 9 } ), ValueFirst( {|| "ran" } )
   ? "loop:", Repeat( "a", "b" )
RETURN

// `...` in a loop passes the same arguments every time: what a callee replaces (PValue()) does not reach the next.
FUNCTION Repeat( ... )
   LOCAL i, s := ""
   FOR i := 1 TO 3
      s += Changes( ... )
   NEXT
RETURN s

FUNCTION Changes( ... )
   LOCAL s := PValue( 1 ) + PValue( 2 )
   PValue( 1, "z" )
RETURN s

// An argument of Eval before `...` lands among its `@...`: the reference stays; where Eval takes its block, the
// value goes.
FUNCTION RefFirst( b, ... )
   LOCAL x := 1
   Eval( b, @x, ... )
RETURN x

FUNCTION ValueFirst( b, ... )
RETURN Eval( @b, ... )

// The arguments before `...` are counted with those it passes on.
FUNCTION Fixed( a, ... )
RETURN Cnt( a, a, ... )

// A function copied for the typed continuation of a loop keeps its `...` in both copies.
FUNCTION Continued( s, ... )
   LOCAL i
   ErrorBlock( {| e | "q" } )
   FOR i := 1 TO 3
      IF ( s * 2 ) + "z" == "qz"
         s := "zz"
      ENDIF
   NEXT
RETURN Cnt( ... )

FUNCTION Cnt( ... )
RETURN PCount()

// A method receives the value of an argument passed on by reference.
FUNCTION Cargo( o, ... )
   o:_cargo( ... )
RETURN o:cargo

FUNCTION Unread( a, ... )
   LOCAL x
   ASize( a, 1 )
   x := { ... }
RETURN NIL

// A reference passed where the function takes none (Eval's block, Hash() everywhere) is its value.
FUNCTION EvalAfter( b, ... )
   LOCAL s := "x"
RETURN Eval( @b, ... ) + ValType( Hash( "k", @s, ... )[ "k" ] )

FUNCTION SendFwd( o, ... )
   o:_subCode( ... )
RETURN o:subCode

FUNCTION UpperOf( ... )
RETURN Upper( ... )

FUNCTION Gone( a, ... )
   ASize( a, 1 )
RETURN Len( { ... } )

FUNCTION Count( a, ... )
RETURN Len( { a, ... } )

FUNCTION Many()
   LOCAL a := Array( 2000 ), i
   FOR i := 1 TO 2000
      a[ i ] := Replicate( "x", i % 7 )
   NEXT
RETURN Spread( 0, a[ 1 ], a[ 2 ], a[ 3 ], a[ 4 ], a[ 5 ], a[ 6 ], a[ 7 ], a[ 8 ] )

FUNCTION Spread( n, ... )
RETURN Collect( ... )

FUNCTION Collect( ... )
   LOCAL i, t := 0
   PValue( 3, Replicate( "y", 50 ) )
   __GcCollect()
   FOR i := 1 TO PCount()
      t += Len( PValue( i ) )
   NEXT
RETURN t
