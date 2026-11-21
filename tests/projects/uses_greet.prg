// A program with the library greet: its class (made, and inherited from by name), its functions (called, by Do() and
// @Name()), the library's own calls staying in it where the program defines a function of the same name (Decorate),
// which the program's calls, and Do(), reach; a library that uses another (cli); the library's INIT procedure before
// the program's, its EXIT procedure after the program's.
PROCEDURE Main()
   LOCAL o := Greeter():New( "xbase" ), f := @Twice()
   ? o:Hello(), Twice( 21 ), Decorate( "own" )
   ? Do( "Twice", 4 ), Eval( f, 5 ), Do( "Decorate", "by name" )
   ? Mine():New( "subclass" ):Hello(), Mine():New( "x" ):ClassName()
   ? Parsed( { "-l" } ), Parsed( { "--quiet" } )
RETURN

FUNCTION Decorate( c )
RETURN "[" + c + "]"

CLASS Mine INHERIT Greeter
   METHOD Hello() INLINE "mine:" + ::Super:Hello()
ENDCLASS

INIT PROCEDURE ProgramStarts()
   ?? " program init"
RETURN

EXIT PROCEDURE ProgramEnds()
   ? "program exit"
RETURN
