// For platform.shrink: a recursion 20,000 deep that makes a PRIVATE at each level.
FUNCTION DeepPrivates( n )
   IF n == 0
      RETURN 0
   ENDIF
   PRIVATE x := n
   RETURN x + DeepPrivates( n - 1 )

