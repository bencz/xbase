// An error that fails 3000 nested ASYNC functions, each awaiting the next: the report keeps XB_CALLS_SHOWN lines of
// where the tasks were waited for at most (the last one says that more were left out), then where the start procedure
// waited, and it is quick (each addition stops at that many lines). The innermost waits for a channel that receives
// its value only once every level has suspended, so each of them is a task waited for, however loaded the machine.
PROCEDURE Main()
   LOCAL ch := Channel(), oT
   oT := Down( 3000, ch )
   ch:TrySend( 1 )
   ? AWAIT oT
RETURN

ASYNC FUNCTION Down( n, ch )
   IF n == 0
      AWAIT ch:ReceiveAsync()
      RETURN 1 + "x"
   ENDIF
RETURN AWAIT Down( n - 1, ch )
