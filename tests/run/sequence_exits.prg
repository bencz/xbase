// Ways out of sequences: a LOOP inside a SWITCH inside a TRY (and in a RECOVER with ALWAYS) runs FINALLY or ALWAYS and
// puts the error block back; a RETURN from a FOR EACH inside a TRY gives the loop variable its value back before
// FINALLY runs, also when FINALLY then raises; and a QUIT set aside while ALWAYS runs stays when the ALWAYS code
// raises a BREAK that an outer sequence would take (as in the reference, the program ends). Also a plain LOCAL loop
// variable restored before FINALLY or ALWAYS runs on a RETURN, as on an error.
PROCEDURE Main()
   LOCAL n := 0, b := ErrorBlock(), x, v := "orig", e
   DO WHILE n < 2
      n++
      TRY
         SWITCH n
         CASE 1
            LOOP
         ENDSWITCH
      CATCH
      FINALLY
         ? "finally", n
      END
   ENDDO
   ? "error block restored:", ErrorBlock() == b
   FOR n := 1 TO 2
      BEGIN SEQUENCE
         Break( n )
      RECOVER USING x
         SWITCH x
         CASE 1
            LOOP
         ENDSWITCH
      ALWAYS
         ? "always", n
      END SEQUENCE
   NEXT
   P( @v, .F. )
   ? "restored:", v
   TRY
      P( @v, .T. )
   CATCH e
      ? "caught:", e
   END
   ? "restored after a throw:", v
   x := { 1, 2 }
   ? "plain variable:", Q( x ), x[ 1 ], R( x ), x[ 1 ]
   BEGIN SEQUENCE
      BEGIN SEQUENCE
         QUIT
      ALWAYS
         ? "always while quitting"
         BREAK "during quit"
      END
   RECOVER USING x
      ? "not here: recovered", x
   END
   ? "not here: after"
RETURN

STATIC FUNCTION P( x, lThrow )
   TRY
      FOR EACH x IN { 1, 2 }
         RETURN NIL
      NEXT
   FINALLY
      ? "finally sees", x
      IF lThrow
         Throw( "f" )
      ENDIF
   END
RETURN NIL

STATIC FUNCTION Q( a )
   LOCAL y := "orig"
   TRY
      FOR EACH y IN a
         RETURN NIL
      NEXT
   FINALLY
      ?? " finally sees", y
      y := "changed"
   END
RETURN NIL

STATIC FUNCTION R( a )
   LOCAL y := "orig"
   BEGIN SEQUENCE
      Break( 0 )
   RECOVER
      FOR EACH y IN a
         RETURN NIL
      NEXT
   ALWAYS
      ?? " always sees", y
      y := "changed"
   END SEQUENCE
RETURN NIL
