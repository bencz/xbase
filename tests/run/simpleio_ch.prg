// simpleio.ch: ? and ?? through OutStd(), ACCEPT reading a line of the standard input (its end dropped; at the end of
// the input, an empty string).
#include "simpleio.ch"

PROCEDURE Main()
   LOCAL a, b
   ? 1, "two", .T.
   ?? "", "more"
   ?
   ACCEPT TO a
   ACCEPT "prompt:" TO b
   ? "[" + a + "]", "[" + b + "]", Len( a ), Len( b )
RETURN
