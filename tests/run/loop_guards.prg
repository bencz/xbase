// Loops that index a value they do not change: the test that it is an array and the index within it moves before the
// loop (LoopGuards), and when it fails on entry the first pass goes the general way, with the loop's values as they
// were. An array of arrays (each level a test of its own), a hash, a double index, an index out of bounds (the error,
// caught on the first pass), an empty loop; and, where no test moves, substrings of a string and a loop that changes
// what it indexes.
PROCEDURE Main()
   LOCAL a := { { 1, 2, 3 }, { 10, 20, 30 } }, h := { "x" => { 4, 5, 6 } }, e
   ? Rows( a, 2, 3 ), Rows( a, 1, 0 ), Deep( { { { 7, 8 } } }, 2 )
   ? Keyed( h, "x", 3 ), Chars( "abcdef", 4 ), Scaled( a, 2.0, 3 )
   BEGIN SEQUENCE WITH {| err | Break( err ) }
      ? Rows( a, 3, 3 )
   RECOVER USING e
      ? "error", e:subCode, e:description, e:operation
   END
   ? Growing( 5 )
RETURN

// The values in LOCALs: parameters may be references, which a loop reads again on every pass.
FUNCTION Rows( a, i, n )
   LOCAL k, s := 0, nN := n, nI := i, aA := a
   FOR k := 1 TO nN
      s += aA[ nI, k ] * k
   NEXT
RETURN s

FUNCTION Deep( a, n )
   LOCAL k, s := 0, nN := n, aA := a
   FOR k := 1 TO nN
      s += aA[ 1, 1, k ]
   NEXT
RETURN s

FUNCTION Keyed( h, c, n )
   LOCAL k, s := 0, nN := n, hH := h, cC := c
   FOR k := 1 TO nN
      s += hH[ cC ][ k ]
   NEXT
RETURN s

FUNCTION Chars( c, n )
   LOCAL k, s := "", nN := n, cC := c
   FOR k := 1 TO nN
      s += SubStr( cC, k, 1 ) + Upper( SubStr( cC, nN - k + 1, 1 ) )
   NEXT
RETURN s

FUNCTION Scaled( a, x, n )
   LOCAL k, s := 0, nN := n, nX := x, aA := a
   FOR k := 1 TO nN
      s += aA[ nX ][ k ]
   NEXT
RETURN s

// The loop changes the array it indexes: nothing about it holds from one pass to the next.
FUNCTION Growing( n )
   LOCAL a := { 1 }, k, s := 0, nN := n
   FOR k := 1 TO nN
      s += a[ 1 ] + Len( a )
      AAdd( a, k )
      a[ 1 ] := k * 10
   NEXT
RETURN s
