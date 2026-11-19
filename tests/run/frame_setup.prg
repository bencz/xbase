// Frames set up where a path first needs one: a leaf's early return runs without its frame, as a small entry its
// callers run in place, and whatever way the function is reached (a direct call, a codeblock, a macro, Do()), the
// rest of it, its errors and the call stack they show, behave as before.
PROCEDURE Main()
   LOCAL e, aStack
   ? Leaf( NIL ), Leaf( "abc" ), Leaf( { 1, 2 } )
   ? Eval( {| x | Leaf( x ) }, "abcd" ), &( 'Leaf( "ab" )' ), Do( "LEAF", "abcde" ), Do( "LEAF" )
   ? Fib( 20 ), Eval( {|| Fib( 15 ) } ), &( "Fib( 10 )" )
   ErrorBlock( {| x | aStack := Stack(), Break( x ) } )
   BEGIN SEQUENCE
      ? Fails( 3 )
   RECOVER USING e
      ? e:Description, e:Operation
      AEval( aStack, {| c | QQOut( " " + c ) } )
   END SEQUENCE
   RETURN

FUNCTION Leaf( x )
   IF x == NIL
      RETURN 0
   ENDIF
   RETURN Len( x )

FUNCTION Fib( n )
   IF n < 2
      RETURN n
   ENDIF
   RETURN Fib( n - 1 ) + Fib( n - 2 )

FUNCTION Fails( n )
   IF n < 1
      RETURN n + "x"
   ENDIF
   RETURN Fails( n - 1 )

STATIC FUNCTION Stack()
   LOCAL a := {}, k := 1
   DO WHILE ! Empty( ProcName( k ) )
      AAdd( a, ProcName( k ) + "(" + LTrim( Str( ProcLine( k ) ) ) + ")" )
      k++
   ENDDO
   RETURN a
