// DO &name [WITH args]: the procedure the text names when the statement runs, its arguments as DO name WITH passes
// them (a variable, LOCAL or memory variable, by reference; an element or another expression by value; @x by
// reference; empty arguments NIL); `&name.`, `&name.suffix`, `&( expr )`, blanks around the name and any letter case;
// and the errors: a name no function has (1001), a text that is not a name (1449), a value that is not a string (1065).
MEMVAR c, p

PROCEDURE Main()
   LOCAL x := 1, a := { 1, 2 }, e
   PRIVATE c := "Hi", p := 5
   DO &c WITH x, a[ 1 ], p, 7
   ? "after:", x, a[ 1 ], p
   DO &c
   c := "hi "
   DO &c WITH @x, , 3
   ? x
   c := "Hi2"
   DO &c.
   c := "H"
   DO &c.i WITH 1
   DO &( "H" + "i" ) WITH 2
   ErrorBlock( {| o | Break( o ) } )
   FOR EACH c IN { "NoSuch", "1+", 3 }
      BEGIN SEQUENCE
         DO &c
      RECOVER USING e
         ? "error", e:subCode, e:operation, e:description
      END SEQUENCE
   NEXT
RETURN

PROCEDURE Hi( a, b, q, d )
   ? "hi", a, b, q, d, PCount()
   a := 10
   b := 20
   q := 30
RETURN

PROCEDURE Hi2
   ? "hi2"
RETURN
