// Integers and doubles in one expression (i + 3.14, i / 4), double to integer (Int()), comparisons across both.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), i, d, s := 0, n := 0
   FOR i := 1 TO 20000000
      d := i + 3.14
      s += Int( d * 4 ) % 7
      IF d > i + 3
         n++
      ENDIF
      s += i / 4
   NEXT
   ? "mixed", ElapsedMs() - t, IIf( s == 50000062499998 .AND. n == 20000000, "ok", "WRONG" )
   RETURN
