// ASYNC functions under load, checked exactly: 10000 handlers suspended at once (a delay, then a value from a channel
// each), all resumed once with their own state; a pipeline of ASYNC stages over bounded channels that keeps each
// producer's order and loses nothing; 20000 suspensions racing the completion of what they await (set from other
// threads at the same moment); a recursion 2000 calls deep that suspends at its bottom; and memory: functions that
// complete at once or after suspending leave nothing behind, and those abandoned while suspended (what they await is
// dropped) are collected, the tables they had open closed.
PROCEDURE Main()
   ? "handlers:", AWAIT Handlers( 10000 )
   ? "pipeline:", AWAIT Pipeline( 4, 3, 2000 )
   ? "races:", Races( 20000 )
   ? "deep:", AWAIT Deep( 2000 )
   ? "memory, completed:", Kept( {|| Completed( 50000 ) } )
   ? "memory, abandoned:", Kept( {|| Abandoned( 20000 ) } )
   ? "abandoned table closed:", AbandonedTable()
RETURN

// n handlers, each waiting k % 20 + 1 ms, then for its own value on a channel the start procedure fills afterwards.
ASYNC FUNCTION Handlers( n )
   LOCAL ch := Channel(), a := {}, k, x, nSum := 0, nBad := 0
   FOR k := 1 TO n
      AAdd( a, Handler( k, ch ) )
   NEXT
   FOR k := 1 TO n
      ch:TrySend( k )
   NEXT
   FOR EACH x IN AWAIT TaskWhenAll( a )
      nSum += x[ 1 ]
      IF x[ 2 ] != x[ 3 ]
         nBad++
      ENDIF
   NEXT
RETURN LTrim( Str( Len( a ) ) ) + " done, sum " + iif( nSum == n * ( n + 1 ), "exact", "WRONG" ) + ", " + ;
       LTrim( Str( nBad ) ) + " lost their state"

ASYNC FUNCTION Handler( k, ch )
   LOCAL cMine := "h" + LTrim( Str( k ) ), v
   AWAIT TaskDelay( k % 20 + 1 )
   v := AWAIT ch:ReceiveAsync()
RETURN { v + k, cMine, "h" + LTrim( Str( k ) ) }

// nProducers send {id, 1 .. nEach} into a channel of 8; nStages workers forward each value to an output channel of 8;
// the consumer checks that each producer's sequence keeps its order through each worker and that all arrive once.
ASYNC FUNCTION Pipeline( nProducers, nStages, nEach )
   LOCAL cin := Channel( 8 ), cout := Channel( 8 ), aP := {}, aS := {}, k, nGot := 0, nBad := 0, x, aSeen
   aSeen := AFill( Array( nProducers ), 0 )
   FOR k := 1 TO nProducers
      AAdd( aP, Produce( cin, k, nEach ) )
   NEXT
   FOR k := 1 TO nStages
      AAdd( aS, Forward( cin, cout ) )
   NEXT
   TaskWhenAll( aP ):ContinueWith( {|| cin:Close() } )
   TaskWhenAll( aS ):ContinueWith( {|| cout:Close() } )
   DO WHILE ( x := AWAIT Receive( cout ) ) != NIL
      nGot++
      aSeen[ x[ 1 ] ]++
   ENDDO
   FOR k := 1 TO nProducers
      IF aSeen[ k ] != nEach
         nBad++
      ENDIF
   NEXT
   FOR EACH x IN AWAIT TaskWhenAll( aS )
      nBad += x
   NEXT
RETURN LTrim( Str( nGot ) ) + " received, " + LTrim( Str( nBad ) ) + " wrong"

ASYNC FUNCTION Produce( ch, id, n )
   LOCAL k
   FOR k := 1 TO n
      AWAIT ch:SendAsync( { id, k } )
   NEXT
RETURN NIL

// Forwards until the input closes: the values out of order (a producer's number not above the last seen from it).
ASYNC FUNCTION Forward( cin, cout )
   LOCAL x, hLast := { => }, nBad := 0
   DO WHILE ( x := AWAIT Receive( cin ) ) != NIL
      IF x[ 1 ] $ hLast .AND. x[ 2 ] <= hLast[ x[ 1 ] ]
         nBad++
      ENDIF
      hLast[ x[ 1 ] ] := x[ 2 ]
      AWAIT cout:SendAsync( x )
   ENDDO
RETURN nBad

// The next value, NIL once the channel is closed and empty.
ASYNC FUNCTION Receive( ch )
   LOCAL e
   TRY
      RETURN AWAIT ch:ReceiveAsync()
   CATCH e
      IF e:SubCode != 9030
         Break( e )
      ENDIF
   END
RETURN NIL

// Each function awaits a promise another thread completes at the same moment: suspended or not, each resumes once.
STATIC FUNCTION Races( n )
   LOCAL aS := {}, aT := {}, aR := {}, k, nSum := 0, x
   FOR k := 1 TO n
      AAdd( aS, TaskCompletionSource() )
   NEXT
   FOR k := 1 TO 4
      AAdd( aR, TaskRun( {| j | SetEvery( aS, j ) }, k ) )
   NEXT
   FOR k := 1 TO n
      AAdd( aT, Awaits( aS[ k ]:Task ) )
   NEXT
   AEval( aR, {| t | t:Wait() } )
   FOR EACH x IN TaskWhenAll( aT ):Result
      nSum += x
   NEXT
RETURN iif( nSum == n * ( n + 1 ), "exact", "WRONG" )

STATIC FUNCTION SetEvery( aS, j )
   LOCAL k
   FOR k := j TO Len( aS ) STEP 4
      aS[ k ]:SetResult( k )
   NEXT
RETURN NIL

ASYNC FUNCTION Awaits( oTask )
   LOCAL n := AWAIT oTask
RETURN n * 2

ASYNC FUNCTION Deep( n )
   IF n == 0
      AWAIT TaskDelay( 1 )
      RETURN 0
   ENDIF
RETURN 1 + AWAIT Deep( n - 1 )

// Whether what bWork leaves behind (the objects alive after a full collection) stays small.
STATIC FUNCTION Kept( bWork )
   LOCAL nBefore := __GcCollect(), x := Eval( bWork )
   __GcCollect()
   TaskRun( {|| NIL } ):Wait() // a step: the environments of the tasks collected close
RETURN iif( __GcCollect() - nBefore < 1000, "bounded", "kept " + LTrim( Str( __GcCollect() - nBefore ) ) ) + ;
       " (" + x + ")"

STATIC FUNCTION Completed( n )
   LOCAL k, nSum := 0
   FOR k := 1 TO n
      nSum += Sync( k ):Result
   NEXT
   FOR k := 1 TO n / 10
      nSum += Later( k ):Result
   NEXT
RETURN iif( nSum == n * ( n + 1 ) / 2 + n / 10 * ( n / 10 + 1 ) / 2, "exact", "WRONG" )

ASYNC FUNCTION Sync( k )
RETURN k

ASYNC FUNCTION Later( k )
   AWAIT TaskDelay( 0 )
RETURN k

STATIC FUNCTION Abandoned( n )
   LOCAL k, a := {}
   FOR k := 1 TO n
      AAdd( a, Waiter( TaskCompletionSource():Task, Replicate( "x", 100 ) ) )
   NEXT
RETURN LTrim( Str( Len( a ) ) ) + " suspended"

ASYNC FUNCTION Waiter( oTask, c )
   AWAIT oTask
RETURN c

// A function suspended with a table open EXCLUSIVE, then abandoned (made by a function that returned: nothing of
// its frame holds it): once collected, its table closes and opens again.
STATIC FUNCTION AbandonedTable()
   LOCAL c
   FErase( "async_stress.dbf" )
   dbCreate( "async_stress.dbf", { { "N", "N", 5, 0 } } )
   c := "open while held " + Hold()
   __GcCollect()
   TaskRun( {|| NIL } ):Wait()
   c += ", after " + iif( TryOpen(), "yes", "no" )
   FErase( "async_stress.dbf" )
RETURN c

STATIC FUNCTION Hold()
   Holder( TaskCompletionSource():Task )
RETURN iif( TryOpen(), "yes", "no" )

ASYNC FUNCTION Holder( oTask )
   USE async_stress NEW EXCLUSIVE
   AWAIT oTask
RETURN NIL

STATIC FUNCTION TryOpen()
   LOCAL l
   USE async_stress NEW EXCLUSIVE
   l := !NetErr() .AND. Used()
   USE
RETURN l
