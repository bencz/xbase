// String similarity: the Jaro-Winkler similarity of 300,000 pairs of 6 to 14 letters (the second one the first with
// random changes), the sum of the doubles checked to 6 decimals.
STATIC s_nSeed := 271

PROCEDURE Main()
   LOCAL t, aPairs := Array( 300000 ), i, p, nSum := 0, ok
   FOR i := 1 TO Len( aPairs )
      aPairs[ i ] := Pair( 6 + Rand( 9 ) )
   NEXT
   t := ElapsedMs()
   FOR EACH p IN aPairs
      nSum += JaroWinkler( p[ 1 ], p[ 2 ] )
   NEXT
   ok := Abs( nSum - 260943.0356007 ) < 0.000001 .AND. ;
      Abs( JaroWinkler( "MARTHA", "MARHTA" ) - 0.961111111 ) < 0.000000001
   ? "jaro_winkler", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN

STATIC FUNCTION JaroWinkler( s1, s2 )
   LOCAL l1 := Len( s1 ), l2 := Len( s2 ), nRange, a1, a2, i, j, k, m := 0, nTrans := 0, nJaro, nPrefix := 0
   IF l1 == 0 .OR. l2 == 0
      RETURN IIf( l1 == l2, 1, 0 )
   ENDIF
   nRange := Max( Int( Max( l1, l2 ) / 2 ) - 1, 0 )
   a1 := AFill( Array( l1 ), .F. )
   a2 := AFill( Array( l2 ), .F. )
   FOR i := 1 TO l1
      FOR j := Max( 1, i - nRange ) TO Min( l2, i + nRange )
         IF !a2[ j ] .AND. SubStr( s1, i, 1 ) == SubStr( s2, j, 1 )
            a1[ i ] := .T.
            a2[ j ] := .T.
            m++
            EXIT
         ENDIF
      NEXT
   NEXT
   IF m == 0
      RETURN 0
   ENDIF
   k := 1
   FOR i := 1 TO l1
      IF a1[ i ]
         DO WHILE !a2[ k ]
            k++
         ENDDO
         IF !( SubStr( s1, i, 1 ) == SubStr( s2, k, 1 ) )
            nTrans++
         ENDIF
         k++
      ENDIF
   NEXT
   nJaro := ( m / l1 + m / l2 + ( m - nTrans / 2 ) / m ) / 3
   DO WHILE nPrefix < 4 .AND. nPrefix < Min( l1, l2 ) .AND. SubStr( s1, nPrefix + 1, 1 ) == SubStr( s2, nPrefix + 1, 1 )
      nPrefix++
   ENDDO
   RETURN nJaro + nPrefix * 0.1 * ( 1 - nJaro )

STATIC FUNCTION Pair( nLen )
   LOCAL s := "", u, i, k
   FOR i := 1 TO nLen
      s += Chr( 65 + Rand( 6 ) )
   NEXT
   u := s
   FOR i := 1 TO 3
      k := Rand( Len( u ) ) + 1
      u := Stuff( u, k, Rand( 2 ), Chr( 65 + Rand( 6 ) ) )
   NEXT
   RETURN { s, u }

STATIC FUNCTION Rand( n )
   s_nSeed := ( s_nSeed * 16807 ) % 2147483647
   RETURN s_nSeed % n
