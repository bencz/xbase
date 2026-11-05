// Clipper's evaluation order in xIR: compound assignment, post-increment of an element, FOR with STEP, IIF and
// .AND. with its short circuit (block parameters).
FUNCTION Main()
   LOCAL a := { 1, 2 }, x := 0, i
   FOR i := 10 TO 1 STEP -2
      x += i
   NEXT
   a[ 1 ]++
   RETURN IIf( x > 2 .AND. x < 100, a[ 1 ], "out" )
