// An error block that raises errors itself, nested too deep, ends the program
// with the call stack, the runtime function raising the last error first.
PROCEDURE Main()
   ErrorBlock( {| e | Inner() } )
   ? "start"
   ? 1 / "a"
RETURN

STATIC FUNCTION Inner()
RETURN SubStr( 1, 2 )
