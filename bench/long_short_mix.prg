// Long-lived and short-lived values together: 100,000 records that live to the end, 2,500,000 temporary arrays and
// strings, one in 8 of them stored into a long-lived record (an old value pointing to a new one), one record in 64
// steps replaced by a new one.
STATIC s_nSeed := 17

PROCEDURE Main()
   LOCAL t := ElapsedMs(), aLong := Array( 100000 ), i, k, x, r, n := 0, ok
   FOR i := 1 TO Len( aLong )
      aLong[ i ] := { i, Str( i ), NIL }
   NEXT
   FOR i := 1 TO 2500000
      x := { i % 1000, Replicate( Chr( 97 + i % 26 ), i % 40 + 1 ), NIL }
      n += Len( x[ 2 ] )
      IF i % 8 == 0
         k := Rand( Len( aLong ) ) + 1
         aLong[ k ][ 3 ] := x
      ENDIF
      IF i % 64 == 0
         k := Rand( Len( aLong ) ) + 1
         aLong[ k ] := { aLong[ k ][ 1 ] + 1, Str( i ), aLong[ k ][ 3 ] }
      ENDIF
   NEXT
   FOR EACH r IN aLong
      n += r[ 1 ] + Len( r[ 2 ] )
      IF r[ 3 ] != NIL
         n += r[ 3 ][ 1 ]
      ENDIF
   NEXT
   ok := n == 5099739142
   ? "long_short_mix", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN

STATIC FUNCTION Rand( n )
   s_nSeed := ( s_nSeed * 16807 ) % 2147483647
   RETURN s_nSeed % n
