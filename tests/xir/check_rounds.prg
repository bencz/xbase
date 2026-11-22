// Typed continuations choose their checks in rounds: no operator computes with SubStr()'s result (Asc() takes it), so
// it gets no check and may hold any type where Asc() takes it; Asc(), TOTAL of a string, may then fail and is checked
// in the next round, and the product of its 0 to 255 needs no check.
FUNCTION Codes( c )
   LOCAL i, n := 0
   FOR i := 1 TO Len( c )
      n += Asc( SubStr( c, i, 1 ) ) * 65536
   NEXT
RETURN n
