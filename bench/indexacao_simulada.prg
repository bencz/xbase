// An index kept in memory, as an order is: 150,000 entries (an 8-digit key and the record number, one string each)
// made from a fixed seed, then timed: sorted with ASort, 100,000 seeks by binary search (the first entry not below
// the key; a hit when its key is the one sought), 3,000 entries inserted in place (binary search, ASize and AIns),
// and the order checked.
STATIC nSeed := 99991

PROCEDURE Main()
   LOCAL nRecs := 150000, a := Array( nRecs ), t, i, k, p, nHits := 0, nSum := 0, nChk := 0, ok := .T.
   FOR i := 1 TO nRecs
      a[ i ] := "C" + StrZero( Rand( 10000000 ), 8 ) + StrZero( i, 6 )
   NEXT
   t := ElapsedMs()
   ASort( a )
   FOR i := 1 TO 100000
      k := "C" + StrZero( Rand( 10000000 ), 8 )
      p := LowerBound( a, k )
      IF p <= Len( a ) .AND. Left( a[ p ], 9 ) == k
         nHits++
         nSum += Val( Right( a[ p ], 6 ) )
      ENDIF
   NEXT
   FOR i := 1 TO 3000
      k := "C" + StrZero( Rand( 10000000 ), 8 ) + StrZero( nRecs + i, 6 )
      p := LowerBound( a, k )
      ASize( a, Len( a ) + 1 )
      AIns( a, p )
      a[ p ] := k
   NEXT
   FOR i := 2 TO Len( a )
      ok := ok .AND. a[ i - 1 ] < a[ i ]
   NEXT
   FOR i := 1 TO Len( a ) STEP 997
      nChk := ( nChk * 31 + Val( Right( a[ i ], 6 ) ) ) % 1000000007
   NEXT
   ok := ok .AND. nHits == 1426 .AND. nSum == 107771969 .AND. nChk == 409559053 .AND. ;
         a[ 1 ] == "C00000040027737" .AND. ATail( a ) == "C09999947069110"
   ? "indexacao_simulada", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
RETURN

// The first position whose entry is not below k, compared over k's length as < does with SET EXACT OFF
// (Len( a ) + 1 when there is none).
STATIC FUNCTION LowerBound( a, k )
   LOCAL nLo := 1, nHi := Len( a ) + 1, m
   DO WHILE nLo < nHi
      m := Int( ( nLo + nHi ) / 2 )
      IF a[ m ] < k
         nLo := m + 1
      ELSE
         nHi := m
      ENDIF
   ENDDO
RETURN nLo

STATIC FUNCTION Rand( n )
   nSeed := nSeed * 16807 % 2147483647
RETURN nSeed % n
