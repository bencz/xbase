// ASYNC functions: 1,000,000 calls that complete at once, awaited (the task made, the first step run, no suspension);
// 100,000 round trips between two ASYNC functions over channels of 1 (four suspensions and resumptions each: what a
// step that waits costs); 10,000 handlers suspended at once on delays and resumed (many waiting, as a server's).
PROCEDURE Main()
   LOCAL t := ElapsedMs(), k, s := 0
   FOR k := 1 TO 1000000
      s += AWAIT Sync( k )
   NEXT
   s += AWAIT PingPong( 100000 )
   s += AWAIT Handlers( 10000 )
   ? "async", ElapsedMs() - t, IIf( s == 500000500000 + 5000050000 + 50005000, "ok", "WRONG" )
RETURN

ASYNC FUNCTION Sync( k )
RETURN k

ASYNC FUNCTION PingPong( n )
   LOCAL a := Channel( 1 ), b := Channel( 1 ), k, s := 0, t
   t := Echo( a, b, n )
   FOR k := 1 TO n
      AWAIT a:SendAsync( k )
      s += AWAIT b:ReceiveAsync()
   NEXT
   AWAIT t
RETURN s

ASYNC FUNCTION Echo( a, b, n )
   LOCAL k
   FOR k := 1 TO n
      AWAIT b:SendAsync( AWAIT a:ReceiveAsync() )
   NEXT
RETURN NIL

ASYNC FUNCTION Handlers( n )
   LOCAL a := {}, k, s := 0, x
   FOR k := 1 TO n
      AAdd( a, Handler( k ) )
   NEXT
   FOR EACH x IN AWAIT TaskWhenAll( a )
      s += x
   NEXT
RETURN s

ASYNC FUNCTION Handler( k )
   AWAIT TaskDelay( k % 10 )
   AWAIT TaskRun( {|| NIL } )
RETURN k
