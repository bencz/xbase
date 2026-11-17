// Semaphore:Release( n ) with threads blocked in Wait(): exactly n of them take a place (one woken, each that takes a
// place with more left wakes the next), the others go on waiting until the next release; Release past the maximum is
// the error that changes nothing. Then threads blocked for good (a Wait() and a Lock() with no limit) end when the
// program ends: the end wakes them at once, it does not wait for them to look.
PROCEDURE Main()
   LOCAL oS := Semaphore( 0, 10 ), aT := {}, oIn := AtomicInt(), k, nStart, e, oM := Mutex()
   FOR k := 1 TO 6
      AAdd( aT, ThreadStart( {|| oS:Wait(), oIn:Add( 1 ) } ) )
   NEXT
   Settle()
   ? "none before:", oIn:Value
   oS:Release( 4 )
   ? "four of six:", Reach( oIn, 4 ), Hold( oIn, 4 ), oS:Count()
   oS:Release( 2 )
   ? "the other two:", Reach( oIn, 6 ), oS:Count()
   AEval( aT, {| t | t:Join() } )
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      oS:Release( 11 )
   RECOVER USING e
      ? "past the maximum:", e:description, e:operation, oS:Count()
   END SEQUENCE

   oS := Semaphore( 0 )
   oM:Lock()
   ThreadStart( {|| oS:Wait() } )
   ThreadStart( {|| oM:Lock() } )
   Settle()
   nStart := ElapsedMs()
   ? "blocked threads end with the program"
RETURN

// Long enough for the threads to block.
STATIC PROCEDURE Settle()
   LOCAL n := ElapsedMs()
   DO WHILE ElapsedMs() - n < 200
   ENDDO
RETURN

// Waits up to 10 s for the counter to reach n.
STATIC FUNCTION Reach( oIn, n )
   LOCAL nStart := ElapsedMs()
   DO WHILE oIn:Value < n .AND. ElapsedMs() - nStart < 10000
   ENDDO
RETURN oIn:Value == n

// No more than n after a while.
STATIC FUNCTION Hold( oIn, n )
   Settle()
RETURN oIn:Value == n
