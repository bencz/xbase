// Blocks run by AEval, AScan and ASort that call those functions again, 30 levels deep, allocating and collecting
// garbage on the way: the runtime keeps every block call's items alive at any depth.
PROCEDURE Main()
   ? WalkEval( 30 ), WalkScan( 30 ), WalkSort( 30 )
   RETURN

FUNCTION WalkEval( n )
   LOCAL s := 1
   IF n > 0
      AEval( { n, Replicate( "x", n ) }, {| x | s += IIf( ValType( x ) == "N", WalkEval( x - 1 ), Len( x ) ) } )
   ENDIF
   IF n % 10 == 0
      __GcCollect()
   ENDIF
   RETURN s

FUNCTION WalkScan( n )
   LOCAL found := 0
   IF n > 0
      found := AScan( { "a", n }, {| x | ValType( x ) == "N" .AND. WalkScan( x - 1 ) >= 0 } )
   ENDIF
   RETURN found + n

FUNCTION WalkSort( n )
   LOCAL a := { 2, 1 }, depth := 0
   IF n > 0
      ASort( a,,, {| x, y | depth := WalkSort( n - 1 ), x < y } )
   ENDIF
   RETURN depth + a[ 1 ]
