// An inlined body in a loop whose operation's error is substituted: the iteration goes on in the loop's generic copy
// from the middle of the body, which still shows the inlined functions on the call stack (one frame per depth, shared
// by the copies) and leaves them at its end.
PROCEDURE Main()
   LOCAL i, s := 0, a := { 1, 2, "x", 4 }
   ErrorBlock( {| e | 100 } )
   FOR i := 1 TO Len( a )
      s += Scaled( a[ i ] )
   NEXT
   ? s, ProcName( 0 )
RETURN

FUNCTION Scaled( x )
   LOCAL y := x * 2
RETURN y + Shown()

FUNCTION Shown()
   ?? ProcName( 0 ), ProcName( 1 ), ProcName( 2 ), ""
RETURN 1
