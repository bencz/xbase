// BEGIN SEQUENCE WITH and ALWAYS, compared with the reference: the WITH block is the error block while the body runs
// (also in functions it calls; ErrorBlock() gives it), the program's again in RECOVER and ALWAYS; nested WITHs; a value
// the WITH block returns substitutes the failed result; ALWAYS runs after the body or RECOVER and on the way out of
// an unwinding: without RECOVER a BREAK goes on after ALWAYS, a BREAK in RECOVER goes on after it, and one in ALWAYS
// replaces the request. Beyond the reference (which rejects them): a RETURN, EXIT or LOOP that leaves RECOVER runs
// ALWAYS first.
PROCEDURE Main()
   LOCAL e, b
   ErrorBlock( {| x | QOut( "global block", x:description ), 0 } )
   b := ErrorBlock()
   BEGIN SEQUENCE WITH {| x | QOut( "with block", x:description ), Break( x ) }
      ? "same block?", ErrorBlock() == b, ValType( ErrorBlock() )
      Sub()
      ? "not here"
   RECOVER USING e
      ? "recovered", ValType( e ), e:description
      ? 1 + "x"
   ALWAYS
      ? "always 1"
   END SEQUENCE
   ? "r:", R()
   ? "after R", 1 + "y"
   BEGIN SEQUENCE WITH {| x | QOut( "outer with" ), Break( 1 ) }
      BEGIN SEQUENCE WITH {| x | QOut( "inner with" ), Break( 2 ) }
         ? 1 + "q"
      RECOVER USING e
         ? "inner recover", e
      END SEQUENCE
   RECOVER USING e
      ? "outer recover", e
   END SEQUENCE
   BEGIN SEQUENCE WITH {| x | 42 }
      ? "subst:", 1 + "w"
   END SEQUENCE
   BEGIN SEQUENCE
      ? "body"
   ALWAYS
      ? "always 2"
   END SEQUENCE
   Part2()
   ? "recover exits:", Exits()
   ? "end"
RETURN

STATIC PROCEDURE Part2()
   LOCAL e, i
   BEGIN SEQUENCE
      BEGIN SEQUENCE
         Break( 5 )
      ALWAYS
         ? "always 3"
      END SEQUENCE
      ? "after inner"
   RECOVER USING e
      ? "outer got", e
   END SEQUENCE
   BEGIN SEQUENCE
      BEGIN SEQUENCE
         Break( 6 )
      RECOVER USING e
         ? "recover", e
         Break( 7 )
      ALWAYS
         ? "always 4"
      END SEQUENCE
   RECOVER USING e
      ? "outer got", e
   END SEQUENCE
   BEGIN SEQUENCE
      BEGIN SEQUENCE
         ? "x"
      ALWAYS
         ? "always 5"
         Break( 8 )
      END SEQUENCE
   RECOVER USING e
      ? "outer got", e
   END SEQUENCE
   BEGIN SEQUENCE
      BEGIN SEQUENCE
         Break( 9 )
      ALWAYS
         ? "always 6"
         Break( 10 )
      END SEQUENCE
   RECOVER USING e
      ? "outer got", e
   END SEQUENCE
   FOR i := 1 TO 2
      BEGIN SEQUENCE
         Break( i )
      RECOVER
         ? "loop recover", i
         LOOP
      END SEQUENCE
   NEXT
RETURN

STATIC FUNCTION Exits()
   LOCAL i, c := ""
   FOR i := 1 TO 4
      BEGIN SEQUENCE
         Break( i )
      RECOVER
         IF i == 2
            LOOP
         ELSEIF i == 3
            EXIT
         ENDIF
         c += "r" + LTrim( Str( i ) )
      ALWAYS
         c += "a" + LTrim( Str( i ) )
      END SEQUENCE
   NEXT
   BEGIN SEQUENCE
      Break( 0 )
   RECOVER
      RETURN c + " returned"
   ALWAYS
      ?? " [always before RETURN]"
   END SEQUENCE
RETURN "not here"
STATIC FUNCTION R()
   BEGIN SEQUENCE WITH {| x | Break( x ) }
      ? "in R"
   ALWAYS
      ? "always in R"
   END SEQUENCE
RETURN "no"
STATIC PROCEDURE Sub()
   ? 1 + {}
RETURN
