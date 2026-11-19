// Arrays grown one element at a time: 5 arrays of 2,000,000 numbers built with AAdd() from empty, then summed.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), a, r, i, x, nSum := 0, ok
   FOR r := 1 TO 5
      a := {}
      FOR i := 1 TO 2000000
         AAdd( a, i + r )
      NEXT
      FOR EACH x IN a
         nSum += x
      NEXT
   NEXT
   ok := nSum == 10000035000000 .AND. Len( a ) == 2000000
   ? "array_append", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN
