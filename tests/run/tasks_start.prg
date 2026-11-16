// The start procedure returns a task (as an asynchronous one will): the program waits for it before the EXIT
// procedures run, and an error that failed it is reported (exit code 1).
FUNCTION Main()
   ? "main returns"
RETURN TaskRun( {|| Work() } )

EXIT PROCEDURE Bye()
   ? "bye", ErrorLevel()
RETURN

STATIC FUNCTION Work()
   LOCAL s := TaskCompletionSource()
   TaskRun( {|| s:SetResult( 1 ) } )
   ? "task ran", s:Task:Result
RETURN 1 + "a"
