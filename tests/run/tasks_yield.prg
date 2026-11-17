// TaskYield(): a task that completes as soon as something waits for it, with NIL: it is not completed until then;
// Wait() and Result complete it; AWAIT of it in an ASYNC function suspends the function, whose next step runs on the
// pool (a loop of 1000 such steps, and 200 functions at once, each with its own state) behind the work queued already:
// with one worker (//WORKERS:1 in tasks_yield.args), a function that yields until a task it started sets a flag lets
// that task run before its next step (one yield); ContinueWith(), TaskWhenAll(), TaskWhenAny(), TaskTimeout() and
// Wait() with a limit complete it too. The yield that lets the task run shows the suspension: without one, the
// function would loop on its worker and the task would never run.
PROCEDURE Main()
   LOCAL t := TaskYield(), u, a := {}, k, nSum := 0, x
   ? "queued work runs first:", Yields(), Yields(), Yields()
   ? "created:", t:IsCompleted
   ? "waited:", t:Wait(), t:IsCompleted, t:Result
   ? "result:", TaskYield():Result
   ? "steps:", AWAIT Steps( 1000 )
   FOR k := 1 TO 200
      AAdd( a, Steps( k ) )
   NEXT
   FOR EACH x IN AWAIT TaskWhenAll( a )
      nSum += Val( x )
   NEXT
   ? "functions:", nSum == 200 * 201 / 2
   u := TaskYield()
   ? "continue:", u:ContinueWith( {| v | ValType( v:Result ) + " after" } ):Result, u:IsCompleted
   ? "when all:", Len( TaskWhenAll( { TaskYield(), TaskYield() } ):Result )
   ? "when any:", TaskWhenAny( { TaskYield(), TaskDelay( 100000 ) } ):Result:IsCompleted
   ? "timeout:", TaskTimeout( TaskYield(), 100000 ):Result, "wait with a limit:", TaskYield():Wait( 100000 )
RETURN

// The yields UntilSet() takes on a worker, waited for with a limit (a wait without one runs a step of the task
// inline, on this thread, ahead of the worker's queue).
STATIC FUNCTION Yields()
   LOCAL t := TaskRun( {|| UntilSet() } ), u
   t:Wait( 100000 )
   u := t:Result
   u:Wait( 100000 )
RETURN u:Result

// Starts a task that sets a flag, and yields until it is set (at most 100000 times): the yields it took.
ASYNC FUNCTION UntilSet()
   LOCAL aFlag := { .F. }, n := 0
   TaskRun( {|| aFlag[ 1 ] := .T. } )
   DO WHILE ! aFlag[ 1 ] .AND. n < 100000
      AWAIT TaskYield()
      n++
   ENDDO
RETURN n

// n steps, each after a yield: the count of them.
ASYNC FUNCTION Steps( n )
   LOCAL k, nSteps := 0
   FOR k := 1 TO n
      AWAIT TaskYield()
      nSteps++
   NEXT
RETURN LTrim( Str( nSteps ) )
