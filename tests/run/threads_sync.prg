// SYNCHRONIZED, Condition and Semaphore: counters that N threads change M times each under SYNCHRONIZED (a matrix of
// thread counts and rounds); the mutex released however the body ends (its end, RETURN, EXIT, LOOP, a BREAK, an error
// a CATCH takes) and taken again inside (nesting); Hold()'s holder; a queue that producers fill and consumers empty
// waiting on a Condition; Notify() going to one waiting thread and NotifyAll() to all; Wait() with a limit, holding the
// mutex as many times as before; a Semaphore that lets two threads in at a time; and the errors of these classes.
PROCEDURE Main()
   LOCAL oM := Mutex(), n, i, o, e, x

   FOR EACH n IN { 1, 2, 4, 8 }
      FOR EACH i IN { 1, 500, 2000 }
         ? "count", n, i, Count( n, i ), n * i
      NEXT
   NEXT

   ? "ends:", Ends( oM, "end" ), Ends( oM, "return" ), Ends( oM, "exit" ), Ends( oM, "loop" ), Ends( oM, "break" ), ;
     Ends( oM, "error" )
   SYNCHRONIZED oM
      SYNCHRONIZED oM
         x := Taken( oM )
      END
      ? "nested:", x, Taken( oM )
   END
   ? "after:", Taken( oM )

   o := oM:Hold()
   ? "holder:", o:ClassName(), Taken( oM ), ArrayText( ObjMethods( o ) ), ArrayText( ObjMethods( oM ) )
   ThreadStart( {|| o:Dispose() } ):Join()
   ? "other thread:", Taken( oM )
   o:Dispose()
   ? "disposed:", Taken( oM )
   o:Dispose()
   ? "twice:", Taken( oM )
   x := {}
   TRY
      USING x[ 3 ] := oM:Hold()
         ? "not reached"
      END
   CATCH e
      ? "assignment fails:", e:subCode, Taken( oM )
   END

   ? "queue:", Queue( 1, 1 ), Queue( 3, 2 ), Queue( 2, 5 )
   ? "notify:", Waiters( .F. ), "all:", Waiters( .T. )

   oM:Lock()
   oM:Lock()
   n := Condition()
   ? "timed:", n:Wait( oM, 30 ), n:Wait( oM, 0 )
   oM:Unlock()
   oM:Unlock()
   ? "depth kept:", Taken( oM )

   ? "semaphore:", Limited( 2, 6 ), Limited( 1, 3 ), Limited( 4, 4 )
   o := Semaphore( 1, 2 )
   ? "count:", o:Count(), o:Wait( 0 ), o:Count(), o:Wait( 0 ), o:Release( 2 ), o:Count(), o:Wait(), o:Release()

   ErrorBlock( {| e | Break( e ) } )
   FOR EACH e IN { {|| n:Wait( oM ) }, {|| n:Wait() }, {|| n:Wait( "m" ) }, {|| WaitHeld( n, oM, "x" ) }, ;
                   {|| o:Release( 2 ) }, {|| o:Release( 0 ) }, {|| o:Release( "1" ) }, {|| o:Wait( "x" ) }, ;
                   {|| Semaphore( -1 ) }, {|| Semaphore( 3, 2 ) }, {|| Semaphore( 0, 0 ) }, {|| Semaphore( "1" ) }, ;
                   {|| Semaphore( 1, 0x80000000 ) } }
      ? "error:", Caught( e )
   NEXT
   ? "unchanged:", o:Count(), Taken( oM )
RETURN

// N threads, each adding 1 M times to an array element under SYNCHRONIZED.
STATIC FUNCTION Count( n, m )
   LOCAL aT := {}, oM := Mutex(), a := { 0 }, k
   FOR k := 1 TO n
      AAdd( aT, ThreadStart( {|| Adder( oM, a, m ) } ) )
   NEXT
   AEval( aT, {| t | t:Join() } )
RETURN a[ 1 ]

STATIC FUNCTION Adder( oM, a, m )
   LOCAL i
   FOR i := 1 TO m
      SYNCHRONIZED oM
         a[ 1 ]++
      END
   NEXT
RETURN NIL

// Whether another thread finds the mutex held.
STATIC FUNCTION Taken( oM )
RETURN ThreadStart( {|| iif( oM:TryLock(), Released( oM ), .T. ) } ):Join()

// Releases the mutex this thread took to look: it was free.
STATIC FUNCTION Released( oM )
   oM:Unlock()
RETURN .F.

// Leaves a SYNCHRONIZED body the way `c` says, then tells whether the mutex is still held.
STATIC FUNCTION Ends( oM, c )
   LOCAL i, e
   IF c == "return"
      Leave( oM )
   ELSEIF c == "exit" .OR. c == "loop"
      FOR i := 1 TO 2
         SYNCHRONIZED oM
            IF c == "exit"
               EXIT
            ENDIF
            LOOP
         END
      NEXT
   ELSE
      TRY
         SYNCHRONIZED oM
            IF c == "break"
               Break( c )
            ELSEIF c == "error"
               i := {} + 1
            ENDIF
         END
      CATCH e
      END
   ENDIF
RETURN c + iif( Taken( oM ), " held", " free" )

STATIC FUNCTION Leave( oM )
   SYNCHRONIZED oM
      RETURN NIL
   END
RETURN NIL

// nProducers threads each put nItems numbers in a queue; as many consumers take them, waiting on a Condition while
// it is empty, until each takes a 0; the sum of what they took.
STATIC FUNCTION Queue( nProducers, nItems )
   LOCAL oM := Mutex(), oC := Condition(), aQ := {}, aT := {}, aC := {}, k, nSum := 0
   FOR k := 1 TO nProducers
      AAdd( aC, ThreadStart( {|| Consumer( oM, oC, aQ ) } ) )
   NEXT
   FOR k := 1 TO nProducers
      AAdd( aT, ThreadStart( {|| Producer( oM, oC, aQ, nItems ) } ) )
   NEXT
   AEval( aT, {| t | t:Join() } )
   FOR k := 1 TO nProducers
      SYNCHRONIZED oM
         AAdd( aQ, 0 )
         oC:Notify()
      END
   NEXT
   AEval( aC, {| t | nSum += t:Join() } )
RETURN nSum

STATIC FUNCTION Producer( oM, oC, aQ, nItems )
   LOCAL i
   FOR i := 1 TO nItems
      SYNCHRONIZED oM
         AAdd( aQ, i )
         oC:Notify()
      END
   NEXT
RETURN NIL

STATIC FUNCTION Consumer( oM, oC, aQ )
   LOCAL n, nSum := 0
   DO WHILE .T.
      SYNCHRONIZED oM
         DO WHILE Empty( aQ )
            oC:Wait( oM )
         ENDDO
         n := aQ[ 1 ]
         ADel( aQ, 1 )
         ASize( aQ, Len( aQ ) - 1 )
      END
      IF n == 0
         EXIT
      ENDIF
      nSum += n
   ENDDO
RETURN nSum

// Three threads wait on a Condition (no limit); once all three wait, Notify() (or NotifyAll()): how many went on
// then, each noting the phase it saw; then NotifyAll() lets the others go too.
STATIC FUNCTION Waiters( lAll )
   LOCAL oM := Mutex(), oC := Condition(), aT := {}, nIn := AtomicInt(), aWoken := {}, aPhase := { 1 }, k, n
   FOR k := 1 TO 3
      AAdd( aT, ThreadStart( {|| Waiter( oM, oC, nIn, aWoken, aPhase ) } ) )
   NEXT
   DO WHILE .T. // a thread counted itself holding the mutex, which only its Wait() releases
      SYNCHRONIZED oM
         IF nIn:Value == 3
            IF lAll
               oC:NotifyAll()
            ELSE
               oC:Notify()
            ENDIF
            EXIT
         ENDIF
      END
   ENDDO
   DO WHILE .T. // until those notified noted it: no other goes on without a signal
      SYNCHRONIZED oM
         IF Len( aWoken ) == iif( lAll, 3, 1 )
            aPhase[ 1 ] := 2
            oC:NotifyAll()
            EXIT
         ENDIF
      END
   ENDDO
   AEval( aT, {| t | t:Join() } )
   n := 0
   AEval( aWoken, {| x | n += iif( x == 1, 1, 0 ) } )
RETURN n

STATIC FUNCTION Waiter( oM, oC, nIn, aWoken, aPhase )
   SYNCHRONIZED oM
      nIn:Add( 1 )
      IF oC:Wait( oM )
         AAdd( aWoken, aPhase[ 1 ] )
      ENDIF
   END
RETURN NIL

// n threads go through a Semaphore of nPlaces, held inside by a gate until the places are full: how many are inside
// then, and a while later (no other gets in), the most inside at once, and the places free in the end.
STATIC FUNCTION Limited( nPlaces, n )
   LOCAL oS := Semaphore( nPlaces ), oGate := Mutex(), nNow := AtomicInt(), nMost := AtomicInt(), aT := {}, k, nFull
   oGate:Lock()
   FOR k := 1 TO n
      AAdd( aT, ThreadStart( {|| Inside( oS, oGate, nNow, nMost ) } ) )
   NEXT
   DO WHILE nNow:Value < nPlaces
      Semaphore( 0 ):Wait( 1 )
   ENDDO
   nFull := nNow:Value
   Semaphore( 0 ):Wait( 30 )
   nFull := LTrim( Str( nFull ) ) + "," + LTrim( Str( nNow:Value ) )
   oGate:Unlock()
   AEval( aT, {| t | t:Join() } )
RETURN nFull + "/" + LTrim( Str( nMost:Value ) ) + "/" + LTrim( Str( oS:Count() ) )

STATIC FUNCTION Inside( oS, oGate, nNow, nMost )
   LOCAL n, nSeen
   oS:Wait()
   n := nNow:Add( 1 )
   DO WHILE ( nSeen := nMost:Value ) < n .AND. !nMost:CompareExchange( nSeen, n )
   ENDDO
   SYNCHRONIZED oGate
   END
   nNow:Add( -1 )
   oS:Release()
RETURN NIL

STATIC FUNCTION WaitHeld( oC, oM, x )
   SYNCHRONIZED oM
      oC:Wait( oM, x )
   END
RETURN NIL

STATIC FUNCTION Caught( b )
   LOCAL e
   BEGIN SEQUENCE
      Eval( b )
   RECOVER USING e
      RETURN e:subSystem + "/" + LTrim( Str( e:genCode ) ) + "/" + LTrim( Str( e:subCode ) ) + " " + e:operation + ;
             " " + e:description
   END SEQUENCE
RETURN "none"

STATIC FUNCTION ArrayText( a )
   LOCAL c := "", x
   FOR EACH x IN a
      c += iif( Empty( c ), "", "," ) + x
   NEXT
RETURN c
