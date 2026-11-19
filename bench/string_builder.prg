// A large text built by appending: 3 rounds of 400,000 table rows of about 37 bytes appended with += to one string
// (about 15 MB each), its length and every 1,000th byte checked.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), r, i, c, n := 0, ok
   LOCAL aNames := { "ant", "bee", "cat", "dog", "eel", "fox", "gnu", "hen", "ibis", "jay" }
   FOR r := 1 TO 3
      c := ""
      FOR i := 1 TO 400000
         c += "<tr><td>" + aNames[ i % 10 + 1 ] + "</td><td>" + LTrim( Str( i + r ) ) + "</td></tr>" + Chr( 10 )
      NEXT
      n += Len( c )
      FOR i := 1 TO Len( c ) STEP 1000
         n += Asc( SubStr( c, i, 1 ) )
      NEXT
   NEXT
   ok := n == 47588053
   ? "string_builder", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN
