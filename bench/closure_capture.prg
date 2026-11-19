// Closures: 500,000 counters made by a function, each a block holding its own copy of a local and a parameter,
// each called 4 times; then 2,000,000 calls of blocks that share one captured local and change it.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), a := Array( 500000 ), i, j, b, n := 0, nShared := 0, bInc, bRead, ok
   FOR i := 1 TO Len( a )
      a[ i ] := Counter( i % 10 )
   NEXT
   FOR j := 1 TO 4
      FOR EACH b IN a
         n += Eval( b, j )
      NEXT
   NEXT
   bInc := {| x | nShared += x }
   bRead := {|| nShared % 1000 }
   FOR i := 1 TO 2000000
      Eval( bInc, i % 7 )
      n += Eval( bRead )
   NEXT
   ok := n == 1031500997 .AND. nShared == 5999997
   ? "closure_capture", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN

STATIC FUNCTION Counter( nStep )
   LOCAL nCount := 0
   RETURN {| x | nCount += nStep + x }
