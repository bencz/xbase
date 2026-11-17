// Finding bytes in bytes against a plain search written here: At(), RAt(), $ and StrTran() over texts of small
// alphabets (so needles match often and partly: periodic needles, overlaps), with NUL and bytes above 127, needles of
// 1 to 40 bytes (short ones and the two-way search's), taken from the text (found) or made up (often not found), and
// needles longer than the text; half the texts repeat a short unit with a byte changed here and there (needles taken
// from them repeat with a period: the two-way search's other branch); empty needles and texts. Then a needle that
// defeats a plain search (2000 bytes, all alike but the last, in a text of those bytes) costs about what an easy one
// does.
PROCEDURE Main()
   LOCAL nSeed := 12345, aAlpha := { "ab", "abc", "a" + Chr( 0 ), Chr( 200 ) + "a" + Chr( 255 ) }, cText, cNeedle
   LOCAL i, k, nLen, nBad := 0, nCases := 0, nFound := 0, cAlpha, cRepl
   FOR EACH cAlpha IN aAlpha
      FOR i := 1 TO 150
         IF i % 2 == 0 // a short unit repeated, a byte changed here and there: periodic needles
            cText := Periodic( Made( cAlpha, Rand( @nSeed, 5 ), @nSeed ), cAlpha, Rand( @nSeed, 300 ) - 1, @nSeed )
         ELSE
            cText := Made( cAlpha, Rand( @nSeed, 300 ) - 1, @nSeed )
         ENDIF
         FOR k := 1 TO 6
            nLen := Rand( @nSeed, 40 )
            IF k % 2 == 0 .AND. Len( cText ) > 0 // taken from the text
               cNeedle := SubStr( cText, Rand( @nSeed, Len( cText ) ), nLen )
            ELSE
               cNeedle := Made( cAlpha, nLen, @nSeed )
            ENDIF
            cRepl := Left( "XYZ", Rand( @nSeed, 4 ) - 1 )
            nCases++
            IF PlainAt( cNeedle, cText ) > 0
               nFound++
            ENDIF
            IF At( cNeedle, cText ) != PlainAt( cNeedle, cText ) .OR. ;
               RAt( cNeedle, cText ) != PlainRAt( cNeedle, cText ) .OR. ;
               ( cNeedle $ cText ) != ( PlainAt( cNeedle, cText ) > 0 ) .OR. ;
               !( StrTran( cText, cNeedle, cRepl ) == PlainTran( cText, cNeedle, cRepl ) )
               nBad++
               IF nBad <= 5
                  ? "MISMATCH:", Len( cText ), Len( cNeedle ), At( cNeedle, cText ), PlainAt( cNeedle, cText ), ;
                     RAt( cNeedle, cText ), PlainRAt( cNeedle, cText )
               ENDIF
            ENDIF
         NEXT
      NEXT
   NEXT
   ? "cases:", nCases, "found in many:", nFound > nCases / 3, "mismatches:", nBad
   ? "empty:", At( "", "abc" ), RAt( "", "abc" ), "" $ "abc", At( "a", "" ), "a" $ "", StrTran( "abc", "", "x" ), ;
      StrTran( "", "a", "x" ) == ""
   ? "longer:", At( "abcd", "abc" ), RAt( "abcd", "abc" ), "abcd" $ "abc"
   ? "hard needle linear:", Hard()
RETURN

// Milliseconds: a needle of 2000 bytes of "a" and a "b" in 2 MB of "a" (a plain search compares about 4e9 bytes)
// against one of "b" and 1999 "a" (it fails at the first byte).
STATIC FUNCTION Hard()
   LOCAL cText := Replicate( "a", 2000000 ), cHard := Replicate( "a", 1999 ) + "b"
   LOCAL cEasy := "b" + Replicate( "a", 1999 )
   LOCAL t, nHard, nEasy, i, n := 0
   t := Seconds()
   FOR i := 1 TO 5
      n += At( cEasy, cText ) + RAt( cEasy, cText )
   NEXT
   nEasy := ( Seconds() - t ) * 1000
   t := Seconds()
   FOR i := 1 TO 5
      n += At( cHard, cText ) + RAt( cHard, cText )
   NEXT
   nHard := ( Seconds() - t ) * 1000
RETURN IIf( n == 0 .AND. nHard < 8 * nEasy + 200, "yes", "NO" )

// A pseudo-random number from 1 to n (the same on every platform).
STATIC FUNCTION Rand( nSeed, n )
   nSeed := ( nSeed * 1103515245 + 12345 ) % 2147483648
RETURN Int( nSeed / 65536 ) % n + 1

STATIC FUNCTION Made( cAlpha, n, nSeed )
   LOCAL c := "", i
   FOR i := 1 TO n
      c += SubStr( cAlpha, Rand( @nSeed, Len( cAlpha ) ), 1 )
   NEXT
RETURN c

STATIC FUNCTION Periodic( cUnit, cAlpha, n, nSeed )
   LOCAL c := "", i
   FOR i := 1 TO n
      c += IIf( Rand( @nSeed, 40 ) == 1, SubStr( cAlpha, Rand( @nSeed, Len( cAlpha ) ), 1 ), ;
         SubStr( cUnit, ( i - 1 ) % Len( cUnit ) + 1, 1 ) )
   NEXT
RETURN c

STATIC FUNCTION PlainAt( cNeedle, cText )
   LOCAL i
   IF Len( cNeedle ) == 0
      RETURN 0
   ENDIF
   FOR i := 1 TO Len( cText ) - Len( cNeedle ) + 1
      IF SubStr( cText, i, Len( cNeedle ) ) == cNeedle
         RETURN i
      ENDIF
   NEXT
RETURN 0

STATIC FUNCTION PlainRAt( cNeedle, cText )
   LOCAL i
   IF Len( cNeedle ) == 0
      RETURN 0
   ENDIF
   FOR i := Len( cText ) - Len( cNeedle ) + 1 TO 1 STEP -1
      IF SubStr( cText, i, Len( cNeedle ) ) == cNeedle
         RETURN i
      ENDIF
   NEXT
RETURN 0

// Each occurrence from the left replaced, the scan going on after it.
STATIC FUNCTION PlainTran( cText, cNeedle, cRepl )
   LOCAL c := "", i := 1
   IF Len( cNeedle ) == 0
      RETURN cText
   ENDIF
   DO WHILE i <= Len( cText )
      IF SubStr( cText, i, Len( cNeedle ) ) == cNeedle
         c += cRepl
         i += Len( cNeedle )
      ELSE
         c += SubStr( cText, i, 1 )
         i++
      ENDIF
   ENDDO
RETURN c
