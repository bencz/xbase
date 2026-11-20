// Keys chosen to collide: 50,000 integers whose hashes are all 0x5EED (the inverse of the integers' fold, computed
// with 64-bit products in 16-bit parts), against as many ordinary ones. Their insertion would take n^2 / 2 probe steps
// with the fixed hash; the table turns keyed after a long probe and takes them as fast as the others, every key still
// found, those inserted before the switch too, strings and dates among them, a copy of it too, after removals.
PROCEDURE Main()
   LOCAL n := 50000, aKeys := Array( n ), h := { => }, g := { => }, k, t0, tFlood, tPlain, ok := .T., c

   FOR k := 1 TO n
      aKeys[ k ] := Unmix( ( k << 32 ) | ( 0x5EED ^^ k ) )
   NEXT
   ? aKeys[ 1 ] == 0x9811B4985191D13B, aKeys[ 2 ] == 0x9DE0D9EBFFFDC3B3, aKeys[ 5 ] == 0x93642C2CC7C85559

   t0 := ElapsedMs()
   FOR k := 1 TO n
      g[ k * 7919 ] := k
   NEXT
   tPlain := ElapsedMs() - t0

   h[ "before" ] := 1
   h[ SToD( "20260101" ) ] := 2
   t0 := ElapsedMs()
   FOR k := 1 TO n
      h[ aKeys[ k ] ] := k
   NEXT
   tFlood := ElapsedMs() - t0
   h[ "after" ] := 3
   h[ 2.5 ] := 4

   FOR k := 1 TO n
      ok := ok .AND. h[ aKeys[ k ] ] == k .AND. g[ k * 7919 ] == k
   NEXT
   ? ok, Len( h ), h[ "before" ], h[ SToD( "20260101" ) ], h[ "after" ], h[ 2.5 ], HHasKey( h, 12345 )
   c := HClone( h )
   ? c[ aKeys[ 777 ] ], c[ "before" ], Len( c )
   FOR k := 1 TO n STEP 2 // removals from the keyed table, and keys found after them
      HDel( c, aKeys[ k ] )
   NEXT
   ok := Len( c ) == n / 2 + 4
   FOR k := 1 TO n
      ok := ok .AND. IIf( k % 2 == 1, ! HHasKey( c, aKeys[ k ] ), c[ aKeys[ k ] ] == k )
   NEXT
   ? ok, Len( h )
   ? IIf( tFlood <= 20 * tPlain + 100, "flood as fast as plain", ;
         "flood " + Str( tFlood ) + " ms, plain " + Str( tPlain ) )
   RETURN

// The inverse of the integers' fold before its last step: x ^= x >> 33, x *= C^-1, ... (C the finalizer's constants).
STATIC FUNCTION Unmix( y )
   y := y ^^ BitShr( y, 33 )
   y := MulLo( y, 0x9CB4B2F8129337DB )
   y := y ^^ BitShr( y, 33 )
   y := MulLo( y, 0x4F74430C22A54005 )
   RETURN y ^^ BitShr( y, 33 )

// The low 64 bits of a * b, in 16-bit parts.
STATIC FUNCTION MulLo( a, b )
   LOCAL x := Array( 4 ), y := Array( 4 ), r := 0, carry := 0, i, j, t
   FOR i := 1 TO 4
      x[ i ] := BitShr( a, 16 * ( i - 1 ) ) & 0xFFFF
      y[ i ] := BitShr( b, 16 * ( i - 1 ) ) & 0xFFFF
   NEXT
   FOR i := 1 TO 4
      t := carry
      FOR j := 1 TO i
         t += x[ j ] * y[ i - j + 1 ]
      NEXT
      r := r | ( ( t & 0xFFFF ) << ( 16 * ( i - 1 ) ) )
      carry := BitShr( t, 16 )
   NEXT
   RETURN r
