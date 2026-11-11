// TRY ... CATCH [var] ... FINALLY ... END and Throw(): an error in the body (also in a function it calls) reaches
// CATCH as its Error object, a Throw( x ) or Break( x ) as x; FINALLY runs on every way out: the end of the body or
// of CATCH, an error or BREAK that goes on (no CATCH, or one in CATCH), and a RETURN, EXIT or LOOP that leaves the
// body or CATCH (which BEGIN SEQUENCE's body does not allow). The error block is the program's again in CATCH and
// FINALLY and after the TRY, however it ended. Also nested TRYs, a Throw in FINALLY replacing the request, and an
// error that no CATCH takes going to the error block once the TRY is left.
PROCEDURE Main()
   LOCAL e, i, c
   ErrorBlock( {| x | QOut( "error block:", x:description ), Break( "eb" ) } )
   TRY
      ? "body"
      ? 1 + "a"
      ? "not here"
   CATCH e
      ? "caught:", ValType( e ), e:description, e:operation, ErrorBlock() == NIL
   FINALLY
      ? "finally 1"
   END
   TRY
      Fail()
   CATCH e
      ? "caught from a function:", e:subCode
   END
   TRY
      Throw( "thrown" )
   CATCH e
      ? "caught:", e
   END TRY
   TRY
      ? "no error"
   CATCH
      ? "not here"
   FINALLY
      ? "finally 2"
   END
   ? "returned:", Ret( 1 ), Ret( 2 )
   c := ""
   FOR i := 1 TO 5
      TRY
         IF i == 2
            LOOP
         ELSEIF i == 4
            EXIT
         ENDIF
         c += LTrim( Str( i ) )
      FINALLY
         c += "f"
      END
   NEXT
   ? "loop:", c, i
   BEGIN SEQUENCE
      TRY
         Throw( 1 )
      FINALLY
         ? "finally without catch"
      END
   RECOVER USING e
      ? "went on:", e
   END SEQUENCE
   BEGIN SEQUENCE
      TRY
         Throw( 2 )
      CATCH e
         ? "catch then error"
         ? 1 + {}
      FINALLY
         ? "finally after catch error"
      END
   RECOVER USING e
      ? "went on:", e
   END SEQUENCE
   BEGIN SEQUENCE
      TRY
         Throw( 3 )
      FINALLY
         Throw( 4 )
      END
   RECOVER USING e
      ? "replaced:", e
   END SEQUENCE
   TRY
      TRY
         ? 1 / {}
      CATCH e
         ? "inner:", e:operation
         Throw( "rethrown" )
      END
   CATCH e
      ? "outer:", e
   END
   BEGIN SEQUENCE
      ? 1 + "b"
   RECOVER USING e
      ? "after:", e
   END SEQUENCE
RETURN

STATIC PROCEDURE Fail()
   LOCAL a := {}
   ? a[ 5 ]
RETURN

STATIC FUNCTION Ret( n )
   TRY
      IF n == 1
         RETURN "from body"
      ENDIF
      Throw( n )
   CATCH
      RETURN "from catch"
   FINALLY
      ?? " [finally in Ret " + LTrim( Str( n ) ) + "]"
   END
RETURN "not here"
