// BEGIN SEQUENCE, BREAK and RECOVER: a BREAK in the body or in what it calls,
// nested sequences, a BREAK from RECOVER, a BREAK without value, and the
// variables as the body left them.
STATIC s_aList := { 1, 2, 3 }
STATIC s_x := "before"

PROCEDURE Main()
   LOCAL x := 0, y := "start", n, i, a := { 10, 20, 30 }, e, bOld
   BEGIN SEQUENCE
      ? "in"
      BREAK 5
      ? "not here"
   RECOVER USING x
      ? "recovered", x
   END SEQUENCE
   ? "after", x

   BEGIN SEQUENCE
      Inner()
   RECOVER USING x
      ? "outer got", x
   END

   BEGIN SEQUENCE
      ? "no break"
   RECOVER
      ? "not here"
   END
   BEGIN SEQUENCE
      BREAK
   RECOVER USING x
      ? "no value:", ValType( x )
   ENDSEQUENCE
   BEGIN SEQUENCE
      BREAK "no recover"
   END
   ? "without RECOVER"

   // Variables keep what the body stored before the BREAK.
   n := 1
   BEGIN SEQUENCE
      n := 2
      y := "changed"
      Thrower( 3 )
      n := 99
   RECOVER USING x
      ? "n", n, y, x
   END

   // A BREAK inside loops inside the body; loops and SWITCH inside the body.
   BEGIN SEQUENCE
      FOR i := 1 TO 10
         DO WHILE .T.
            IF i == 3
               BREAK i * 100
            ENDIF
            EXIT
         ENDDO
      NEXT
   RECOVER USING x
      ? "loop break", x, i
   END
   FOR i := 1 TO 3
      BEGIN SEQUENCE
         IF i == 2
            BREAK
         ENDIF
         ? "iteration", i
      RECOVER
         ? "skipped", i
         LOOP
      END
   NEXT

   // FOR EACH variables get their values back when a BREAK leaves the loop.
   x := "kept"
   BEGIN SEQUENCE
      FOR EACH x IN a
         IF x == 20
            BREAK x
         ENDIF
      NEXT
   RECOVER USING n
      ? "each", n, x, a[ 2 ]
   END
   x := "static"
   BEGIN SEQUENCE
      EachStatic()
   RECOVER USING n
      ? "each static", n, s_x, Len( s_aList )
   END
   ? Seq2(), Seq3()

   // A call that BREAKs leaves the variable it would assign unchanged.
   x := 5
   y := 7
   e := ErrorNew()
   bOld := ErrorBlock( {| err | Break( err ) } )
   BEGIN SEQUENCE
      x := Thrower( 0 )
   RECOVER
      ? "x keeps", x
   END
   BEGIN SEQUENCE
      y := e:foo()
   RECOVER
      ? "y keeps", y
   END
   ErrorBlock( bOld )
RETURN

PROCEDURE Inner()
   LOCAL x
   BEGIN SEQUENCE
      BREAK "inner"
   RECOVER USING x
      ? "inner got", x
      BREAK x + "!"
   END
RETURN

PROCEDURE Thrower( n )
   IF n > 0
      Thrower( n - 1 )
   ELSE
      Break( "deep" )
   ENDIF
RETURN

STATIC PROCEDURE EachStatic()
   FOR EACH s_x IN s_aList
      IF s_x == 2
         BREAK s_x
      ENDIF
   NEXT
RETURN

FUNCTION Seq2()
   LOCAL n := 1
   BEGIN SEQUENCE
      n := 2
      BREAK
   RECOVER USING n
   END
RETURN n

FUNCTION Seq3()
   LOCAL n := 1
   BEGIN SEQUENCE
      BEGIN SEQUENCE
         n++
         BREAK n
      RECOVER USING n
         n *= 10
      END
      n++
   RECOVER
      n := -1
   END
RETURN n
