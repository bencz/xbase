// Appends whose operand is a sum of strings, appended piece by piece (split-appends) where the pieces are known to be
// strings, and the inline copy of each piece (8, 4, 2 and 1 bytes at both ends up to 16, memcpy past it): pieces of
// every length from 0 to 40 bytes; sums of calls in a loop (their results checked); a sum that holds the variable
// itself (`s += s + "x"`); `s := s + a + b`; a piece of unknown type (a parameter: no split, its error as the `+`'s);
// SubStr() with positions and counts that are numbers with decimals (`%` and `/` make them).
PROCEDURE Main()
   LOCAL s, k, i, c := "", cAlpha := "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
   FOR k := 0 TO 40
      s := "<"
      s += Left( cAlpha, k ) + "|" + Right( cAlpha, k ) + ">"
      c += IIf( s == "<" + Left( cAlpha, k ) + "|" + Right( cAlpha, k ) + ">", "", " len " + Str( k, 2 ) + " wrong" )
   NEXT
   ? "lengths 0 to 40:", IIf( Empty( c ), "ok", c )
   s := ""
   FOR i := 1 TO 1000
      s += Chr( i % 26 + 65 ) + SubStr( cAlpha, i % 62 + 1, 1 ) + Chr( i % 10 + 48 ) + "."
   NEXT
   ? "calls in a loop:", Len( s ), Left( s, 12 ), Right( s, 12 ), Hash32( s )
   s := "ab"
   FOR i := 1 TO 3
      s += s + "x"
   NEXT
   ? "the variable itself:", s
   s := "["
   FOR i := 1 TO 5
      s := s + Str( i, 1 ) + "," + Chr( 96 + i )
   NEXT
   ? "s := s + a + b:", s
   ? "unknown type:", Pieces( "a" ), Pieces( 7 )
   ? "positions with decimals:", SubStr( cAlpha, 10 % 4 + 1, 1 ), SubStr( cAlpha, 7 / 2, 1 ), ;
      SubStr( cAlpha, 2.9, 1.5 ), SubStr( cAlpha, 63 % 64, 1 ), "[" + SubStr( cAlpha, 63.5, 1 ) + "]", ;
      SubStr( cAlpha, 0.5, 1 ), Asc( SubStr( cAlpha, 26 % 27, 1 ) )
RETURN

// x is a string or not: s += "<" + x + ">" appends as a whole (the `+` fails as it would).
STATIC FUNCTION Pieces( x )
   LOCAL s := "", cOut
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      s += "<" + x + ">"
      cOut := s
   RECOVER USING cOut
      cOut := cOut:Description + " " + cOut:Operation
   END SEQUENCE
RETURN cOut

// A checksum of the bytes, so the line stays short.
STATIC FUNCTION Hash32( s )
   LOCAL n := 0, i
   FOR i := 1 TO Len( s )
      n := ( n * 31 + Asc( SubStr( s, i, 1 ) ) ) % 1000000007
   NEXT
RETURN n
