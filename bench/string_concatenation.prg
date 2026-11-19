// Short strings joined with +: 3,000,000 strings of seven parts picked by index from small tables, each one's length
// and last character read; every string dies at once.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), i, s, n := 0, ok
   LOCAL aA := { "alpha", "beta", "gamma", "delta", "epsilon", "zeta", "eta" }
   LOCAL aB := { "red", "green", "blue", "cyan", "magenta" }
   FOR i := 1 TO 3000000
      s := aA[ i % 7 + 1 ] + "-" + aB[ i % 5 + 1 ] + "/" + aA[ i % 3 + 1 ] + ":" + aB[ i % 4 + 1 ]
      n += Len( s ) + Asc( Right( s, 1 ) )
   NEXT
   ok := n == 378692857
   ? "string_concatenation", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN
