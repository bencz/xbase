// Loops nested in a loop whose first pass is peeled and whose slow paths go on into the inner loops (AEval and its
// codeblock run in place, next to an AEval of a block made at run time): the outer loop's polls, removed, go on into
// the inner loops' headers through the preheaders inserted for them after the polls were found (the compiler crashed
// on this program), and the program runs as without the passes.
PROCEDURE Main()
   LOCAL a := Array( 3000 ), i, t, r, b := &( "{| x | x * 2 }" )
   FOR i := 1 TO Len( a )
      a[ i ] := i % 7
   NEXT
   FOR r := 1 TO 3
      t := Len( a )
      FOR i := 1 TO 10
         AEval( a, {| x | x * 2 } )
      NEXT
      ? "literal", Len( a ) - t
      t := Len( a )
      FOR i := 1 TO 10
         AEval( a, b )
      NEXT
      ? "made at run time", Len( a ) - t, a[ 1 ] + a[ 3000 ]
   NEXT
RETURN
