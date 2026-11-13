// assert.ch: an ASSERT that holds does nothing (with and without a message); one that fails writes the procedure,
// its line and the message, or the expression as written, to the standard output and ends the program (exit code 0).
#include "assert.ch"

PROCEDURE Main()
   LOCAL x := 5
   ASSERT( x > 1 )
   ASSERT( x == 5, "five" )
   ? "passed"
   Check( x )
   ? "not reached"
RETURN

STATIC PROCEDURE Check( n )
   ASSERT( n * 2 < Len( "abc" ) )
RETURN
