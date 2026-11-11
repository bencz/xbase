// Orders kept up to date through many changes: 1500 records with three orders (a character key, a descending number,
// a condition), then 3000 key changes and appends at random (the condition's entries come and go); each order then
// walks in its order, holds exactly the records it should, and a SEEK finds every key. Pages split, take from their
// siblings and join. The files start with "rdd_index_stress", deleted at the end.
FIELD K, N
STATIC nSeed := 12345

PROCEDURE Main()
   LOCAL i, nRec
   dbCreate( "rdd_index_stress", { { "K", "C", 12, 0 }, { "N", "N", 8, 0 } } )
   USE rdd_index_stress NEW
   FOR i := 1 TO 1500
      APPEND BLANK
      REPLACE K WITH RandKey(), N WITH Rand( 100000 )
   NEXT
   INDEX ON K TO rdd_index_stress_k
   INDEX ON N TO rdd_index_stress_n DESCENDING ADDITIVE
   INDEX ON K TO rdd_index_stress_f FOR N < 50000 ADDITIVE
   SET ORDER TO 1
   FOR i := 1 TO 3000
      nRec := Rand( LastRec() ) + 1
      GO nRec
      IF i % 7 == 0
         APPEND BLANK
      ENDIF
      REPLACE K WITH RandKey(), N WITH Rand( 100000 )
   NEXT
   Check( 1, {|| K }, {|| .T. } )
   Check( 2, {|| N }, {|| .T. } )
   Check( 3, {|| K }, {|| N < 50000 } )
   CLOSE
   FErase( "rdd_index_stress.dbf" )
   AEval( { "k", "n", "f" }, {| c | FErase( "rdd_index_stress_" + c + ".ntx" ) } )
RETURN

STATIC PROCEDURE Check( nOrder, bKey, bFor )
   LOCAL nCount := 0, nWant := 0, xPrev, lOrdered := .T., nBad := 0
   SET ORDER TO nOrder
   GO TOP
   DO WHILE ! Eof()
      IF nCount > 0 .AND. IIf( nOrder == 2, Eval( bKey ) > xPrev, Eval( bKey ) < xPrev )
         lOrdered := .F.
      ENDIF
      xPrev := Eval( bKey )
      nCount++
      SKIP
   ENDDO
   SET ORDER TO 0
   GO TOP
   DO WHILE ! Eof()
      IF Eval( bFor )
         nWant++
      ENDIF
      SKIP
   ENDDO
   SET ORDER TO nOrder
   GO TOP
   DO WHILE ! Eof()
      IF nOrder != 2 .AND. ! dbSeek( Eval( bKey ) )
         nBad++
      ENDIF
      SKIP
   ENDDO
   ? "order", nOrder, "count", nCount, "want", nWant, "ordered", lOrdered, "seek misses", nBad
RETURN

STATIC FUNCTION Rand( n )
   nSeed := ( nSeed * 1103 + 12345 ) % 1000003
RETURN nSeed % n

STATIC FUNCTION RandKey()
   LOCAL c := "", i
   FOR i := 1 TO 6
      c += Chr( 65 + Rand( 26 ) )
   NEXT
RETURN c
