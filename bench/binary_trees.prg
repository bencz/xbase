// The allocator and the collector: binary trees of two-element arrays made and walked, depths 4 to 14 (a stretch tree
// of depth 15, one of depth 14 that lives throughout, 2^(14 - d + 4) trees of each even depth d).
PROCEDURE Main()
   LOCAL t := ElapsedMs(), nMaxDepth := 14, nTotal := 0, aLong, d, i, nIters
   nTotal += Check( Make( nMaxDepth + 1 ) )
   aLong := Make( nMaxDepth )
   FOR d := 4 TO nMaxDepth STEP 2
      nIters := 1 << ( nMaxDepth - d + 4 )
      FOR i := 1 TO nIters
         nTotal += Check( Make( d ) )
      NEXT
   NEXT
   nTotal += Check( aLong )
   ? "binary_trees", ElapsedMs() - t, IIf( nTotal == 3222190, "ok", "WRONG" )
   RETURN

STATIC FUNCTION Make( d )
   IF d == 0
      RETURN { NIL, NIL }
   ENDIF
   RETURN { Make( d - 1 ), Make( d - 1 ) }

// The nodes of the tree.
STATIC FUNCTION Check( aNode )
   IF aNode[ 1 ] == NIL
      RETURN 1
   ENDIF
   RETURN 1 + Check( aNode[ 1 ] ) + Check( aNode[ 2 ] )
