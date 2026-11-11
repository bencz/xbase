// A generator: the function returns gen.new with its arguments; its resume function starts with gen.restore and the
// dispatch on the state, and YIELD is gen.yield. The FOR EACH collection, saved value and position live across the
// YIELD, so they are kept in frame variables (the position boxed: box.i64, unbox.i64).
FUNCTION Count( n )
   LOCAL i, x
   FOR i := 1 TO n
      YIELD i
   NEXT
   FOR EACH x IN { 1, 2 }
      YIELD x
   NEXT
RETURN
