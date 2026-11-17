// Waiters a channel serves stay alive while it settles them, with collections running meanwhile (another thread
// allocates without pause): senders that nobody keeps (SendAsync on a full channel, its task dropped) whose values
// receivers take, and receivers that nobody keeps but their continuation (ReceiveAsync():ContinueWith). Every value
// arrives once, in order, and every continuation runs with its value.
PROCEDURE Main()
   LOCAL ch := Channel( 1 ), a := { NIL }, i, nOrder := 0, aStop := { .F. }, tGarbage, nSum := 0
   LOCAL nRounds := 100000, oCount := AtomicInt(), oSum := AtomicInt()
   tGarbage := TaskRun( {|| Garbage( aStop ) } )
   ch:TrySend( { 0, Str( 0 ) } )
   FOR i := 1 TO nRounds
      ch:SendAsync( { i, Str( i ) } ) // waits; its task is kept by the channel alone
      ch:TryReceive( a )
      IF a[ 1 ][ 1 ] == i - 1
         nOrder++
      ENDIF
   NEXT
   ch:TryReceive( a )
   ? "senders served in order:", nOrder == nRounds, a[ 1 ][ 1 ]
   FOR i := 1 TO nRounds
      ch:ReceiveAsync():ContinueWith( {| t | oSum:Add( t:Result[ 1 ] ), oCount:Add( 1 ) } ) /* counted once summed */
      ch:TrySend( { i, Str( i ) } )
      nSum += i
   NEXT
   FOR i := 1 TO 500
      IF oCount:Value == nRounds
         EXIT
      ENDIF
      TaskDelay( 10 ):Wait()
   NEXT
   ? "receivers' continuations:", oCount:Value == nRounds, oSum:Value == nSum
   aStop[ 1 ] := .T.
   tGarbage:Wait()
RETURN

STATIC PROCEDURE Garbage( aStop )
   LOCAL x
   DO WHILE ! aStop[ 1 ]
      x := Array( 64 )
      x := Space( 20000 ) // collections often
   ENDDO
RETURN
