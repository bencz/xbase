// Calls of small functions of the module, inlined: an argument without a reference runs the body after the versioning
// test (the parameter a value promote-variables takes), one with a reference (@n) the body versioned for references;
// the function's variables still in memory are cleared before inline.leave. A recursive call stays a call.
FUNCTION Main( n )
   LOCAL s := Sq( n )
   Bump( @n )
   RETURN s + n + Fact( 3 )

STATIC FUNCTION Sq( x )
   LOCAL r := x * x
RETURN r

STATIC PROCEDURE Bump( x )
   x++
RETURN

STATIC FUNCTION Fact( k )
RETURN iif( k <= 1, 1, k * Fact( k - 1 ) )
