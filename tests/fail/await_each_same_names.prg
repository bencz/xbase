// AWAIT FOR EACH misused: with one name for k and v.
ASYNC FUNCTION Walk( a )
   LOCAL x
   AWAIT FOR EACH x => x IN a
   NEXT
RETURN NIL
