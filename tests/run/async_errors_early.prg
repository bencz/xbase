// An ASYNC function whose body fails before its first AWAIT: its task is failed when the call returns (the first step
// runs on the caller's thread), and AWAIT of that failed task raises the error again where it is awaited, as for one
// that failed after suspending. Handled, the error object is the one raised, with its place; not handled, the default
// handler reports where it happened (Called from) and each place it was waited for (Waited for in): Inner fails at
// once, Middle awaits it at once, Main awaits Middle; the calls of the first step are the task's own, not its caller's.
// A task's call stack (ProcName()) is its own calls, whether its step runs inline on its caller's or waiter's thread
// or on a worker.
PROCEDURE Main()
   LOCAL t, e, ch := Channel()
   ? "own calls:", Levels(), "|", TaskRun( {|| Levels() } ):Result, "|", AWAIT Deep( NIL ), "|", ;
     Later( ch, TaskRun( {|| Levels() } ) ), "|", Later( ch, Deep( ch ) )
   t := Inner( 1 )
   ? "failed at once:", t:IsCompleted, t:IsFailed, t:Error:subCode, t:Error:operation
   TRY
      AWAIT Inner( 2 )
   CATCH e
      ? "caught:", e:subCode, e:operation, e:description
   END
   ? "middle:", AWAIT Middle( 3 )
   ? "not reached"
RETURN

ASYNC FUNCTION Middle( n )
   LOCAL x := AWAIT Inner( n )
RETURN x

ASYNC FUNCTION Inner( n )
RETURN Helper( n )

// The names of the calls below this function, outermost last.
STATIC FUNCTION Levels()
   LOCAL c := "", n := 1
   DO WHILE !Empty( ProcName( n ) )
      c += ProcName( n ) + " "
      n++
   ENDDO
RETURN c

// Levels() in its first step (no channel), or after it waited for the channel.
ASYNC FUNCTION Deep( ch )
   IF ch != NIL
      AWAIT ch:ReceiveAsync()
   ENDIF
RETURN Levels()

// The result of a task, once the channel lets it go on (a send for a waiting step, none needed otherwise).
STATIC FUNCTION Later( ch, t )
   ch:TrySend( 1 )
RETURN t:Result

STATIC FUNCTION Helper( n )
RETURN n + "text"
