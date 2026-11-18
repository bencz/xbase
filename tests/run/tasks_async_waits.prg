// Waits that suspend an ASYNC function instead of holding its worker, with two workers (//WORKERS:2 in
// tasks_async_waits.args): Mutex:LockAsync() granting 400 waiters in the order they came while the holder keeps the
// lock across an AWAIT; AWAIT SYNCHRONIZED (HoldAsync) around a read and a write with an AWAIT between, exact over 300
// functions; the lock held by a function across its steps (another thread cannot take it, the function releases it
// from a later step); a flow that holds the lock already (at once, taken again: released as many times); a wait with a
// time limit (.F., NIL for HoldAsync), one granted before its limit (.T.), and one a token cancels (passed over: the
// next waiter gets the lock); blocking waiters beside asynchronous ones; a flow with two waiters in line and another
// flow's between them (its second gets the lock with its first); Condition:Wait() handing the lock to an asynchronous
// waiter; Semaphore:WaitAsync() letting at most its places in at once (and that many: those inside wait on a gate
// opened once all 300 are started), its order after a canceled waiter, and its limit; argument errors.
PROCEDURE Main()
   LOCAL oM := Mutex(), oS, a := {}, k, aLog := {}, aCount := { 0 }, oIn, oMax, s, t, e

   AAdd( a, Holder( oM ) )
   FOR k := 1 TO 400
      AAdd( a, Logged( oM, aLog, k ) )
   NEXT
   ? "in order:", TaskWhenAll( a ):Wait( 30000 ), Len( aLog ), InOrder( aLog ), oM:TryLock()
   oM:Unlock()

   a := {}
   FOR k := 1 TO 300
      AAdd( a, Guarded( oM, aCount ) )
   NEXT
   ? "synchronized:", TaskWhenAll( a ):Wait( 30000 ), aCount[ 1 ]

   s := TaskCompletionSource()
   t := Keeps( oM, s:Task )
   ? "held across steps:", TaskRun( {|| oM:TryLock() } ):Result, s:SetResult( 1 ), t:Wait( 30000 ), t:Result, ;
     oM:TryLock()
   oM:Unlock()

   oM:Lock()
   t := oM:HoldAsync():Result
   ? "own lock:", oM:LockAsync():Result, t:ClassName()
   oM:Unlock()
   t:Dispose()
   ? "still held:", TaskRun( {|| oM:TryLock() } ):Result
   oM:Unlock()
   ? "released three times:", TaskRun( {|| oM:TryLock() .AND. oM:Unlock() == NIL } ):Result

   oM:Lock()
   ? "time limit:", TaskRun( {|| oM:LockAsync( 30 ):Result } ):Result, ;
     TaskRun( {|| oM:HoldAsync( 30 ):Result } ):Result
   s := CancellationTokenSource()
   t := TaskRun( {|| oM:LockAsync( , s:Token ) } ):Result
   e := TakesAndGives( oM )
   s:Cancel()
   t:Wait( 30000 )
   oM:Unlock()
   ? "canceled:", t:IsCanceled, "next waiter:", e:Wait( 30000 ), e:Result
   ? "mixed:", Mixed( oM )

   oM:Lock()
   t := TwoInLine( oM, aLog := {} )
   oM:Unlock()
   ? "two of one flow:", t:Wait( 30000 ), t:Result, ArrayText( aLog )
   oM:Lock()
   t := Within( oM )
   oM:Unlock()
   ? "granted in time:", t:Wait( 30000 ), t:Result
   ? "condition releases:", Notified( oM )

   oS := Semaphore( 3 )
   oIn := AtomicInt()
   oMax := AtomicInt()
   a := {}
   aLog := {}
   s := TaskCompletionSource()
   FOR k := 1 TO 300
      AAdd( a, Limited( oS, oIn, oMax, aLog, k, s:Task ) )
   NEXT
   s:SetResult( .T. )
   ? "semaphore:", TaskWhenAll( a ):Wait( 30000 ), oMax:Value, oS:Count(), Len( aLog )
   ? "semaphore order:", SemaphoreOrder()
   ? "places:", oS:WaitAsync():Result, oS:WaitAsync():Result, oS:WaitAsync():Result, oS:WaitAsync( 30 ):Result, ;
     oS:WaitAsync( 0 ):Result, oS:Count()
   oS:Release( 3 )

   ErrorBlock( {| e | Break( e ) } )
   FOR EACH e IN { {|| oM:LockAsync( "x" ) }, {|| oM:HoldAsync( , 5 ) }, {|| oS:WaitAsync( {} ) }, ;
                   {|| oS:WaitAsync( 1, "token" ) } }
      ? "error:", Caught( e )
   NEXT
RETURN

// Holds the lock across a delay: the others queue meanwhile.
ASYNC FUNCTION Holder( oM )
   AWAIT oM:LockAsync()
   AWAIT TaskDelay( 50 )
   oM:Unlock()
RETURN NIL

ASYNC FUNCTION Logged( oM, aLog, k )
   AWAIT oM:LockAsync()
   AAdd( aLog, k )
   AWAIT TaskYield()
   oM:Unlock()
RETURN NIL

// A read and a write with another task's turn between: exact only if the lock is held across the AWAIT.
ASYNC FUNCTION Guarded( oM, aCount )
   LOCAL n
   AWAIT SYNCHRONIZED oM
      n := aCount[ 1 ]
      AWAIT TaskYield()
      aCount[ 1 ] := n + 1
   END
RETURN NIL

// Takes the lock, waits until the start procedure has seen another thread fail to take it, releases it from that
// later step (on a worker): done.
ASYNC FUNCTION Keeps( oM, oGate )
   AWAIT oM:LockAsync()
   AWAIT oGate
   oM:Unlock()
RETURN "released"

// Waits for the lock, then gives it back.
ASYNC FUNCTION TakesAndGives( oM )
   AWAIT oM:LockAsync()
   oM:Unlock()
RETURN "taken"

// One flow with two waiters in line and another flow's waiter between them: once the first is granted, the flow's
// second takes the lock at once (it would otherwise wait behind a waiter that waits for this flow).
ASYNC FUNCTION TwoInLine( oM, aLog )
   LOCAL t1 := oM:LockAsync(), oOther := Other( oM, aLog ), t2 := oM:LockAsync()
   AWAIT t1
   AWAIT t2
   AAdd( aLog, "both" )
   oM:Unlock()
   oM:Unlock()
   AWAIT oOther
RETURN "ended"

ASYNC FUNCTION Other( oM, aLog )
   AWAIT oM:LockAsync()
   AAdd( aLog, "other" )
   oM:Unlock()
RETURN NIL

// A wait with a limit granted before it: .T., which its timer does not change.
ASYNC FUNCTION Within( oM )
   LOCAL l := AWAIT oM:LockAsync( 300 )
   oM:Unlock()
   AWAIT TaskDelay( 400 )
RETURN l

// A Condition's Wait() releases the mutex to an asynchronous waiter, which runs and releases it before the wait ends.
STATIC FUNCTION Notified( oM )
   LOCAL c := Condition(), aDone := { .F. }, t
   oM:Lock()
   t := SetsDone( oM, aDone )
   c:Wait( oM, 2000 )
   oM:Unlock()
RETURN t:Wait( 30000 ) .AND. aDone[ 1 ]

ASYNC FUNCTION SetsDone( oM, aDone )
   AWAIT oM:LockAsync()
   aDone[ 1 ] := .T.
   oM:Unlock()
RETURN NIL

// Waiters of a semaphore with no place: the first one's token canceled, then a place at a time: the others get them
// in their order.
STATIC FUNCTION SemaphoreOrder()
   LOCAL oS := Semaphore( 0 ), s := CancellationTokenSource(), aLog := {}, t1, t2, t3
   t1 := Place( oS, s:Token, aLog, 1 )
   t2 := Place( oS, NIL, aLog, 2 )
   t3 := Place( oS, NIL, aLog, 3 )
   s:Cancel()
   t1:Wait( 30000 )
   oS:Release()
   t2:Wait( 30000 )
   oS:Release()
   t3:Wait( 30000 )
RETURN iif( t1:IsFailed, "canceled", "?" ) + " " + ArrayText( aLog ) + " " + LTrim( Str( oS:Count() ) )

ASYNC FUNCTION Place( oS, oToken, aLog, k )
   AWAIT oS:WaitAsync( , oToken )
   AAdd( aLog, k )
   oS:Release()
RETURN NIL

STATIC FUNCTION ArrayText( a )
   LOCAL c := "", x
   FOR EACH x IN a
      c += iif( Empty( c ), "", "," ) + iif( ValType( x ) == "N", LTrim( Str( x ) ), x )
   NEXT
RETURN c

// A thread waiting with Lock() and asynchronous waiters for one mutex: all get it.
STATIC FUNCTION Mixed( oM )
   LOCAL a := {}, k, aCount := { 0 }, oT
   oM:Lock()
   oT := ThreadStart( {|| oM:Lock(), aCount[ 1 ]++, oM:Unlock() } )
   FOR k := 1 TO 20
      AAdd( a, Counted( oM, aCount ) )
   NEXT
   oM:Unlock()
   oT:Join()
RETURN TaskWhenAll( a ):Wait( 30000 ) .AND. aCount[ 1 ] == 21

ASYNC FUNCTION Counted( oM, aCount )
   AWAIT SYNCHRONIZED oM
      aCount[ 1 ]++
   END
RETURN NIL

// Counts the functions inside at once (the most in oMax) and logs each; inside, waits for the gate (the first ones in
// stay until every function is started) and lets the thread go once.
ASYNC FUNCTION Limited( oS, oIn, oMax, aLog, k, tGate )
   LOCAL n, m
   AWAIT oS:WaitAsync()
   AAdd( aLog, k )
   n := oIn:Add( 1 )
   DO WHILE ( m := oMax:Value ) < n .AND. ! oMax:CompareExchange( m, n )
   ENDDO
   AWAIT tGate
   AWAIT TaskYield()
   oIn:Add( -1 )
   oS:Release()
RETURN NIL

STATIC FUNCTION InOrder( a )
   LOCAL k
   FOR k := 1 TO Len( a )
      IF a[ k ] != k
         RETURN .F.
      ENDIF
   NEXT
RETURN .T.

STATIC FUNCTION Caught( b )
   LOCAL e
   BEGIN SEQUENCE
      Eval( b )
   RECOVER USING e
      RETURN LTrim( Str( e:subCode ) ) + " " + e:operation
   END SEQUENCE
RETURN "none"
