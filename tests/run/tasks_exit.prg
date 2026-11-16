// At the program's end what is still timed ends too: an EXIT procedure that waits for a delay, a timeout or a token
// with a CancelAfter pending from the start procedure is not held: the delay and the timeout are canceled, the source
// canceled (its Register task, scheduled then, is canceled with the tasks that never started). Tasks started by the
// EXIT procedure run, with timers of their own.
STATIC s_aPending

PROCEDURE Main()
   LOCAL s := CancellationTokenSource()
   s:CancelAfter( 3600000 )
   s_aPending := { TaskDelay( 3600000 ), TaskTimeout( TaskCompletionSource():Task, 3600000 ), s, ;
                   s:Token:Register( {|| "callback" } ) }
   ? "main ends"
RETURN

EXIT PROCEDURE Bye()
   ? "delay:", s_aPending[ 1 ]:Wait(), s_aPending[ 1 ]:IsCanceled
   ? "timeout:", s_aPending[ 2 ]:Wait(), s_aPending[ 2 ]:IsCanceled
   ? "source:", s_aPending[ 3 ]:IsCancellationRequested, s_aPending[ 4 ]:Wait(), s_aPending[ 4 ]:IsCanceled
   ? "new timers:", TaskDelay( 20 ):Wait(), TaskTimeout( TaskDelay( 3600000 ), 20 ):Wait()
RETURN
