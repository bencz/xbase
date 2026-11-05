// A FOR loop whose control variable is a FOR EACH variable: the element may move while the array grows.
PROCEDURE Main()
   LOCAL a := { 1 }, e, k
   FOR EACH e IN a
      FOR e := 1 TO 3
         FOR k := 1 TO 20
            AAdd( a, k )
         NEXT
      NEXT
      EXIT
   NEXT
   ? a[ 1 ], Len( a )
   RETURN
