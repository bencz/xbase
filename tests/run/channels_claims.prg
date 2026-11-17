// Channels whose waiters are completed outside the channel's lock (claimed under it, so no token cancels them after):
// senders and receivers with tokens that other threads cancel at random while values flow: each send whose task
// completed (.T.) is received exactly once, none whose task was canceled is received, and each receive whose task
// completed got one value (a count per value). Long-lived receivers stay waiting while 20000 receives with tokens are
// made and canceled: a cycle costs about the same with 16383 receivers waiting (their ring just full) as with one (the
// ring grows rather than being compacted again and again for one place), and the waiters still get their values in
// order. A burst of a million values held, then received: the ring gives its room
// back (the heap's memory comes back). A send made to wait whose channel frees room before it is queued completes at
// once.
PROCEDURE Main()
   ? "claims:", Claims( 4, 20000 )
   ? "long-lived waiters:", Waiters( 16383, 20000 )
   ? "burst:", Burst( 1000000 )
RETURN

// P senders with tokens on a channel of capacity 3; a canceller cancels some sends' tokens; two receivers, some with
// tokens canceled too. Checked: the values counted by the receivers are exactly those whose send completed.
STATIC FUNCTION Claims( nP, n )
   LOCAL ch := Channel( 3 ), aSent := Array( nP * n ), aGot := Array( nP * n ), aT := {}, k, i, nBad := 0
   LOCAL aSources := Array( 64 )
   AFill( aGot, 0 )
   FOR k := 1 TO Len( aSources )
      aSources[ k ] := CancellationTokenSource()
   NEXT
   FOR k := 1 TO 2
      AAdd( aT, TaskRun( {|| Receive( ch, aGot, aSources ) } ) )
   NEXT
   AAdd( aT, TaskRun( {|| Canceller( aSources, ch ) } ) )
   FOR k := 1 TO nP
      AAdd( aT, TaskRun( {| p | Send( ch, p, n, aSent, aSources ) }, k ) )
   NEXT
   FOR k := 4 TO Len( aT ) // the senders
      aT[ k ]:Wait()
   NEXT
   ch:Close()
   AEval( aT, {| x | x:Wait() } )
   FOR i := 1 TO Len( aSent )
      IF aGot[ i ] != IIf( aSent[ i ], 1, 0 )
         nBad++
      ENDIF
   NEXT
RETURN IIf( nBad == 0, "each sent value received once, no canceled one", LTrim( Str( nBad ) ) + " WRONG" )

STATIC PROCEDURE Send( ch, p, n, aSent, aSources )
   LOCAL i, t, v
   FOR i := 1 TO n
      v := ( p - 1 ) * n + i
      t := ch:SendAsync( v, aSources[ v % Len( aSources ) + 1 ]:Token )
      t:Wait()
      aSent[ v ] := t:IsCompleted .AND. ! t:IsCanceled .AND. ! t:IsFailed
   NEXT
RETURN

STATIC PROCEDURE Receive( ch, aGot, aSources )
   LOCAL t, k := 0
   DO WHILE .T.
      t := ch:ReceiveAsync( IIf( ++k % 5 == 0, aSources[ k % Len( aSources ) + 1 ]:Token, NIL ) )
      t:Wait()
      IF t:IsFailed
         EXIT // closed
      ENDIF
      IF ! t:IsCanceled
         aGot[ t:Result ]++
      ENDIF
   ENDDO
RETURN

// Cancels the sources one after another, each replaced by a new one, until the channel closes.
STATIC PROCEDURE Canceller( aSources, ch )
   LOCAL k := 0
   DO WHILE ! ch:IsClosed
      aSources[ k % Len( aSources ) + 1 ]:Cancel()
      aSources[ k % Len( aSources ) + 1 ] := CancellationTokenSource()
      k++
   ENDDO
RETURN

STATIC FUNCTION Waiters( nL, n )
   LOCAL ch := Channel(), aW := {}, i, nOne, nMany, c, lOrder := .T.
   ch:ReceiveAsync()
   nOne := Cycles( ch, n ) // warms up too
   nOne := Cycles( ch, n )
   ch:TrySend( 0 ) // that one served
   FOR i := 1 TO nL
      AAdd( aW, ch:ReceiveAsync() )
   NEXT
   nMany := Cycles( ch, n )
   c := IIf( nMany < 4 * nOne + 50, "cycles cost the same", "SLOW: " + Str( nOne ) + Str( nMany ) )
   FOR i := 1 TO nL
      ch:TrySend( i )
   NEXT
   FOR i := 1 TO nL
      lOrder := lOrder .AND. aW[ i ]:Result == i
   NEXT
RETURN c + IIf( lOrder, ", every waiter served in order", ", WRONG ORDER" ) + IIf( ch:Count() == 0, ", none left", "" )

// Milliseconds to make and cancel n receives with tokens on ch.
STATIC FUNCTION Cycles( ch, n )
   LOCAL i, s, t := ElapsedMs()
   FOR i := 1 TO n
      s := CancellationTokenSource()
      ch:ReceiveAsync( s:Token )
      s:Cancel()
   NEXT
RETURN ElapsedMs() - t

STATIC FUNCTION Burst( n )
   LOCAL ch := Channel(), i, nBase, nPeak, s := 0, a := { NIL }
   __GcCollect()
   nBase := __GcMemory()[ 2 ]
   FOR i := 1 TO n
      ch:TrySend( i )
   NEXT
   nPeak := __GcMemory()[ 2 ]
   FOR i := 1 TO n
      ch:TryReceive( a )
      s += a[ 1 ]
   NEXT
   __GcCollect()
   __GcCollect()
RETURN IIf( s == n * ( n + 1 ) / 2, "all received", "WRONG" ) + ;
   IIf( nPeak > nBase + 16 * 1048576, ", held", ", small" ) + ;
   IIf( __GcMemory()[ 2 ] < nBase + ( nPeak - nBase ) / 2, ", room given back", ", KEPT" )
