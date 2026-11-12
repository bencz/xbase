// Typed continuations: in a loop, an operation whose error may substitute its result and whose own types hold no heap
// reference ends its block with subst.check; a substituted value continues in the generic copy of the rest of the
// function (blocks named .any), joined to the original's values by block parameters.
FUNCTION Sum( a )
   LOCAL i, s := 0
   FOR i := 1 TO Len( a )
      s += a[ i ] * 2
   NEXT
RETURN s
