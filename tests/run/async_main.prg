// An ASYNC start procedure: its first step runs with the program's arguments, then the runtime waits for its task
// before the EXIT procedures run and the program ends; the steps after an AWAIT run on workers, and the exit code is
// the one they set.
ASYNC PROCEDURE Main( cFirst, cSecond )
   LOCAL n := 0, k
   ? "arguments:", cFirst, cSecond
   FOR k := 1 TO 50
      n += AWAIT TaskRun( {| i | i }, k )
   NEXT
   ? "sum:", n
   AWAIT TaskDelay( 5 )
   ? "after delay"
   ErrorLevel( 3 )
RETURN

EXIT PROCEDURE Bye()
   ? "exit procedure"
RETURN
