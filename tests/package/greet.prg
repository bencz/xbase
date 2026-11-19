CLASS Greeter
   VAR cName INIT ""
   METHOD New( c ) INLINE ( ::cName := c, Self )
   METHOD Hello() INLINE "hello, " + ::cName
ENDCLASS

FUNCTION Twice( n )
RETURN n * 2
