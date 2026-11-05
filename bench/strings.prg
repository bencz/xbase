// Strings: Left, Upper, Len and At on 20,000,000 short strings.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), i, w, total := 0
   FOR i := 1 TO 20000000
      w := Upper( Left( "abcdefghij", i % 10 + 1 ) )
      total := total + Len( w ) + At( "D", w )
   NEXT
   ? "strings", ElapsedMs() - t, IIf( total == 166000000, "ok", "WRONG" )
   RETURN
