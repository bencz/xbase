// A FOR EACH variable shared before the loop (captured by a codeblock, a STATIC passed by reference): everyone
// sharing it sees the current element during the loop and the restored value after it.
STATIC s := 1
PROCEDURE Main()
   LOCAL a := { 10, 20 }, x := 1, pre, ins := {}, r, k
   pre := {|| x }
   FOR EACH x IN a
      ?? " during", Eval( pre )
      AAdd( ins, {|| x } )
   NEXT
   ? "after", x, Eval( pre ), Eval( ins[ 1 ] ), Eval( ins[ 2 ] )
   x := 5
   ? "x := 5", Eval( pre ), Eval( ins[ 1 ] ), Eval( ins[ 2 ] )
   Eval( ins[ 1 ], 0 )
   ins := {}
   FOR EACH x IN a
      AAdd( ins, {| n | IIf( n == NIL, x, x := n ) } )
   NEXT
   Eval( ins[ 1 ], 77 )
   ? "write via block after loop", x, a[ 1 ], a[ 2 ]
   x := 1
   k := 0
   FOR EACH x IN a
      AAdd( ins, {| n | IIf( n == NIL, x, x := n ) } )
      IF ++k == 2
         Eval( ins[ Len( ins ) - 1 ], 55 )
      ENDIF
   NEXT
   ? "write via old block in loop", x, a[ 1 ], a[ 2 ]
   r := Cap( @s )
   FOR EACH s IN a
      ?? " static during", Eval( r )
   NEXT
   ? "static after", s, Eval( r )
   RETURN

FUNCTION Cap( v )
   RETURN {| n | IIf( n == NIL, v, v := n ) }
