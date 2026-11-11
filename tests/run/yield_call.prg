// YIELD followed by a parenthesis is a call of a function Yield(), as in Clipper: the function that makes it stays an
// ordinary function, which may RETURN a value.
PROCEDURE Main()
   ? ValType( Foo() ), Foo()
RETURN

FUNCTION Foo()
   Yield( 3 )
   YIELD( 4 )
RETURN 7

FUNCTION Yield( n )
   ?? "called", n, ""
RETURN NIL
