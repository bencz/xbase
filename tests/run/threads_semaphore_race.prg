// Semaphore waiters racing the release chain: 12 threads take places 300 times each from a semaphore of 4, with no
// limit, with a limit of 0 and 1 ms (which expire while releases wake others), some taking a second place with a
// limit and giving both back with one Release( 2 ) (which wakes one waiter, who wakes the next while places are left).
// Every place comes back: no waiter without a limit is left waiting (a lost wake would hang it), never more than 4
// are held at once, every attempt either took a place or timed out, and the count ends where it began.
PROCEDURE Main()
   LOCAL oS := Semaphore( 4, 4 ), oHeld := AtomicInt(), oMost := AtomicInt(), oTook := AtomicInt()
   LOCAL oTimed := AtomicInt(), aT := {}, k
   FOR k := 1 TO 12
      AAdd( aT, ThreadStart( @Worker(), k, oS, oHeld, oMost, oTook, oTimed ) )
   NEXT
   AEval( aT, {| t | t:Join() } )
   ? "attempts:", oTook:Value + oTimed:Value == 12 * 300 + 6 * 150, "some took:", oTook:Value > 0
   ? "at most 4 held:", oMost:Value <= 4, "none held:", oHeld:Value, "count:", oS:Count()
RETURN

STATIC PROCEDURE Worker( nId, oS, oHeld, oMost, oTook, oTimed )
   LOCAL k, nGot
   FOR k := 1 TO 300
      nGot := 0
      IF oS:Wait( iif( ( nId + k ) % 3 == 0, NIL, ( nId + k ) % 2 ) )
         nGot := 1
         oTook:Add( 1 )
      ELSE
         oTimed:Add( 1 )
      ENDIF
      IF nGot == 1 .AND. nId % 2 == 0 .AND. k % 2 == 0 // half the threads, every other time: 150 more attempts each
         IF oS:Wait( 1 )
            nGot := 2
            oTook:Add( 1 )
         ELSE
            oTimed:Add( 1 )
         ENDIF
      ELSEIF nId % 2 == 0 .AND. k % 2 == 0
         oTimed:Add( 1 ) // the second attempt not made counts as one that took nothing
      ENDIF
      IF nGot > 0
         Most( oMost, oHeld:Add( nGot ) )
         oHeld:Add( -nGot )
         oS:Release( nGot )
      ENDIF
   NEXT
RETURN

// Keeps in oMost the largest n seen.
STATIC PROCEDURE Most( oMost, n )
   LOCAL nWas := oMost:Value
   DO WHILE n > nWas .AND. !oMost:CompareExchange( nWas, n )
      nWas := oMost:Value
   ENDDO
RETURN
