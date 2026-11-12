// The result of a call within the module, which the callee writes when it returns (the caller does not clear it
// first): a function that returns nothing gives NIL, also into a variable that held a value; a function left by BREAK
// gives nothing, and the variable its result was going to keeps what it held.
PROCEDURE Main()
   LOCAL x := "before", y := 42, e
   x := Nothing()
   ? ValType( x ), x
   BEGIN SEQUENCE
      y := Leaves()
   RECOVER USING e
      ? "recovered", e
   END
   ? ValType( y ), y
   y := Twice( 21 )
   ? y, Twice( Twice( 2 ) )
RETURN

PROCEDURE Nothing()
RETURN

FUNCTION Leaves()
   BREAK "out"
RETURN 1

FUNCTION Twice( n )
RETURN n * 2
