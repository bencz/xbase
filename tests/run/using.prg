// USING expr ... END: the value is disposed (its Dispose()) however the body ends: at its end, by LOOP, EXIT and
// RETURN, by a BREAK and by an error that goes on to a RECOVER or a CATCH around it, and by QUIT (the last case); a
// NIL value is not; nested statements dispose innermost first; `USING v := expr` assigns v too, and the value disposed
// is the one evaluated, whatever v holds later, and is disposed when assigning v fails; a value without Dispose() is
// an error at the end; a Dispose() that breaks goes on as any BREAK; a USING in a generator around no YIELD; USING as
// the name of a variable and of a function still works.
STATIC s_aLog := {}

PROCEDURE Main()
   LOCAL o, i, e, x, using := { 10 }
   USING o := Res():New( "plain" )
      Mark( "body " + o:cName )
      o := "changed"
   END USING
   ? "plain:", Done(), ValType( o )

   FOR i := 1 TO 4
      USING Res():New( "pass" + LTrim( Str( i ) ) )
         IF i == 2
            LOOP
         ELSEIF i == 3
            EXIT
         ENDIF
         Mark( "end of body" )
      END
   NEXT
   ? "loop:", Done()

   ? "return:", Returns( .T. ), Returns( .F. ), Done()

   BEGIN SEQUENCE
      USING Res():New( "break" )
         Break( "value" )
      END
   RECOVER USING x
      Mark( "recover " + x )
   END SEQUENCE
   ? "break:", Done()

   TRY
      USING Res():New( "error" )
         x := 1 + "a"
      END
   CATCH e
      Mark( "catch " + e:operation )
   END
   ? "error:", Done()

   ErrorBlock( {| e | Break( e ) } )
   BEGIN SEQUENCE
      USING Res():New( "outer" )
         USING Res():New( "inner" )
            x := Len( 5 )
         END
      END
   RECOVER USING e
      Mark( "recover " + e:operation )
   END SEQUENCE
   ? "nested:", Done()

   USING NIL
      Mark( "nil body" )
   END
   ? "nil:", Done()

   x := {}
   TRY
      USING x[ 2 ] := Res():New( "assigned" )
         Mark( "not reached" )
      END
   CATCH e
      Mark( "catch " + LTrim( Str( e:subCode ) ) )
   END
   ? "assignment fails:", Done()

   BEGIN SEQUENCE
      USING "no dispose"
         Mark( "body" )
      END
   RECOVER USING e
      Mark( LTrim( Str( e:genCode ) ) + " " + LTrim( Str( e:subCode ) ) + " " + e:operation )
   END SEQUENCE
   ? "no Dispose:", Done()

   BEGIN SEQUENCE
      USING Res():New( "breaks", .T. )
         Mark( "body" )
      END
      Mark( "not reached" )
   RECOVER USING x
      Mark( "recover " + x )
   END SEQUENCE
   ? "Dispose breaks:", Done()

   FOR EACH x IN Gen()
      Mark( "got " + LTrim( Str( x ) ) )
   NEXT
   ? "generator:", Done()

   using[ 1 ]++
   ? "names:", using[ 1 ], Using( 1 )

   USING Res():New( "quit" )
      QUIT
   END
RETURN

STATIC FUNCTION Returns( lEarly )
   USING Res():New( iif( lEarly, "early", "late" ) )
      IF lEarly
         RETURN "early"
      ENDIF
   END
RETURN "late"

STATIC FUNCTION Gen()
   LOCAL i
   USING Res():New( "generator" )
      Mark( "start" )
   END
   FOR i := 1 TO 2
      YIELD i
   NEXT
RETURN

STATIC FUNCTION Using( n )
RETURN n + 1

STATIC PROCEDURE Mark( c )
   AAdd( s_aLog, c )
RETURN

// The log so far, as one line, emptied.
STATIC FUNCTION Done()
   LOCAL c := "", x
   FOR EACH x IN s_aLog
      c += iif( Empty( c ), "", " | " ) + x
   NEXT
   ASize( s_aLog, 0 )
RETURN c

CLASS Res
   VAR cName
   VAR lBreaks INIT .F.
   METHOD New( c, l ) INLINE ::cName := c, ::lBreaks := ( l == .T. ), Self
   METHOD Dispose()
ENDCLASS

METHOD Dispose() CLASS Res
   IF ::cName == "quit"
      ? "disposed: quit"
   ENDIF
   Mark( "dispose " + ::cName )
   IF ::lBreaks
      Break( "from Dispose" )
   ENDIF
RETURN NIL
