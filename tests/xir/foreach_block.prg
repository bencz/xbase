// FOR EACH through references, a captured variable (shared, its store followed by a barrier), and a codeblock with
// its captures.
FUNCTION Main( aList )
   LOCAL e, n := 0, b
   FOR EACH e IN aList
      n += e
   NEXT
   b := { | k | k * n }
   RETURN Eval( b, 2 )
