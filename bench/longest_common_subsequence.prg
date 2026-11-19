// Longest common subsequence: its length for 600 pairs of 200 to 300 letters from a 4-letter alphabet (the second
// related to the first), by dynamic programming over two rows, the letters split into arrays first.
STATIC s_nSeed := 31

PROCEDURE Main()
   LOCAL t, aPairs := Array( 600 ), i, p, n := 0, ok
   FOR i := 1 TO Len( aPairs )
      aPairs[ i ] := Pair( 200 + Rand( 101 ) )
   NEXT
   t := ElapsedMs()
   FOR EACH p IN aPairs
      n += Lcs( p[ 1 ], p[ 2 ] )
   NEXT
   ok := n == 118188 .AND. Lcs( "ABCBDAB", "BDCABA" ) == 4
   ? "longest_common_subsequence", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN

STATIC FUNCTION Lcs( s, u )
   LOCAL m := Len( s ), n := Len( u ), aS := Array( m ), aU := Array( n ), aPrev, aCur, aSwap, i, j, c
   FOR i := 1 TO m
      aS[ i ] := SubStr( s, i, 1 )
   NEXT
   FOR j := 1 TO n
      aU[ j ] := SubStr( u, j, 1 )
   NEXT
   aPrev := AFill( Array( n + 1 ), 0 )
   aCur := AFill( Array( n + 1 ), 0 )
   FOR i := 1 TO m
      c := aS[ i ]
      FOR j := 1 TO n
         IF c == aU[ j ]
            aCur[ j + 1 ] := aPrev[ j ] + 1
         ELSE
            aCur[ j + 1 ] := Max( aPrev[ j + 1 ], aCur[ j ] )
         ENDIF
      NEXT
      aSwap := aPrev
      aPrev := aCur
      aCur := aSwap
   NEXT
   RETURN aPrev[ n + 1 ]

STATIC FUNCTION Pair( nLen )
   LOCAL s := "", u := "", x, i
   FOR i := 1 TO nLen
      x := Chr( 65 + Rand( 4 ) )
      s += x
   NEXT
   FOR i := 1 TO nLen
      IF Rand( 4 ) != 0
         x := SubStr( s, i, 1 )
         u += x
      ENDIF
      IF Rand( 5 ) == 0
         x := Chr( 65 + Rand( 4 ) )
         u += x
      ENDIF
   NEXT
   RETURN { s, u }

STATIC FUNCTION Rand( n )
   s_nSeed := ( s_nSeed * 16807 ) % 2147483647
   RETURN s_nSeed % n
