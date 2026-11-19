// bench/join_arrays.prg in Harbour (its hash functions where xbase has its own names, positions where it has FOR EACH
// with keys).
// Joins of two arrays of records: 20,000 customers { id, name, segment, city } and 500,000 orders { id, customer,
// amount } made from a fixed seed (a third of the orders name a customer that exists). Timed: a hash join (the
// customers in a hash by id, each order looked up; amounts summed per segment, orders counted per city in a hash),
// then a sort-merge join (the orders' customer and position packed in one integer key, sorted with ASort, merged with
// the customers in id order; the amounts weighted by segment).
STATIC nSeed := 2718

PROCEDURE Main()
   LOCAL aCust := Array( 20000 ), aOrd := Array( 500000 ), t, i, c, o, hCust := { => }, aSeg := { 0, 0, 0, 0, 0 }
   LOCAL hCity := { => }, nMatched := 0, j, aKeys, nPos, nW := 0, nMerged := 0, k, cBest, nBest := 0, n, ok
   FOR i := 1 TO Len( aCust )
      aCust[ i ] := { i * 3, "Customer " + LTrim( Str( i ) ), Rand( 5 ) + 1, "CITY" + StrZero( Rand( 100 ) + 1, 3 ) }
   NEXT
   FOR i := 1 TO Len( aOrd )
      aOrd[ i ] := { i, Rand( 60000 ) + 1, Rand( 100000 ) + 1 }
   NEXT
   t := ElapsedMs()

   FOR EACH c IN aCust
      hCust[ c[ 1 ] ] := c
   NEXT
   FOR EACH o IN aOrd
      IF hb_HHasKey( hCust, o[ 2 ] )
         c := hCust[ o[ 2 ] ]
         aSeg[ c[ 3 ] ] += o[ 3 ]
         hCity[ c[ 4 ] ] := hb_HGetDef( hCity, c[ 4 ], 0 ) + 1
         nMatched++
      ENDIF
   NEXT
   FOR j := 1 TO Len( hCity )
      k := hb_HKeyAt( hCity, j )
      n := hb_HValueAt( hCity, j )
      IF n > nBest .OR. ( n == nBest .AND. k < cBest )
         cBest := k
         nBest := n
      ENDIF
   NEXT

   aKeys := Array( Len( aOrd ) )
   FOR i := 1 TO Len( aOrd )
      aKeys[ i ] := aOrd[ i ][ 2 ] * 1000000 + i
   NEXT
   ASort( aKeys )
   nPos := 1
   FOR EACH k IN aKeys
      o := aOrd[ k % 1000000 ]
      DO WHILE nPos <= Len( aCust ) .AND. aCust[ nPos ][ 1 ] < o[ 2 ]
         nPos++
      ENDDO
      IF nPos <= Len( aCust ) .AND. aCust[ nPos ][ 1 ] == o[ 2 ]
         nW += o[ 3 ] * aCust[ nPos ][ 3 ]
         nMerged++
      ENDIF
   NEXT

   ok := nMatched == 166641 .AND. nMerged == 166641 .AND. nW == 25122891088 .AND. aSeg[ 1 ] == 1626943956 .AND. ;
         aSeg[ 2 ] == 1648544230 .AND. aSeg[ 3 ] == 1673211091 .AND. aSeg[ 4 ] == 1694374411 .AND. ;
         aSeg[ 5 ] == 1680345551 .AND. cBest == "CITY080" .AND. nBest == 1980 .AND. Len( hCity ) == 100
   ? "join_arrays", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
RETURN

STATIC FUNCTION Rand( n )
   nSeed := nSeed * 16807 % 2147483647
RETURN nSeed % n
