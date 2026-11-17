// One channel enumerator awaited by two consumers at once, on the pool's workers, 200 times: one of them goes in line
// for a value, the other's MoveNextAsync() raises BASE/9035 (also when both made their waiting tasks at the same time:
// the second to take the channel's lock finds the first in line), so its task fails; the values sent then all go to
// the one in line, none lost, none given twice.
PROCEDURE Main()
   LOCAL k, ch, e, a, nRight := 0, cOdd := "", t, nFailed, cGot, nStart
   FOR k := 1 TO 200
      ch := Channel()
      e := ch:GetEnumerator()
      a := { TaskRun( {|| Consume( e ):Result } ), TaskRun( {|| Consume( e ):Result } ) }
      nStart := ElapsedMs()
      DO WHILE !a[ 1 ]:IsCompleted .AND. !a[ 2 ]:IsCompleted .AND. ElapsedMs() - nStart < 10000
         AWAIT TaskYield()
      ENDDO
      ch:TrySend( 1 )
      ch:TrySend( 2 )
      ch:TrySend( 3 )
      ch:Close()
      TaskWhenAll( a ):Wait( 10000 )
      nFailed := 0
      cGot := ""
      FOR EACH t IN a
         IF t:IsFailed
            nFailed++
            IF t:Error:subCode != 9035
               cGot += "?" + LTrim( Str( t:Error:subCode ) )
            ENDIF
         ELSE
            cGot += t:Result
         ENDIF
      NEXT
      IF nFailed == 1 .AND. cGot == "123"
         nRight++
      ELSE
         cOdd += " " + LTrim( Str( nFailed ) ) + ":" + cGot
      ENDIF
   NEXT
   ? "rounds right:", nRight, "others:" + cOdd
RETURN

ASYNC FUNCTION Consume( e )
   LOCAL x, c := ""
   AWAIT FOR EACH x IN e
      c += LTrim( Str( x ) )
   NEXT
RETURN c
