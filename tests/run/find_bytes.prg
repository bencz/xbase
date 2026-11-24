// At(), RAt(), $ and StrTran() against a search written here, place by place: texts of a few letters (needles found
// often, their first and last bytes common), of bytes past 127, and long ones of letters where the needle is rare, so
// that searches cross eight-byte words, the switch from memchr to eight places at a time (64 bytes) and its stretches
// (512 bytes); needles of 1 to 20 bytes (past 16 the two-way search), taken from the text or made up; and literals,
// which the compiler finds itself.
STATIC s_nSeed := 77

PROCEDURE Main()
   LOCAL i, cText, cNeedle, nLen, nNeedle, nBad := 0, nFound := 0, nTried := 0, cAlpha, nAt, nRAt, nWant, nWantR
   LOCAL nCount, nWantCount
   LOCAL aAlpha := { "ab", "abc", "abcd", Chr( 200 ) + Chr( 201 ) + "a", "abcdefghijklmnopqrstuvwxyz" }

   FOR i := 1 TO 6000
      cAlpha := aAlpha[ Rand( Len( aAlpha ) ) + 1 ]
      nLen := IIf( Rand( 6 ) == 0, Rand( 1500 ), Rand( 70 ) )
      cText := Made( cAlpha, nLen )
      nNeedle := Rand( 20 ) + 1
      IF Rand( 2 ) == 0 .AND. nNeedle <= nLen
         cNeedle := SubStr( cText, Rand( nLen - nNeedle + 1 ) + 1, nNeedle )
      ELSE
         cNeedle := Made( cAlpha, nNeedle )
      ENDIF
      nWant := PlaceOf( cNeedle, cText, .F. )
      nWantR := PlaceOf( cNeedle, cText, .T. )
      nWantCount := Occurrences( cNeedle, cText )
      nAt := At( cNeedle, cText )
      nRAt := RAt( cNeedle, cText )
      nCount := ( Len( cText ) - Len( StrTran( cText, cNeedle, "" ) ) ) / Len( cNeedle )
      IF nAt != nWant .OR. nRAt != nWantR .OR. ( cNeedle $ cText ) != ( nWant > 0 ) .OR. nCount != nWantCount
         IF nBad < 5
            ? "different:", Len( cText ), Len( cNeedle ), nAt, nWant, nRAt, nWantR, nCount, nWantCount
         ENDIF
         nBad++
      ENDIF
      nFound += IIf( nWant > 0, 1, 0 )
      nTried++
   NEXT
   ? "searched:", nTried, "found:", nFound, "different:", nBad

   cText := Replicate( "abcdefgh", 100 ) + "needle" + Replicate( "xyz", 50 ) + "needle" + "tail"
   ? "long:", At( "needle", cText ), RAt( "needle", cText ), At( "h", cText ), RAt( "a", cText ), ;
     At( "neeDle", cText ), At( "eedle" + "x", cText )
   ? "literals:", At( "ms=", "a ms=1 ms=2" ), RAt( "ms=", "a ms=1 ms=2" ), At( "z", "abc" ), "bc" $ "abcd", ;
     RAt( "", "abc" ), At( "abcd", "abc" ), At( "abcdefghijklmnopq", "xxabcdefghijklmnopqxx" )
RETURN

// nLen bytes drawn from cAlpha.
STATIC FUNCTION Made( cAlpha, nLen )
   LOCAL c := "", i
   FOR i := 1 TO nLen
      c += SubStr( cAlpha, Rand( Len( cAlpha ) ) + 1, 1 )
   NEXT
RETURN c

// The first (lLast: the last) place of x in y, compared byte by byte; 0 for none or an empty x.
STATIC FUNCTION PlaceOf( x, y, lLast )
   LOCAL i, nAt := 0
   IF Len( x ) == 0
      RETURN 0
   ENDIF
   FOR i := 1 TO Len( y ) - Len( x ) + 1
      IF SubStr( y, i, Len( x ) ) == x
         nAt := i
         IF ! lLast
            EXIT
         ENDIF
      ENDIF
   NEXT
RETURN nAt

// The occurrences of x in y that do not overlap, from the left (as StrTran replaces them).
STATIC FUNCTION Occurrences( x, y )
   LOCAL i := 1, n := 0
   DO WHILE i <= Len( y ) - Len( x ) + 1
      IF SubStr( y, i, Len( x ) ) == x
         n++
         i += Len( x )
      ELSE
         i++
      ENDIF
   ENDDO
RETURN n

STATIC FUNCTION Rand( n )
   s_nSeed := ( s_nSeed * 16807 ) % 2147483647
RETURN s_nSeed % n
