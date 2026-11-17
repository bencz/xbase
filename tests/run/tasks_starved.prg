// Tasks that starve: 1100 tasks wait with Lock() for a mutex the start procedure holds, each holding its worker. The
// pool fills their places at once up to twice the number of workers that run at once, then a new worker at a pace (one
// 25 ms after the last at least), up to the most there may be: after six seconds it has grown past the target and
// within that pace (TaskWorkers()). The tasks left in the queue wait while every worker waits, and the program's
// standard error is told so (tasks_starved.err) after two seconds. Once the mutex is released, every task takes it in
// turn and ends. It runs alone (tasks_starved.serial): it starts a few hundred threads.
PROCEDURE Main()
   LOCAL oM := Mutex(), a := {}, k, aCount := { 0 }, nStart := ElapsedMs(), aW, n
   oM:Lock()
   FOR k := 1 TO 1100
      AAdd( a, TaskRun( {|| oM:Lock(), aCount[ 1 ]++, oM:Unlock() } ) )
   NEXT
   TaskDelay( 6000 ):Wait()
   aW := TaskWorkers()
   n := ElapsedMs() - nStart
   ? "workers:", aW[ 1 ] > aW[ 2 ], aW[ 1 ] <= 2 * aW[ 2 ] + Int( n / 25 ) + 1, aW[ 1 ] <= aW[ 3 ]
   oM:Unlock()
   ? "ended:", TaskWhenAll( a ):Wait( 60000 ), aCount[ 1 ]
RETURN
