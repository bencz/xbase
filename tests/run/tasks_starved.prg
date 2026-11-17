// Tasks that starve: 1100 tasks wait with Lock() for a mutex the start procedure holds, each holding its worker, until
// no other worker can start (the most there may be, or the threads the system lets start); the tasks left in the
// queue then wait while every worker waits, and the program's standard error is told so (tasks_starved.err) after two
// seconds (the mutex is released after six, time for the threads to start on a loaded machine). Once it is, every
// task takes it in turn and ends. It runs alone (tasks_starved.serial): it starts a thousand threads.
PROCEDURE Main()
   LOCAL oM := Mutex(), a := {}, k, aCount := { 0 }
   oM:Lock()
   FOR k := 1 TO 1100
      AAdd( a, TaskRun( {|| oM:Lock(), aCount[ 1 ]++, oM:Unlock() } ) )
   NEXT
   TaskDelay( 6000 ):Wait()
   oM:Unlock()
   ? "ended:", TaskWhenAll( a ):Wait( 60000 ), aCount[ 1 ]
RETURN
