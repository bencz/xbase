// AWAIT FOR EACH misused: YIELD in its body (here the procedure that starts the program, which may await, is the
// generator), named as such rather than as an ALWAYS.
PROCEDURE Main( a )
   LOCAL x
   AWAIT FOR EACH x IN a
      YIELD x
   NEXT
RETURN
