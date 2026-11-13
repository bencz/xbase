// Inlined calls of functions whose variables blocks capture, and of functions that read the arguments they received:
// each call's blocks keep cells of their own (also from a loop), a reference passed on inside an inlined body still
// reaches the caller's variable when that body is inlined in turn, PCount() counts the call's own arguments (also in
// a loop with different counts, and in a block evaluated inside), and the call stack shows each function.
PROCEDURE Main()
   LOCAL a := {}, i, b
   FOR i := 1 TO 3
      AAdd( a, Adder( i * 10 ) )
   NEXT
   ? Eval( a[ 1 ], 1 ), Eval( a[ 2 ], 1 ), Eval( a[ 3 ], 1 )
   b := Counter()
   ? Eval( b ), Eval( b ), Eval( Counter() )
   ? ByRef(), Twice()
   FOR i := 0 TO 3
      ?? Count( iif( i > 0, 1, NIL ), iif( i > 1, 2, NIL ) ), ""
   NEXT
   ? Count(), Count( 1 ), Count( 1, 2 ), Count( 1, 2, 3 ), Inner( 7, 8 )
RETURN

FUNCTION Adder( n )
RETURN {| x | x + n }

FUNCTION Counter()
   LOCAL n := 0
RETURN {|| ++n }

FUNCTION ByRef()
   LOCAL n := 3
   Bump( @n )
RETURN n

FUNCTION Twice()
RETURN ByRef() * 2

PROCEDURE Bump( x )
   x += 100
RETURN

FUNCTION Count( a, b, c )
RETURN PCount()

FUNCTION Inner( x, y )
RETURN Str( PCount(), 2 ) + Str( Eval( {| p | PCount() + p }, 10 ), 3 ) + " " + ProcName( 0 ) + "<" + ProcName( 1 )
