// A fork-join: each task starts tasks and waits for TaskWhenAll() of them (:Result, :Wait()), deeper than the workers
// are many; the waiter runs the ones still to start, so no worker waits for tasks no worker is left to run. The same
// with a TaskWhenAll() of a TaskWhenAll(). A waiter of TaskWhenAny() runs none: it would wait for a task that waits
// for what the waiter does next.
PROCEDURE Main()
   ? Fork( 0 ), ForkWait( 0 ), ForkNested( 0 ), ForkAny()
   RETURN

STATIC FUNCTION Fork( d )
   LOCAL a := {}, k, n := 0, x
   IF d == 4
      RETURN 1
   ENDIF
   FOR k := 1 TO 8
      AAdd( a, TaskRun( {|| Fork( d + 1 ) } ) )
   NEXT
   FOR EACH x IN TaskWhenAll( a ):Result
      n += x
   NEXT
   RETURN n

STATIC FUNCTION ForkWait( d )
   LOCAL a := {}, k, n := 0, t
   IF d == 3
      RETURN 2
   ENDIF
   FOR k := 1 TO 10
      AAdd( a, TaskRun( {|| ForkWait( d + 1 ) } ) )
   NEXT
   TaskWhenAll( a ):Wait()
   FOR EACH t IN a
      n += t:Result
   NEXT
   RETURN n

STATIC FUNCTION ForkNested( d )
   LOCAL a := {}, b := {}, k, n := 0, x
   IF d == 3
      RETURN 3
   ENDIF
   FOR k := 1 TO 6
      AAdd( a, TaskRun( {|| ForkNested( d + 1 ) } ) )
      AAdd( b, TaskRun( {|| ForkNested( d + 1 ) } ) )
   NEXT
   FOR EACH x IN TaskWhenAll( { TaskWhenAll( a ), TaskWhenAll( b ) } ):Result
      AEval( x, {| v | n += v } )
   NEXT
   RETURN n

STATIC FUNCTION ForkAny()
   LOCAL oSource, oSlow, oFirst, k, n := 0
   FOR k := 1 TO 20
      oSource := TaskCompletionSource()
      oSlow := TaskRun( {|| oSource:Task:Result } )
      oFirst := TaskWhenAny( { oSlow, TaskDelay( 5 ) } ):Result
      oSource:SetResult( k )
      n += iif( oFirst == oSlow, 0, oSlow:Result )
   NEXT
   RETURN n
