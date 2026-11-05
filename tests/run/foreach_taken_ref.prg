// A reference taken from a FOR EACH variable designates the element of that iteration: inner loops on the same
// variable and the restore after the loop do not change what it designates.
PROCEDURE Main()
   LOCAL a := { 1, 2 }, b := { "x", "y" }, x, r
   FOR EACH x IN a
      r := Cap( @x )
      FOR EACH x IN b
         ?? Eval( r )
      NEXT
   NEXT
   ? "after", x, Eval( r )
   Eval( r, 7 )
   ? "wrote", a[ 1 ], a[ 2 ], x
   RETURN

FUNCTION Cap( v )
   RETURN {| n | IIf( n == NIL, v, v := n ) }
