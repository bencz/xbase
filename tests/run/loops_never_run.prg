// Loops whose bounds let no pass run (FOR k := 1 TO 0, a negative step from below, bounds known only at run time):
// the optimizer's ranges for their counters are empty, and an empty range is one however its bounds read (1..0 or
// any other lo > hi), so its fixpoints settle (two empty ranges written differently once made -O1/-O2 loop forever
// on the first of these). Each still runs as Clipper's: no pass, the counter left at its first value.
PROCEDURE Main( p )
   LOCAL r := "", k, n := 0, a := {}
   FOR k := 1 TO 0
      r += Cs( p )
   NEXT
   ? "string, no pass:", "[" + r + "]", k
   FOR k := 1 TO 0
      n += k * 2
      r += Cs( "A" )
   NEXT
   ? "number, no pass:", n, k
   FOR k := 5 TO 10 STEP -1
      AAdd( a, k )
      r += Str( k )
   NEXT
   ? "negative step, no pass:", Len( a ), k
   FOR k := 1 TO Len( a )
      n += Cs( p ) == NIL
   NEXT
   ? "bound at run time, no pass:", n, k
   FOR k := 1 TO 0
      FOR n := 1 TO 3
         r += Cs( p )
      NEXT
   NEXT
   ? "nested, no pass:", "[" + r + "]", k, n
RETURN

STATIC FUNCTION Cs( x )
   IF x == "A"
      RETURN Str( x )
   ENDIF
RETURN NIL
