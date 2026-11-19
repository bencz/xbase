// A thread that starts waiting on a Condition right after a NotifyAll() takes no signal given to the threads that
// waited before it: two threads that hand a state back and forth with NotifyAll() never both wait unwoken. A Notify()
// given without the mutex wakes a waiter that may take it, whoever else waits.
PROCEDURE Main()
   LOCAL a, oM, oC, oT, n, nDone := 0
   FOR n := 1 TO 300
      a := { 0 }
      oM := Mutex()
      oC := Condition()
      oT := ThreadStart( {|| Step( a, oM, oC, 1, 2, -1 ) } )
      Step( a, oM, oC, 0, 1, 2 )
      oT:Join()
      IF a[ 1 ] == 2
         nDone++
      ENDIF
   NEXT
   ? "done", nDone
   nDone := 0
   FOR n := 1 TO 100
      nDone += Tokens()
   NEXT
   ? "tokens", nDone
   RETURN

// Four waiters take one token each; each token is given under the mutex and notified without it.
STATIC FUNCTION Tokens()
   LOCAL a := { 0, 0 }, oM := Mutex(), oC := Condition(), aT := {}, k
   FOR k := 1 TO 4
      AAdd( aT, ThreadStart( {|| Take( a, oM, oC ) } ) )
   NEXT
   FOR k := 1 TO 4
      oM:Lock()
      a[ 1 ]++
      oM:Unlock()
      oC:Notify()
   NEXT
   AEval( aT, {| t | t:Join() } )
   RETURN a[ 2 ]

STATIC PROCEDURE Take( a, oM, oC )
   oM:Lock()
   DO WHILE a[ 1 ] == 0
      oC:Wait( oM )
   ENDDO
   a[ 1 ]--
   a[ 2 ]++
   oM:Unlock()
   RETURN

STATIC PROCEDURE Step( a, oM, oC, nFrom, nTo, nUntil )
   oM:Lock()
   DO WHILE a[ 1 ] != nFrom
      oC:Wait( oM )
   ENDDO
   a[ 1 ] := nTo
   oC:NotifyAll()
   DO WHILE nUntil != -1 .AND. a[ 1 ] != nUntil
      oC:Wait( oM )
   ENDDO
   oM:Unlock()
   RETURN
