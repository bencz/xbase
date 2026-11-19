// A hash that grows and shrinks: 20 rounds of 100,000 keys added to an empty hash (its store doubling from 8 entries)
// and deleted again, the even ones first, then the odd ones, the store shrinking as they go.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), h, r, i, nSum := 0, ok := .T.
   FOR r := 1 TO 20
      h := { => }
      FOR i := 1 TO 100000
         h[ i * 7 + r ] := i
      NEXT
      ok := ok .AND. Len( h ) == 100000
      FOR i := 2 TO 100000 STEP 2
         nSum += h[ i * 7 + r ]
         HDel( h, i * 7 + r )
      NEXT
      ok := ok .AND. Len( h ) == 50000
      FOR i := 1 TO 100000 STEP 2
         nSum += h[ i * 7 + r ]
         HDel( h, i * 7 + r )
      NEXT
      ok := ok .AND. Len( h ) == 0
   NEXT
   ok := ok .AND. nSum == 100001000000
   ? "hash_resize", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN
