// Producers and consumers on threads (ThreadStart) over a bounded queue of 64 guarded by a Mutex and two Conditions
// (not full, not empty): 4 producers put 50,000 numbers each, 4 consumers take them until each finds the end mark the
// program puts once the producers are joined. The sum and count of what was taken must be what was put, whatever the
// scheduling.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), oM := Mutex(), oNotFull := Condition(), oNotEmpty := Condition(), aQ := {}
   LOCAL aP := {}, aC := {}, k, nSum := 0, nCount := 0, a, nItems := 50000
   FOR k := 1 TO 4
      AAdd( aC, ThreadStart( {|| Consume( oM, oNotFull, oNotEmpty, aQ ) } ) )
   NEXT
   FOR k := 1 TO 4
      AAdd( aP, ThreadStart( {| p | Produce( oM, oNotFull, oNotEmpty, aQ, p, nItems ) }, k ) )
   NEXT
   AEval( aP, {| x | x:Join() } )
   FOR k := 1 TO 4
      Put( oM, oNotFull, oNotEmpty, aQ, NIL )
   NEXT
   FOR EACH a IN aC
      a := a:Join()
      nSum += a[ 1 ]
      nCount += a[ 2 ]
   NEXT
   ? "producer_consumer", ElapsedMs() - t, ;
     IIf( nCount == 4 * nItems .AND. nSum == 10000000 * nItems + 2 * nItems * ( nItems + 1 ), "ok", "WRONG" )
RETURN

// Producer p puts p * 1000000 + i for i from 1 to n.
STATIC FUNCTION Produce( oM, oNotFull, oNotEmpty, aQ, p, n )
   LOCAL i
   FOR i := 1 TO n
      Put( oM, oNotFull, oNotEmpty, aQ, p * 1000000 + i )
   NEXT
RETURN NIL

STATIC PROCEDURE Put( oM, oNotFull, oNotEmpty, aQ, x )
   SYNCHRONIZED oM
      DO WHILE Len( aQ ) >= 64
         oNotFull:Wait( oM )
      ENDDO
      AAdd( aQ, x )
      oNotEmpty:Notify()
   END
RETURN

// Takes numbers until the end mark (NIL); returns { their sum, their count }.
STATIC FUNCTION Consume( oM, oNotFull, oNotEmpty, aQ )
   LOCAL nSum := 0, nCount := 0, x
   DO WHILE .T.
      SYNCHRONIZED oM
         DO WHILE Len( aQ ) == 0
            oNotEmpty:Wait( oM )
         ENDDO
         x := aQ[ 1 ]
         ADel( aQ, 1 )
         ASize( aQ, Len( aQ ) - 1 )
         oNotFull:Notify()
      END
      IF x == NIL
         EXIT
      ENDIF
      nSum += x
      nCount++
   ENDDO
RETURN { nSum, nCount }
