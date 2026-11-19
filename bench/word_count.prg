// Word frequencies: 200,000 lines of 1 to 30 words (about 1,300,000 words, of a 5,000-word vocabulary in mixed case
// and a skewed distribution) cut at blanks, lower-cased and counted in a hash; the most frequent word found.
STATIC s_nSeed := 4242

PROCEDURE Main()
   LOCAL t, aVocab := Array( 5000 ), aLines := Array( 200000 ), i, j, w, c, n, h := { => }, nWords := 0, ok
   LOCAL cTop := "", nTop := 0, nCheck := 0
   FOR i := 1 TO Len( aVocab )
      w := ""
      FOR j := 1 TO 2 + Rand( 9 )
         c := SubStr( "eEtaAoinNsrhldcumfpgwybvkxjqz", Rand( Rand( 29 ) + 1 ) + 1, 1 )
         w += c
      NEXT
      aVocab[ i ] := w
   NEXT
   FOR i := 1 TO Len( aLines )
      c := ""
      FOR j := 1 TO 1 + Rand( 30 )
         w := aVocab[ Rand( Rand( 5000 ) + 1 ) + 1 ]
         c += w + " "
      NEXT
      aLines[ i ] := c
   NEXT
   t := ElapsedMs()
   FOR EACH c IN aLines
      DO WHILE ( n := At( " ", c ) ) > 0
         w := Lower( Left( c, n - 1 ) )
         c := SubStr( c, n + 1 )
         nWords++
         IF w $ h
            h[ w ]++
         ELSE
            h[ w ] := 1
         ENDIF
      ENDDO
   NEXT
   FOR EACH n IN h
      nCheck += n * n
      IF n > nTop
         nTop := n
      ENDIF
   NEXT
   FOR i := 1 TO Len( aVocab )
      IF h[ Lower( aVocab[ i ] ) ] == nTop
         cTop := Lower( aVocab[ i ] )
         EXIT
      ENDIF
   NEXT
   ok := nWords == 1312183 .AND. Len( h ) == 4328 .AND. nTop == 7336 .AND. cTop == "ee" .AND. ;
      nCheck == 971695061
   ? "word_count", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN

STATIC FUNCTION Rand( n )
   s_nSeed := ( s_nSeed * 16807 ) % 2147483647
   RETURN s_nSeed % n
