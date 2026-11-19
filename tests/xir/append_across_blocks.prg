// `c += <a call inlined in between>` stays an append in place: the load of c is in a block before the store's (the
// inlined body), which it dominates, and nothing on the way writes c.
PROCEDURE Main()
   LOCAL c := "", i
   FOR i := 1 TO 10
      c += Id( "ab" )
   NEXT
   ? c
   RETURN

STATIC FUNCTION Id( x )
RETURN x
