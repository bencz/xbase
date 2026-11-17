// QUIT in a task ends the program with the exit code ErrorLevel() set: the start procedure, waiting for that task,
// goes no further, but its FINALLY runs; a wait there (for a continuation whose antecedent never completes) ends at
// once, since the program is ending; then the EXIT procedures run. The task quits once the start procedure is inside
// TRY (a gate it opens there), however loaded the machine is.
PROCEDURE Main()
   LOCAL s := TaskCompletionSource(), g := TaskCompletionSource(), c, t
   c := s:Task:ContinueWith( {|| "never" } )
   t := TaskRun( {|| g:Task:Wait(), Quits() } )
   TRY
      g:SetResult( .T. )
      t:Wait()
      ? "not reached"
   FINALLY
      ? "finally"
      c:Wait()
      ? "after the wait", c:IsCompleted
   END
RETURN

EXIT PROCEDURE Bye()
   ? "bye", ErrorLevel()
RETURN

STATIC FUNCTION Quits()
   ErrorLevel( 5 )
   QUIT
RETURN NIL
