// Channels under load, checked exactly: P producers and C consumers (threads and tasks) over channels of capacity 1, 7
// and unbounded; every value is received once (a count per value), each producer's values arrive in its order (a
// channel keeps the order of one sender), nothing is lost when the channel closes while senders and receivers wait;
// values of every kind (NIL, numbers, strings, dates, logicals, arrays of thousands of elements, hashes, objects,
// codeblocks) cross intact while collections run; receivers canceled in the middle of the flow lose no value (a value
// goes only to a receiver that takes it); a thousand receivers wait at once and are served in order; 100000 receives
// and sends that a token cancels keep nothing in the channel (the objects alive after a collection, counted).
PROCEDURE Main()
   LOCAL nCap
   FOR EACH nCap IN { 1, 7, 0 }
      ? "order", nCap, Order( 4, 3, 3000, nCap )
   NEXT
   ? "kinds:", Kinds()
   ? "canceled receivers:", CanceledReceivers( 2000 )
   ? "close in the flow:", CloseInFlow()
   ? "many waiting:", ManyWaiting( 1000 )
   ? "memory, canceled receives:", Kept( {|| CanceledWaits( .T., 100000 ) } )
   ? "memory, canceled sends:", Kept( {|| CanceledWaits( .F., 100000 ) } )
RETURN

// Whether what bWork leaves behind (the objects alive after a full collection) stays small.
STATIC FUNCTION Kept( bWork )
   LOCAL nBefore := __GcCollect(), x := Eval( bWork )
RETURN iif( __GcCollect() - nBefore < 10000, "bounded", "kept " + LTrim( Str( __GcCollect() - nBefore ) ) ) + ;
       " (" + x + ")"

// n receives (or sends, on a full channel) that a canceled token ends, on a channel nothing else uses: the channel
// keeps none of them; then it still works.
STATIC FUNCTION CanceledWaits( lReceive, n )
   LOCAL ch := Channel( 1 ), s := CancellationTokenSource(), i, t
   s:Cancel()
   IF !lReceive
      ch:TrySend( "full" )
   ENDIF
   FOR i := 1 TO n
      t := iif( lReceive, ch:ReceiveAsync( s:Token ), ch:SendAsync( i, s:Token ) )
   NEXT
   t:Wait()
RETURN iif( t:IsCanceled, "canceled", "not canceled" ) + ", then " + ;
       iif( lReceive, iif( ch:TrySend( 1 ) .AND. ch:ReceiveAsync():Result == 1, "works", "broken" ), ;
                      iif( ch:ReceiveAsync():Result == "full" .AND. ch:Count() == 0, "works", "broken" ) )

// nProducers each send {id, 1 .. nEach}; nConsumers take them until the channel closes; each consumer checks that each
// producer's sequence numbers grow; at the end every pair was received exactly once.
STATIC FUNCTION Order( nProducers, nConsumers, nEach, nCap )
   LOCAL ch := Channel( nCap ), aP := {}, aC := {}, k, aSeen := Array( nProducers, nEach ), nBad := 0, i, j
   LOCAL oLock := Mutex()
   FOR k := 1 TO nConsumers
      AAdd( aC, iif( k % 2 == 0, ThreadStart( {|| Consume( ch, nProducers, aSeen, oLock ) } ), ;
                                 TaskRun( {|| Consume( ch, nProducers, aSeen, oLock ) } ) ) )
   NEXT
   FOR k := 1 TO nProducers
      AAdd( aP, TaskRun( {| id | Send( ch, id, nEach ) }, k ) )
   NEXT
   AEval( aP, {| t | t:Wait() } )
   ch:Close()
   FOR k := 1 TO nConsumers
      nBad += iif( k % 2 == 0, aC[ k ]:Join(), aC[ k ]:Result )
   NEXT
   FOR i := 1 TO nProducers
      FOR j := 1 TO nEach
         IF aSeen[ i, j ] != 1
            nBad++
         ENDIF
      NEXT
   NEXT
RETURN nBad

STATIC FUNCTION Send( ch, id, nEach )
   LOCAL i
   FOR i := 1 TO nEach
      ch:SendAsync( { id, i } ):Wait()
   NEXT
RETURN NIL

// Takes until the channel is closed and empty: the out-of-order count (a producer's number below one already seen).
STATIC FUNCTION Consume( ch, nProducers, aSeen, oLock )
   LOCAL x, aLast := AFill( Array( nProducers ), 0 ), nBad := 0
   FOR EACH x IN ch
      IF x[ 2 ] <= aLast[ x[ 1 ] ]
         nBad++
      ENDIF
      aLast[ x[ 1 ] ] := x[ 2 ]
      SYNCHRONIZED oLock
         aSeen[ x[ 1 ], x[ 2 ] ] := iif( aSeen[ x[ 1 ], x[ 2 ] ] == NIL, 1, aSeen[ x[ 1 ], x[ 2 ] ] + 1 )
      END
   NEXT
RETURN nBad

// Every kind of value sent and received back, 200 rounds, with collections between.
STATIC FUNCTION Kinds()
   LOCAL ch := Channel( 3 ), aValues, i, k, t, o := Box():New( "inside" )
   aValues := { NIL, 0, -1.5, 10 ^ 300, "", Replicate( "x", 100000 ), SToD( "20261116" ), .T., .F., ;
                AFill( Array( 5000 ), 7 ), { "k" => { 1, 2 } }, o, {| n | n * 3 } }
   t := TaskRun( {|| Echo( ch, Len( aValues ) * 200 ) } )
   FOR i := 1 TO 200
      FOR k := 1 TO Len( aValues )
         ch:SendAsync( aValues[ k ] ):Wait()
      NEXT
      IF i % 50 == 0
         __GcCollect()
      ENDIF
   NEXT
RETURN t:Result

STATIC FUNCTION Echo( ch, n )
   LOCAL i, x, nGood := 0, k
   LOCAL aRef := { NIL, 0, -1.5, 10 ^ 300, "", 100000, SToD( "20261116" ), .T., .F., 5000, 1, "inside", 6 }
   FOR i := 1 TO n
      x := ch:ReceiveAsync():Result
      k := ( i - 1 ) % Len( aRef ) + 1
      DO CASE
      CASE k == 6
         nGood += iif( Len( x ) == aRef[ k ] .AND. Right( x, 1 ) == "x", 1, 0 )
      CASE k == 10
         nGood += iif( Len( x ) == aRef[ k ] .AND. x[ 5000 ] == 7, 1, 0 )
      CASE k == 11
         nGood += iif( x[ "k" ][ 2 ] == 2, 1, 0 )
      CASE k == 12
         nGood += iif( x:cName == aRef[ k ], 1, 0 )
      CASE k == 13
         nGood += iif( Eval( x, 2 ) == aRef[ k ], 1, 0 )
      OTHERWISE
         nGood += iif( ValType( x ) == ValType( aRef[ k ] ) .AND. x == aRef[ k ], 1, 0 )
      ENDCASE
   NEXT
RETURN nGood == n

// n receivers wait, every third one canceled; then n values are sent: the receivers not canceled take one each, in the
// order they came (the j-th of them value j), and the values left are still in the channel: none lost.
STATIC FUNCTION CanceledReceivers( n )
   LOCAL ch := Channel(), a := {}, aS := {}, k, nTaken := 0, nCanceled := 0, nWrong := 0, s
   FOR k := 1 TO n
      s := CancellationTokenSource()
      AAdd( aS, s )
      AAdd( a, ch:ReceiveAsync( s:Token ) )
   NEXT
   FOR k := 3 TO n STEP 3
      aS[ k ]:Cancel()
      a[ k ]:Wait()
   NEXT
   FOR k := 1 TO n
      ch:TrySend( k )
   NEXT
   FOR k := 1 TO n
      a[ k ]:Wait()
      IF a[ k ]:IsCanceled
         nCanceled++
      ELSE
         nTaken++
         IF a[ k ]:Result != k - Int( k / 3 )
            nWrong++
         ENDIF
      ENDIF
   NEXT
RETURN LTrim( Str( nTaken ) ) + " taken (" + LTrim( Str( nWrong ) ) + " out of order), " + ;
       LTrim( Str( nCanceled ) ) + " canceled, " + LTrim( Str( ch:Count() ) ) + " left, sum " + ;
       LTrim( Str( Sum( a ) + Rest( ch ) ) )

STATIC FUNCTION Sum( a )
   LOCAL t, n := 0
   FOR EACH t IN a
      IF t:IsCompleted .AND. !t:IsCanceled
         n += t:Result
      ENDIF
   NEXT
RETURN n

STATIC FUNCTION Rest( ch )
   LOCAL n := 0, aSlot := { NIL }
   DO WHILE ch:TryReceive( aSlot )
      n += aSlot[ 1 ]
   ENDDO
RETURN n

// Senders blocked on a full channel and receivers waiting on an empty one when it closes: all fail, none hangs; the
// values kept are received after.
STATIC FUNCTION CloseInFlow()
   LOCAL full := Channel( 1 ), empty := Channel(), aS := {}, aR := {}, k, nFailed := 0
   full:TrySend( "kept" )
   FOR k := 1 TO 100
      AAdd( aS, full:SendAsync( k ) )
      AAdd( aR, empty:ReceiveAsync() )
   NEXT
   TaskRun( {|| full:Close(), empty:Close() } ):Wait()
   AEval( aS, {| t | t:Wait(), nFailed += iif( t:IsFailed, 1, 0 ) } )
   AEval( aR, {| t | t:Wait(), nFailed += iif( t:IsFailed, 1, 0 ) } )
RETURN LTrim( Str( nFailed ) ) + " failed, then " + full:ReceiveAsync():Result

// n receivers wait in order; n values sent: receiver k gets value k.
STATIC FUNCTION ManyWaiting( n )
   LOCAL ch := Channel(), a := {}, k, nGood := 0
   FOR k := 1 TO n
      AAdd( a, ch:ReceiveAsync() )
   NEXT
   FOR k := 1 TO n
      ch:SendAsync( k )
   NEXT
   FOR k := 1 TO n
      nGood += iif( a[ k ]:Result == k, 1, 0 )
   NEXT
RETURN nGood

CLASS Box
   VAR cName
   METHOD New( c ) INLINE ::cName := c, Self
ENDCLASS
