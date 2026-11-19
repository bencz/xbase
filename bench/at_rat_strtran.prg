// Searching and replacing: At() and RAt() of 200 words in a 20,000-byte text of those words, and StrTran() of each
// word by a longer one; 80 rounds.
STATIC s_nSeed := 33

PROCEDURE Main()
   LOCAL t, aWords := Array( 200 ), c := "", i, j, w, r, n := 0, ok
   FOR i := 1 TO Len( aWords )
      w := ""
      FOR j := 1 TO 3 + Rand( 6 )
         w += Chr( 97 + Rand( 26 ) )
      NEXT
      aWords[ i ] := w
   NEXT
   DO WHILE Len( c ) < 20000
      c += aWords[ Rand( Rand( 200 ) + 1 ) + 1 ] + " "
   ENDDO
   t := ElapsedMs()
   FOR r := 1 TO 80
      FOR EACH w IN aWords
         n += At( w, c ) + RAt( w, c ) + Len( StrTran( c, w, "<" + w + ">" ) )
      NEXT
   NEXT
   ok := n == 620527280
   ? "at_rat_strtran", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN

STATIC FUNCTION Rand( n )
   s_nSeed := ( s_nSeed * 16807 ) % 2147483647
   RETURN s_nSeed % n
