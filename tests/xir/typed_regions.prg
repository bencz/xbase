// Typed continuations by regions: a function with a BEGIN SEQUENCE (its handler unwinds, so the function is not
// copied whole) copies the loop alone, and its ways out join the original's (subst.value joined: the operation's own
// types there).
FUNCTION Regions( n )
   LOCAL i, s := 0, c := ""
   BEGIN SEQUENCE
      c := "x"
   END SEQUENCE
   FOR i := 1 TO n
      s := s + i * 3
   NEXT
RETURN c + Str( s )
