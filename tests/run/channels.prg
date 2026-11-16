// Channels: TrySend/TryReceive and Count on an unbounded channel (the slot array); a receiver that waits until a value
// comes; a bounded one (senders wait while it is full, and go on as receivers make room, in order); producers and
// consumers on threads and tasks (a matrix of counts: every value received once, the sum checked); FOR EACH over a
// channel until it is closed and empty; Close (waiting receivers and senders fail with "Channel closed", the values
// kept are still received, closing again does nothing); a token that cancels a waiting receive or send (the value of
// a canceled send is not kept); argument errors.
PROCEDURE Main()
   LOCAL ch, a := { NIL }, t, t2, s, x, n, m, e
   ch := Channel()
   ? "try:", ch:ClassName(), ch:TrySend( 1 ), ch:TrySend( "two" ), ch:Count(), ch:TryReceive( a ), a[ 1 ], ;
     ch:TryReceive(), ch:Count(), ch:TryReceive( a ), ch:IsClosed
   t := ch:ReceiveAsync()
   ? "waits:", t:IsCompleted, ch:SendAsync( "hi" ):Result, t:Result, ch:Count()

   ch := Channel( 2 )
   ? "bounded:", ch:SendAsync( 1 ):IsCompleted, ch:SendAsync( 2 ):IsCompleted, ch:TrySend( 9 )
   t := ch:SendAsync( 3 )
   t2 := ch:SendAsync( 4 )
   ? "full:", t:IsCompleted, t2:IsCompleted, ch:Count()
   ? "room:", ch:ReceiveAsync():Result, t:IsCompleted, t2:IsCompleted, ch:ReceiveAsync():Result, t2:IsCompleted, ;
     ch:ReceiveAsync():Result, ch:ReceiveAsync():Result, ch:Count()

   FOR EACH n IN { 1, 3 }
      FOR EACH m IN { 1, 5, 2000 }
         ? "flow", n, m, Flow( n, m, 0 ), Flow( n, m, 4 ), Int( n * m * ( m + 1 ) / 2 )
      NEXT
   NEXT

   ch := Channel()
   ch:TrySend( "kept" )
   ch:Close()
   ch:Close()
   ? "closed:", ch:IsClosed, ch:ReceiveAsync():Result, Failure( ch:ReceiveAsync() ), Failure( ch:SendAsync( 1 ) ), ;
     ch:TrySend( 1 )
   ch := Channel( 1 )
   ch:TrySend( 1 )
   t := ch:SendAsync( 2 )
   ch:Close()
   ? "close fails a sender:", Failure( t ), ch:ReceiveAsync():Result, Failure( ch:ReceiveAsync() )
   ch := Channel()
   t := ch:ReceiveAsync()
   TaskRun( {|| ch:Close() } ):Wait()
   ? "close fails a receiver:", Failure( t )

   ch := Channel()
   s := CancellationTokenSource()
   t := ch:ReceiveAsync( s:Token )
   s:Cancel()
   t:Wait()
   ? "canceled receive:", t:IsCanceled, ch:TrySend( "after" ), ch:ReceiveAsync():Result
   ch := Channel( 1 )
   ch:TrySend( 1 )
   s := CancellationTokenSource()
   t := ch:SendAsync( 2, s:Token )
   s:Cancel()
   t:Wait()
   ? "canceled send:", t:IsCanceled, ch:ReceiveAsync():Result, ch:Count(), ch:TryReceive()

   ErrorBlock( {| e | Break( e ) } )
   FOR EACH e IN { {|| Channel( -1 ) }, {|| Channel( "x" ) }, {|| Channel():ReceiveAsync( 5 ) }, ;
                   {|| Channel():SendAsync( 1, "x" ) } }
      ? "error:", Caught( e )
   NEXT
RETURN

// n producers (threads, or tasks when nCapacity is not 0) each send 1 .. m through a channel of nCapacity (0: any),
// and a FOR EACH takes them until the channel is closed: their sum.
STATIC FUNCTION Flow( n, m, nCapacity )
   LOCAL ch := Channel( nCapacity ), a := {}, k, x, nSum := 0
   FOR k := 1 TO n
      AAdd( a, iif( nCapacity == 0, ThreadStart( {|| Produce( ch, m ) } ), TaskRun( {|| Produce( ch, m ) } ) ) )
   NEXT
   ThreadStart( {|| AEval( a, {| t | iif( nCapacity == 0, t:Join(), t:Wait() ) } ), ch:Close() } )
   FOR EACH x IN ch
      nSum += x
   NEXT
RETURN nSum

STATIC FUNCTION Produce( ch, m )
   LOCAL i
   FOR i := 1 TO m
      ch:SendAsync( i ):Wait()
   NEXT
RETURN NIL

STATIC FUNCTION Failure( t )
   t:Wait()
RETURN iif( t:IsFailed, t:Error:description + " " + t:Error:operation, "no failure" )

STATIC FUNCTION Caught( b )
   LOCAL e
   BEGIN SEQUENCE
      Eval( b )
   RECOVER USING e
      RETURN e:subSystem + "/" + LTrim( Str( e:genCode ) ) + "/" + LTrim( Str( e:subCode ) ) + " " + e:operation
   END SEQUENCE
RETURN "none"
