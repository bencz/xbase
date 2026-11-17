// A sender's token canceled while the channel holds the sender's own claim (it looks for room once more before it
// waits in line) still cancels the send: another thread cancels each token as the send is made, again and again, on a
// full channel; every send ends (canceled), none waits forever, and no value of a canceled send is received.
PROCEDURE Main()
   LOCAL ch := Channel( 1 ), cts, t, i, nLost := 0, nCanceled := 0, aCur := { NIL, .F. }, tc, a := { NIL }
   LOCAL nRounds := 50000
   ch:TrySend( 0 )
   tc := TaskRun( {|| Canceller( aCur ) } )
   FOR i := 1 TO nRounds
      cts := CancellationTokenSource()
      aCur[ 1 ] := cts
      t := ch:SendAsync( i, cts:Token )
      cts:Cancel()
      IF ! t:Wait( 2000 )
         IF ++nLost == 3 // enough to tell
            EXIT
         ENDIF
      ELSEIF t:IsCanceled
         nCanceled++
      ENDIF
   NEXT
   aCur[ 2 ] := .T.
   tc:Wait()
   ? "sends whose canceled token never ended them:", nLost
   ? "sends canceled:", nCanceled == nRounds
   ch:TryReceive( a )
   ? "the value kept:", a[ 1 ], "then:", ch:TryReceive( a )
RETURN

STATIC PROCEDURE Canceller( aCur )
   LOCAL c
   DO WHILE ! aCur[ 2 ]
      c := aCur[ 1 ]
      IF c != NIL
         c:Cancel()
      ENDIF
   ENDDO
RETURN
