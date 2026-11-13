// AEval with a codeblock literal, which runs the runtime's AEval and the block in place, behaves as the calls: the
// range (start and count, past the end, none), an array that shrinks while it runs, the array returned, the call
// stack inside the block (AEVAL with line 0), PCount() of the block, an error in the block, a BREAK out of it, an
// argument that is no array (BASE/2017), nested AEvals, an array of unknown type, and a collection object.
PROCEDURE Main()
   LOCAL a := { 1, 2, 3, 4, 5 }, s := 0, e, b := { 1, 2, 3, 4 }
   AEval( a, {| x | s += x } )
   ? s
   s := ""
   AEval( a, {| x, i | s += Str( x, 1 ) + Str( i, 1 ) + " " }, 2, 3 )
   AEval( a, {| x | s += Str( x, 1 ) }, 4 )
   AEval( a, {| x | s += Str( x, 1 ) }, 9 )
   AEval( a, {| x | s += Str( x, 1 ) }, 1, 0 )
   ? s
   AEval( b, {| x, i | iif( i == 2, ASize( b, 2 ), NIL ), s := x } )
   ? Len( AEval( a, {|| NIL } ) ), s
   AEval( { 7 }, {| x | QOut( x, ProcName( 0 ), ProcLine( 0 ), ProcName( 1 ), ProcLine( 1 ), ProcName( 2 ), ;
      ProcLine( 2 ), PCount() ) } )
   BEGIN SEQUENCE WITH {| o | Break( o ) }
      AEval( a, {| x | x / "z" } )
   RECOVER USING e
      ? "error", e:description, e:operation, ProcName( 0 )
   END
   BEGIN SEQUENCE
      AEval( a, {| x | iif( x == 3, Break( "out at " + Str( x, 1 ) ), NIL ) } )
   RECOVER USING e
      ? "break", e
   END
   BEGIN SEQUENCE WITH {| o | Break( o ) }
      AEval( "abc", {| x | QOut( x ) } )
   RECOVER USING e
      ? "not array", e:genCode, e:subCode, e:operation, Len( e:args )
   END
   s := 0
   AEval( { { 1, 2 }, { 3 } }, {| x | AEval( x, {| y | s += y } ) } )
   ? "nested", s, Sum( a ), Sum( { 10, 20 } )
RETURN

FUNCTION Sum( aValues )
   LOCAL n := 0
   AEval( aValues, {| x | n += x } )
RETURN n
