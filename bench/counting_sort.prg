// Counting sort of 1,000,000 keys in 0 .. 65535 (the high bits of a linear congruential generator), stable through
// prefix sums into an output array, 6 times with different seeds; the order and a weighted checksum are checked.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), n := 1000000, m := 65536, a := Array( n ), b := Array( n ), cnt := Array( m ), seed, i, k
   LOCAL rep, ok := .T., sum := 0, pos
   FOR rep := 1 TO 6
      seed := rep + 20
      FOR i := 1 TO n
         seed := Int( seed * 16807 % 2147483647 )
         a[ i ] := Int( seed / 32768 )
      NEXT
      AFill( cnt, 0 )
      FOR i := 1 TO n
         cnt[ a[ i ] + 1 ]++
      NEXT
      pos := 1
      FOR k := 1 TO m
         i := cnt[ k ]
         cnt[ k ] := pos
         pos += i
      NEXT
      FOR i := 1 TO n
         k := a[ i ] + 1
         b[ cnt[ k ] ] := a[ i ]
         cnt[ k ]++
      NEXT
      FOR i := 2 TO n
         IF b[ i - 1 ] > b[ i ]
            ok := .F.
         ENDIF
      NEXT
      FOR i := 1 TO n
         sum += b[ i ] * ( Int( i % 8 ) + 1 )
      NEXT
   NEXT
   ? "counting_sort", ElapsedMs() - t, IIf( ok .AND. sum == 884633960242, "ok", "WRONG" )
   RETURN
