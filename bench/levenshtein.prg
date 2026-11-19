// Edit distance: the Levenshtein distance of 10,000 pairs of strings of 30 to 60 letters (the second one the first
// with random edits), by dynamic programming over two rows, the letters read with SubStr().
STATIC s_nSeed := 1001

PROCEDURE Main()
   LOCAL t, aPairs := Array( 10000 ), i, p, n := 0, ok
   FOR i := 1 TO Len( aPairs )
      aPairs[ i ] := Pair( 30 + Rand( 31 ) )
   NEXT
   t := ElapsedMs()
   FOR EACH p IN aPairs
      n += Distance( p[ 1 ], p[ 2 ] )
   NEXT
   ok := n == 108309 .AND. Distance( "kitten", "sitting" ) == 3
   ? "levenshtein", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN

STATIC FUNCTION Distance( s, u )
   LOCAL m := Len( s ), n := Len( u ), aPrev := Array( n + 1 ), aCur := Array( n + 1 ), aSwap, i, j, c
   FOR j := 1 TO n + 1
      aPrev[ j ] := j - 1
   NEXT
   FOR i := 1 TO m
      aCur[ 1 ] := i
      c := SubStr( s, i, 1 )
      FOR j := 1 TO n
         aCur[ j + 1 ] := Min( Min( aPrev[ j + 1 ], aCur[ j ] ) + 1, aPrev[ j ] + IIf( c == SubStr( u, j, 1 ), 0, 1 ) )
      NEXT
      aSwap := aPrev
      aPrev := aCur
      aCur := aSwap
   NEXT
   RETURN aPrev[ n + 1 ]

STATIC FUNCTION Pair( nLen )
   LOCAL s := "", u := "", i, k
   FOR i := 1 TO nLen
      s += Chr( 97 + Rand( 4 ) )
   NEXT
   FOR i := 1 TO nLen
      k := Rand( 10 )
      IF k == 0
         u += Chr( 97 + Rand( 4 ) )
      ELSEIF k == 1
         u += SubStr( s, i, 1 ) + Chr( 97 + Rand( 4 ) )
      ELSEIF k != 2
         u += SubStr( s, i, 1 )
      ENDIF
   NEXT
   RETURN { s, u }

STATIC FUNCTION Rand( n )
   s_nSeed := ( s_nSeed * 16807 ) % 2147483647
   RETURN s_nSeed % n
