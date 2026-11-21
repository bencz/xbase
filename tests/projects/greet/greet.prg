// The library's class and functions: Hello() calls Decorate() of the library's other module, Twice() a STATIC
// function of its own module, Parsed() the cli library.
CLASS Greeter
   VAR cName
   METHOD New( cName ) INLINE ::cName := cName, Self
   METHOD Hello() INLINE Decorate( "Hello, " + ::cName )
ENDCLASS

FUNCTION Twice( n )
RETURN Double( n )

STATIC FUNCTION Double( n )
RETURN n * 2

FUNCTION Parsed( aArgs )
   LOCAL o := CommandLine():New( "greet" )
   o:AddFlag( "loud", "l", "" )
RETURN o:Parse( aArgs ) .AND. o:Value( "loud" )

// The library initializes before the program that uses it, and ends after it.
INIT PROCEDURE GreetStarts()
   ?? "greet init;"
RETURN

EXIT PROCEDURE GreetEnds()
   ? "greet exit"
RETURN
