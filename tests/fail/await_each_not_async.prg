// AWAIT FOR EACH misused: outside an ASYNC function (one error for the statement).
PROCEDURE Main()
   ? Walk( { 1 } )
RETURN

STATIC FUNCTION Walk( a )
   LOCAL x
   AWAIT FOR EACH x IN a
   NEXT
RETURN NIL
