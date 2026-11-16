// Tasks: 200,000 short tasks (TaskRun, each a block with an argument) started from the program's thread and joined by
// TaskWhenAll, then a 50,000-link ContinueWith chain and 20,000 tasks that each start and wait for another (run
// inline): what starting, scheduling, completing and waiting for a task costs.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), a := Array( 200000 ), k, r, s := 0, c := TaskFromResult( 0 )
   FOR k := 1 TO Len( a )
      a[ k ] := TaskRun( {| i | i % 7 }, k )
   NEXT
   r := TaskWhenAll( a ):Result
   AEval( r, {| x | s += x } )
   FOR k := 1 TO 50000
      c := c:ContinueWith( {| p | p:Result + 1 } )
   NEXT
   s += c:Result
   FOR k := 1 TO 20000
      s += TaskRun( {|| TaskRun( {|| 1 } ):Result } ):Result
   NEXT
   ? "tasks", ElapsedMs() - t, IIf( s == 599997 + 50000 + 20000, "ok", "WRONG" )
   RETURN
