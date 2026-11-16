// A nest of two loops whose inner loop takes a slow path once (an integer product that overflows to a double): the
// nest is split whole (its fast copy makes no call), the outer loop's generic pass goes back to the outer loop again
// and the inner one's to the inner loop again (codegen.loop_nest_split); the sum is the same as at -O0.
PROCEDURE Main()
   LOCAL a := { { 1, 2, 3 }, { 4, 0x3FFFFFFFFFFFFFFF, 6 }, { 7, 8, 9 }, { 10, 11, 12 } }, i, j, s := 0, n := 0
   FOR i := 1 TO Len( a )
      FOR j := 1 TO Len( a[ i ] )
         s += a[ i, j ] * i
         n++
      NEXT
   NEXT
   ? Str( s, 30, 0 ), n, i, j
RETURN
