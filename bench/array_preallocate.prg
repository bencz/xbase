// Arrays made at their size: 25 arrays of 2,000,000 numbers made with Array( n ), filled by index, then summed by
// index (array_append builds the same arrays with AAdd()).
PROCEDURE Main()
   LOCAL t := ElapsedMs(), a, r, i, n := 2000000, nSum := 0, ok
   FOR r := 1 TO 25
      a := Array( n )
      FOR i := 1 TO n
         a[ i ] := i + r
      NEXT
      FOR i := 1 TO n
         nSum += a[ i ]
      NEXT
   NEXT
   ok := nSum == 50000675000000 .AND. Len( a ) == n
   ? "array_preallocate", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN
