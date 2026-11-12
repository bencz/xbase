// The macro operator's frames: a small one, in the evaluator's own stack frame, and one too large for it, allocated;
// the collector runs inside both while their items (strings made there, a codeblock's argument) are still in use.
PROCEDURE Main()
   LOCAL cDeep := "", i, b
   ? &( "Upper( 'a' + Collected( 'b' ) + 'c' )" )
   FOR i := 1 TO 60 // each level keeps its left operand on the machine's stack while the right one is computed
      cDeep += "Len( Replicate( 'x', " + LTrim( Str( i ) ) + " ) ) + ("
   NEXT
   cDeep += "Len( Collected( 'end' ) )" + Replicate( ")", 60 )
   ? &( cDeep )
   b := &( "{| c, n | Collected( c ) + Replicate( '-', n ) }" )
   FOR i := 1 TO 3
      ? Eval( b, "k" + LTrim( Str( i ) ), i )
   NEXT
RETURN

// Its argument, after a full collection.
FUNCTION Collected( c )
   __GcCollect()
RETURN c
