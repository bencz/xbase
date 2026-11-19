// The runtime's walks with blocks: AEval() over an array of 1,000,000 numbers and HEval() over a hash of 200,000
// entries, the block adding to a captured local; 20 rounds.
PROCEDURE Main()
   LOCAL t, a := Array( 1000000 ), h := { => }, i, r, n := 0, ok
   FOR i := 1 TO Len( a )
      a[ i ] := i % 777
   NEXT
   FOR i := 1 TO 200000
      h[ i * 3 ] := i % 555
   NEXT
   t := ElapsedMs()
   FOR r := 1 TO 20
      AEval( a, {| x, nPos | n += x + IIf( nPos % 1000 == 0, r, 0 ) } )
      HEval( h, {| k, v | n += v + IIf( k % 3000 == 0, 1, 0 ) } )
   NEXT
   ok := n == 8867500260
   ? "aeval_heval", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN
