// Integer arithmetic in a loop: the sum of the first 500,000,000 odd numbers.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), i, n := 0
   FOR i := 1 TO 500000000
      n := n + i * 2 - 1
   NEXT
   ? "loops", ElapsedMs() - t, IIf( n == 250000000000000000, "ok", "WRONG" )
   RETURN
