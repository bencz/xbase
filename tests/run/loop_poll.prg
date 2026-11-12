// A loop with no call in it: its back edge tests the context's pending requests (a safepoint for a collection another
// thread makes, codegen.loop_back_edge_poll), and the loop's result is the same.
PROCEDURE Main()
   ? Sum( 1000000 ), Sum( 0 ), Sum( 1 )
RETURN

FUNCTION Sum( n )
   LOCAL i, t := 0
   FOR i := 1 TO n
      t += i
   NEXT
RETURN t
