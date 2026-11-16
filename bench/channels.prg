// Channels: 4 producer tasks send 250,000 numbers each through a bounded channel (capacity 64) to 2 consumer tasks
// (SendAsync and ReceiveAsync, waiting when it is full or empty), then 1,000,000 TrySend/TryReceive pairs on an
// unbounded one from one thread: what handing a value from task to task costs.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), ch := Channel( 64 ), aP := {}, aC := {}, k, s := 0, u := Channel(), aSlot := { NIL }
   FOR k := 1 TO 2
      AAdd( aC, TaskRun( {|| Consume( ch ) } ) )
   NEXT
   FOR k := 1 TO 4
      AAdd( aP, TaskRun( {|| Produce( ch, 250000 ) } ) )
   NEXT
   AEval( aP, {| x | x:Wait() } )
   ch:Close()
   AEval( aC, {| x | s += x:Result } )
   FOR k := 1 TO 1000000
      u:TrySend( k )
      u:TryReceive( aSlot )
      s += aSlot[ 1 ] % 2
   NEXT
   ? "channels", ElapsedMs() - t, IIf( s == 4 * 31250125000 + 500000, "ok", "WRONG" )
   RETURN

STATIC FUNCTION Produce( ch, n )
   LOCAL i
   FOR i := 1 TO n
      ch:SendAsync( i ):Wait()
   NEXT
RETURN NIL

STATIC FUNCTION Consume( ch )
   LOCAL x, s := 0
   FOR EACH x IN ch
      s += x
   NEXT
RETURN s
