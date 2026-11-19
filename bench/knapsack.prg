// 0/1 knapsack by dynamic programming over one array of capacities: 200 items (weights 1 .. 1000 and values
// 1 .. 1000 from a linear congruential generator) into capacities up to 10,000, 10 times with different seeds; the best
// values are checked.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), n := 200, cap := 10000, best := Array( cap + 1 ), w := Array( n ), v := Array( n ), seed
   LOCAL i, c, wi, vi, x, rep, total := 0
   FOR rep := 1 TO 10
      seed := rep
      FOR i := 1 TO n
         seed := Int( seed * 16807 % 2147483647 )
         w[ i ] := Int( Int( seed / 65536 ) % 1000 ) + 1
         seed := Int( seed * 16807 % 2147483647 )
         v[ i ] := Int( Int( seed / 65536 ) % 1000 ) + 1
      NEXT
      AFill( best, 0 )
      FOR i := 1 TO n
         wi := w[ i ]
         vi := v[ i ]
         FOR c := cap + 1 TO wi + 1 STEP -1
            x := best[ c - wi ] + vi
            IF x > best[ c ]
               best[ c ] := x
            ENDIF
         NEXT
      NEXT
      total += best[ cap + 1 ]
   NEXT
   ? "knapsack", ElapsedMs() - t, IIf( total == 366516, "ok", "WRONG" )
   RETURN
