// bench/producer_consumer.prg in Harbour: producers and consumers on threads over a bounded queue of 64 guarded by a
// mutex, with its mutexes' notification queues as counting semaphores where xbase has Conditions (free places, values
// waiting): 4 producers put 50,000 numbers each, 4 consumers take them until each finds the end mark the program puts
// once the producers are joined. The sum and count of what was taken must be what was put, whatever the scheduling.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), oM := hb_mutexCreate(), oFree := hb_mutexCreate(), oFull := hb_mutexCreate(), aQ := {}
   LOCAL aP := {}, aC := {}, k, nSum := 0, nCount := 0, a, th, nItems := 50000
   FOR k := 1 TO 64
      hb_mutexNotify( oFree )
   NEXT
   FOR k := 1 TO 4
      AAdd( aC, hb_threadStart( {|| Consume( oM, oFree, oFull, aQ ) } ) )
   NEXT
   FOR k := 1 TO 4
      AAdd( aP, hb_threadStart( {| p | Produce( oM, oFree, oFull, aQ, p, nItems ) }, k ) )
   NEXT
   AEval( aP, {| x | hb_threadJoin( x ) } )
   FOR k := 1 TO 4
      Put( oM, oFree, oFull, aQ, NIL )
   NEXT
   FOR EACH th IN aC
      hb_threadJoin( th, @a )
      nSum += a[ 1 ]
      nCount += a[ 2 ]
   NEXT
   ? "producer_consumer", ElapsedMs() - t, ;
     IIf( nCount == 4 * nItems .AND. nSum == 10000000 * nItems + 2 * nItems * ( nItems + 1 ), "ok", "WRONG" )
RETURN

// Producer p puts p * 1000000 + i for i from 1 to n.
STATIC FUNCTION Produce( oM, oFree, oFull, aQ, p, n )
   LOCAL i
   FOR i := 1 TO n
      Put( oM, oFree, oFull, aQ, p * 1000000 + i )
   NEXT
RETURN NIL

STATIC PROCEDURE Put( oM, oFree, oFull, aQ, x )
   hb_mutexSubscribe( oFree )
   hb_mutexLock( oM )
   AAdd( aQ, x )
   hb_mutexUnlock( oM )
   hb_mutexNotify( oFull )
RETURN

// Takes numbers until the end mark (NIL); returns { their sum, their count }.
STATIC FUNCTION Consume( oM, oFree, oFull, aQ )
   LOCAL nSum := 0, nCount := 0, x
   DO WHILE .T.
      hb_mutexSubscribe( oFull )
      hb_mutexLock( oM )
      x := aQ[ 1 ]
      ADel( aQ, 1 )
      ASize( aQ, Len( aQ ) - 1 )
      hb_mutexUnlock( oM )
      hb_mutexNotify( oFree )
      IF x == NIL
         EXIT
      ENDIF
      nSum += x
      nCount++
   ENDDO
RETURN { nSum, nCount }
