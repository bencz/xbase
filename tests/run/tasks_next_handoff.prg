// A task made ready for the next turn of a worker whose running task then keeps it busy still runs: one TaskRun()
// starts inside a task that then waits for it by spinning, and one continuation readied by a completion (a channel's
// send) whose completer then spins for it. Each must run while the other spins (within 3 seconds), with one worker
// (//WORKERS:1, the .args): the monitor hands the place of the spinning task over, woken by the submission though it
// went idle in the pause before (nothing queued). The program's thread spins too, so that it runs none of them itself.
PROCEDURE Main()
   LOCAL flag := AtomicInt( 0 ), done := AtomicInt( 0 ), ch := Channel( 1 ), w, t
   Pause()
   t := TaskRun( {|| Outer( flag, done ) } )
   Spin( done )
   ? "task run inside a task:", t:Result
   flag := AtomicInt( 0 )
   done := AtomicInt( 0 )
   w := Waiter( ch, flag )
   Pause()
   t := TaskRun( {|| Feeder( ch, flag, done ) } )
   Spin( done )
   ? "continuation of a completion:", t:Result
   AWAIT w
   RETURN

STATIC PROCEDURE Pause()
   LOCAL t0 := ElapsedMs()
   DO WHILE ElapsedMs() - t0 < 300
   ENDDO
   RETURN

STATIC PROCEDURE Spin( done )
   LOCAL t0 := ElapsedMs()
   DO WHILE done:Value == 0 .AND. ElapsedMs() - t0 < 5000
   ENDDO
   RETURN

STATIC FUNCTION Outer( flag, done )
   LOCAL inner, t0 := ElapsedMs()
   inner := TaskRun( {|| flag:Value := 1 } )
   DO WHILE flag:Value == 0 .AND. ElapsedMs() - t0 < 3000
   ENDDO
   done:Value := 1
   RETURN IIf( flag:Value == 1, "ran", "stranded" )

ASYNC FUNCTION Waiter( ch, flag )
   flag:Value := AWAIT ch:ReceiveAsync()
   RETURN NIL

STATIC FUNCTION Feeder( ch, flag, done )
   LOCAL t0 := ElapsedMs()
   ch:TrySend( 7 )
   DO WHILE flag:Value == 0 .AND. ElapsedMs() - t0 < 3000
   ENDDO
   done:Value := 1
   RETURN IIf( flag:Value == 7, "ran", "stranded" )
