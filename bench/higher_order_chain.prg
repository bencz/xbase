// Chains of the runtime's Enumerable methods over an array of 500,000 numbers, each step a generator fed by the one
// before: Where, Select, Skip, TakeWhile and Sum; Select with the position, Where, Take and Count; Distinct with
// a key and Count; 5 rounds.
PROCEDURE Main()
   LOCAL t, a := Array( 500000 ), i, r, n := 0, ok
   FOR i := 1 TO Len( a )
      a[ i ] := ( i * 37 ) % 1000
   NEXT
   t := ElapsedMs()
   FOR r := 1 TO 5
      n += a:Where( {| x | x % 3 != 0 } ):Select( {| x | x * r } ):Skip( 10 ):TakeWhile( {| x | x < 4900 } ):Sum()
      n += a:Select( {| x, nPos | x + nPos % 7 } ):Where( {| x | x % 2 == 0 } ):Take( 50000 + r ):Count()
      n += a:Distinct( {| x | x % ( 100 * r ) } ):Count()
   NEXT
   ok := n == 1663633050
   ? "higher_order_chain", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN
