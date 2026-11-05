// Expression lists ( a, b, ... ): evaluated in order, the value of the last; in assignments, conditions, indexes
// and as a statement.
PROCEDURE Main()
   LOCAL x := 1, a := { 1, 2 }, y
   y := ( x := x + 1, x * 10 )
   ? y, x
   ? ( 1, 2, 3 )
   IF ( x := 5, x > 3 )
      ? "if list", x
   ENDIF
   ? a[ ( x := 1, x + 1 ) ]
   y := ( ( x ), x + 1 )
   ? y
   ( x := 9, Nop() )
   ? x
   RETURN
PROCEDURE Nop()
   RETURN
