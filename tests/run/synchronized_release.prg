// SYNCHRONIZED releases its mutex once however the body ends, the mutex's own releaser serving every entry: nested on
// one mutex (released as often as taken), left by RETURN, EXIT, BREAK and an error; another thread finds the mutex free
// after each (TryLock). A value that is no Mutex gets its Hold() sent, its holder disposed of at the end.
PROCEDURE Main()
   LOCAL oM := Mutex(), i, x
   Nested( oM, 3 )
   ? "nested:", Free( oM )
   ? "return:", ByReturn( oM ), Free( oM )
   FOR i := 1 TO 3
      SYNCHRONIZED oM
         IF i == 2
            EXIT
         ENDIF
      END
   NEXT
   ? "exit:", Free( oM )
   BEGIN SEQUENCE
      SYNCHRONIZED oM
         Break( "out" )
      END
   RECOVER USING x
      ? "break:", x, Free( oM )
   END SEQUENCE
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      SYNCHRONIZED oM
         x := 1 + "a"
      END
   RECOVER USING x
      ? "error:", x:subCode, Free( oM )
   END SEQUENCE
   x := TLock():New()
   SYNCHRONIZED x
      ? "own hold:", x:nHeld
   END
   ? "own dispose:", x:nHeld, x:nDisposed
   RETURN

STATIC PROCEDURE Nested( oM, n )
   SYNCHRONIZED oM
      IF n > 1
         Nested( oM, n - 1 )
      ENDIF
   END
   RETURN

STATIC FUNCTION ByReturn( oM )
   SYNCHRONIZED oM
      RETURN "inside"
   END
   RETURN "after"

// Whether another thread can take the mutex now (it gives it back at once).
STATIC FUNCTION Free( oM )
   LOCAL t := ThreadStart( {|| IIf( oM:TryLock(), ( oM:Unlock(), .T. ), .F. ) } )
   RETURN t:Join()

CLASS TLock
   VAR nHeld INIT 0
   VAR nDisposed INIT 0
   METHOD Hold() INLINE ( ::nHeld++, Self )
   METHOD Dispose() INLINE ::nDisposed++
ENDCLASS
