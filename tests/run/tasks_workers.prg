// The scheduler's pool, with two workers that run tasks at once (//WORKERS:2 in tasks_workers.args): TaskWorkers()
// gives { the workers alive, the number that run at once, the most there may be }, none alive before any task. Tasks
// that block (Lock() on a mutex held 600 ms) have their places filled: at once up to twice the target, past that
// paced (a new worker 25 ms after the last at least): the pool grows past the target but stays within that pace,
// every task ends, and the workers beyond the target end once idle (2 seconds). On a worker's stack (a task waited for
// with a limit, which never runs it inline) a recursion 4000 calls deep runs, and one without end is the stack error
// the task's own sequence recovers from, as on a thread of ThreadStart() (the program's thread: stack_overflow.prg). It
// runs alone (tasks_workers.serial): its recursions take their stacks whole, and its pace is timed.
#include "error.ch"

PROCEDURE Main()
   LOCAL a := TaskWorkers(), oM := Mutex(), aT := {}, k, n, nStart, t, oT, aCount := { 0 }, aMid
   ? "before any task:", Len( a ), a[ 1 ], a[ 2 ], a[ 3 ] >= a[ 2 ] .AND. a[ 3 ] <= 1024

   oM:Lock()
   nStart := ElapsedMs()
   FOR k := 1 TO 40
      AAdd( aT, TaskRun( {|| oM:Lock(), aCount[ 1 ]++, oM:Unlock() } ) )
   NEXT
   TaskDelay( 600 ):Wait()
   aMid := TaskWorkers()
   n := ElapsedMs() - nStart
   oM:Unlock()
   ? "blocked tasks: grew", aMid[ 1 ] > aMid[ 2 ], "paced", aMid[ 1 ] <= 2 * aMid[ 2 ] + Int( n / 25 ) + 1, ;
     "ended", TaskWhenAll( aT ):Wait( 30000 ), aCount[ 1 ]
   nStart := ElapsedMs()
   DO WHILE TaskWorkers()[ 1 ] > TaskWorkers()[ 2 ] .AND. ElapsedMs() - nStart < 10000
      TaskDelay( 50 ):Wait()
   ENDDO
   ? "idle surplus ends:", TaskWorkers()[ 1 ] <= TaskWorkers()[ 2 ]

   t := TaskRun( {|| Depth( 4000 ) } )
   ? "deep on a worker:", t:Wait( 30000 ), t:Result
   t := TaskRun( {|| Endless() } )
   ? "endless on a worker:", t:Wait( 30000 ), t:Result
   oT := ThreadStart( {|| Endless() } )
   ? "endless on a thread:", oT:Join()
RETURN

STATIC FUNCTION Depth( n )
RETURN iif( n == 0, 0, 1 + Depth( n - 1 ) )

// The stack error, recovered: its kind and how deep the calls went (past 1000 on any thread).
STATIC FUNCTION Endless()
   LOCAL e, aDepth := { 0 }
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      Down( aDepth )
   RECOVER USING e
      RETURN iif( e:genCode == EG_LIMIT .AND. e:description == "Stack overflow" .AND. aDepth[ 1 ] > 1000, ;
                  "stack error after more than 1000 calls", "other: " + e:description )
   END SEQUENCE
RETURN "no error"

STATIC FUNCTION Down( aDepth )
   aDepth[ 1 ]++
RETURN Down( aDepth ) + 1
