// One worker (//WORKERS:1 in tasks_handoff.args): a task that waits for another blocks the worker, so another takes
// its place (blocking handoff): a task waiting for a completion source a later task sets (on a worker: the start
// procedure waits with a limit, which runs nothing inline), tasks that each wait for the next one's source (a chain
// longer than the target), a task that waits for one it started (run inline), and many short tasks behind blocked
// ones. Then, the one worker kept busy, the start procedure runs a task inline inside Type(): its error is its own (it
// fails with the Error object, Type() gives "UE"), and nothing leaks out of Type().
MEMVAR pTask

PROCEDURE Main()
   LOCAL a := {}, k, n := 0, aSource := {}, t, aStop := { .F., .F. }, oBusy
   t := TaskRun( {|| Handoff() } )
   DO WHILE !t:Wait( 1000 )
   ENDDO
   ? "handoff:", t:Result
   FOR k := 1 TO 20
      AAdd( aSource, TaskCompletionSource() )
   NEXT
   FOR k := 1 TO 19
      AAdd( a, TaskRun( {| i | aSource[ i ]:SetResult( aSource[ i + 1 ]:Task:Result + 1 ) }, k ) )
   NEXT
   AAdd( a, TaskRun( {|| aSource[ 20 ]:SetResult( 0 ) } ) )
   ? "chain:", aSource[ 1 ]:Task:Result
   ? "inline:", TaskRun( {|| TaskRun( {|| 5 } ):Result * 2 } ):Result
   a := {}
   FOR k := 1 TO 200
      AAdd( a, TaskRun( {| i | i }, k ) )
   NEXT
   AEval( a, {| t | n += t:Result } )
   ? "short:", n

   oBusy := TaskRun( {|| Spin( aStop ) } )
   DO WHILE !aStop[ 2 ] // until the worker runs it
   ENDDO
   PRIVATE pTask := TaskRun( {|| 1 + "a" } )
   ? "inline in Type():", Type( "pTask:Result" ), pTask:IsFailed, ValType( pTask:Error ), pTask:Error:operation
   aStop[ 1 ] := .T.
   oBusy:Wait()
   ? "after:", oBusy:IsCompleted
RETURN

// The worker stays busy (running, not blocked: no other worker starts) until told to stop.
STATIC FUNCTION Spin( aStop )
   aStop[ 2 ] := .T.
   DO WHILE !aStop[ 1 ]
   ENDDO
RETURN NIL

STATIC FUNCTION Handoff()
   LOCAL s := TaskCompletionSource()
   TaskRun( {|| s:SetResult( "done" ) } )
RETURN s:Task:Result
