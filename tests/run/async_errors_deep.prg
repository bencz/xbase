// An error that fails 3000 nested ASYNC functions, each awaiting the next: the report keeps XB_CALLS_SHOWN lines of
// where the tasks were waited for at most (the last one says that more were left out), then where the start procedure
// waited, and it is quick (each addition stops at that many lines).
PROCEDURE Main()
   ? AWAIT Down( 3000 )
RETURN

ASYNC FUNCTION Down( n )
   IF n == 0
      AWAIT TaskDelay( 1 )
      RETURN 1 + "x"
   ENDIF
RETURN AWAIT Down( n - 1 )
