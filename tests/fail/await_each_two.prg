// AWAIT FOR EACH misused: with two variables and collections.
ASYNC FUNCTION Walk( a, b )
   LOCAL x, y
   AWAIT FOR EACH x, y IN a, b
   NEXT
RETURN NIL
