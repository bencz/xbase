// Timers and cancellation, with one worker (//WORKERS:1 in tasks_timers.args): TaskDelay (at least its time; 0; a
// negative time), canceled by its token (before it is due, or when the token was canceled already); TaskTimeout (the
// task in time: its result, failure or cancellation; too late: failed with "Task timed out"); cancellation sources
// (Cancel, CancelAfter, linked sources, Register callbacks, ThrowIfCancellationRequested, cancelling twice); argument
// errors; the monitor: a task that runs long while another waits in the queue gets another worker beside it (the
// waiting task stops the long one: without the monitor, neither would end), also while the timers' thread sleeps until
// a timer far away; a delay or a CancelAfter of 0 or less done
// when the call returns; a later CancelAfter postponing an earlier one; Register after the cancellation, a link to a
// token canceled already, CancelAfter on a canceled source; and memory: 100000 timeouts settled long before their
// deadline, and 100000 sources linked to one long-lived token and disposed, keep nothing (the objects alive after a
// collection, counted).
PROCEDURE Main()
   LOCAL n, t, s, s2, s3, e, x, aFlag := { .F. }, oLong
   n := ElapsedMs()
   ? "delay:", TaskDelay( 60 ):Wait(), ElapsedMs() - n >= 60, TaskDelay( 0 ):Wait(), TaskDelay( -5 ):Result
   s := CancellationTokenSource()
   t := TaskDelay( 60000, s:Token )
   ? "token:", s:Token:ClassName(), s:ClassName(), t:IsCompleted, s:IsCancellationRequested
   s:Cancel()
   t:Wait()
   ? "canceled:", t:IsCanceled, s:IsCancellationRequested, s:Token:IsCancellationRequested
   t := TaskDelay( 60000, s:Token )
   t:Wait()
   ? "already:", t:IsCanceled

   ? "in time:", TaskTimeout( TaskRun( {|| 7 } ), 60000 ):Result, ;
     Ended( TaskTimeout( TaskRun( {|| Break( "x" ) } ), 60000 ) ):IsFailed, ;
     Ended( TaskTimeout( Canceled(), 60000 ) ):IsCanceled
   t := TaskTimeout( TaskDelay( 60000 ), 20 )
   t:Wait()
   ? "too late:", t:IsFailed, t:Error:subCode, t:Error:description, t:Error:operation

   s := CancellationTokenSource()
   x := s:Token:Register( {|| "callback" } )
   s:CancelAfter( 30 )
   ? "after:", x:Result, s:IsCancellationRequested
   s := CancellationTokenSource()
   s2 := CancellationTokenSource()
   s3 := CancellationTokenSource( s:Token, s2:Token )
   s2:Cancel()
   s2:Cancel()
   ? "linked:", s3:IsCancellationRequested, s:IsCancellationRequested
   s := CancellationTokenSource()
   ? "not yet:", Caught( {|| s:Token:ThrowIfCancellationRequested() } )
   s:Cancel()
   ? "throw:", Caught( {|| s:Token:ThrowIfCancellationRequested() } )
   ErrorBlock( {| e | Break( e ) } )
   FOR EACH e IN { {|| TaskDelay( "x" ) }, {|| TaskDelay( 1, 5 ) }, {|| TaskTimeout( 5, 1 ) }, ;
                   {|| TaskTimeout( TaskDelay( 1 ), "x" ) }, {|| CancellationTokenSource( 5 ) }, ;
                   {|| s:CancelAfter( "x" ) }, {|| s:Token:Register( 5 ) } }
      ? "error:", Caught( e )
   NEXT

   t := TaskDelay( 60000 )
   TaskDelay( 30 ):Wait() /* the timers' thread then sleeps until the timer a minute away, with nothing queued */
   oLong := TaskRun( {|| Spin( aFlag ) } )
   ? "monitor:", TaskRun( {|| aFlag[ 1 ] := .T. } ):Wait( 10000 ), oLong:Wait( 10000 )
   aFlag[ 1 ] := .T.

   s := CancellationTokenSource()
   s:CancelAfter( 0 )
   s2 := CancellationTokenSource()
   s2:CancelAfter( -1 )
   ? "at once:", TaskDelay( 0 ):IsCompleted, TaskDelay( -10 ):IsCompleted, s:IsCancellationRequested, ;
     s2:IsCancellationRequested
   s := CancellationTokenSource()
   s:CancelAfter( 30 )
   s:CancelAfter( 60000 )
   TaskDelay( 120 ):Wait()
   ? "postponed:", s:IsCancellationRequested
   s:CancelAfter( 20 )
   TaskDelay( 100 ):Wait()
   ? "sooner again:", s:IsCancellationRequested
   s := CancellationTokenSource()
   s:Cancel()
   ? "after the cancellation:", s:Token:Register( {|| "ran" } ):Result, ;
     CancellationTokenSource( s:Token ):IsCancellationRequested, Ended( TaskDelay( 60000, s:Token ) ):IsCanceled
   s:CancelAfter( 10 )
   ? "canceled stays:", s:IsCancellationRequested
   s := CancellationTokenSource()
   s2 := CancellationTokenSource( s:Token )
   s2:Dispose()
   s:Cancel()
   ? "disposed link:", s2:IsCancellationRequested, s:IsCancellationRequested
   ? "timeout of an ended task:", Timeouts0( 20000 )
   ? "memory, timeouts:", Kept( {|| Timeouts( 100000 ) } )
   ? "memory, linked sources:", Kept( {|| Linked( 100000 ) } )
RETURN

// Whether what bWork leaves behind (the objects alive after a full collection) stays small.
STATIC FUNCTION Kept( bWork )
   LOCAL nBefore := __GcCollect()
   Eval( bWork )
RETURN iif( __GcCollect() - nBefore < 10000, "bounded", "kept " + LTrim( Str( __GcCollect() - nBefore ) ) )

// TaskTimeout( t, 0 ) of tasks ended already: none times out (the task's end settles it before any timer).
STATIC FUNCTION Timeouts0( n )
   LOCAL i, nTimedOut := 0, t
   FOR i := 1 TO n
      t := TaskTimeout( TaskFromResult( i ), 0 )
      t:Wait()
      nTimedOut += iif( t:IsFailed, 1, 0 )
   NEXT
RETURN nTimedOut

STATIC FUNCTION Timeouts( n )
   LOCAL i
   FOR i := 1 TO n
      TaskTimeout( TaskFromResult( i ), 3600000 )
   NEXT
RETURN NIL

STATIC FUNCTION Linked( n )
   LOCAL s := CancellationTokenSource(), i, l
   FOR i := 1 TO n
      USING l := CancellationTokenSource( s:Token )
         l:CancelAfter( 3600000 )
      END
   NEXT
RETURN s

STATIC FUNCTION Ended( t )
   t:Wait()
RETURN t

STATIC FUNCTION Canceled()
   LOCAL s := TaskCompletionSource()
   s:SetCanceled()
RETURN s:Task

// Runs until the flag is set.
STATIC FUNCTION Spin( aFlag )
   DO WHILE !aFlag[ 1 ]
   ENDDO
RETURN NIL

STATIC FUNCTION Caught( b )
   LOCAL e
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      Eval( b )
   RECOVER USING e
      RETURN e:subSystem + "/" + LTrim( Str( e:genCode ) ) + "/" + LTrim( Str( e:subCode ) ) + " " + e:operation + ;
             " " + e:description
   END SEQUENCE
RETURN "none"
