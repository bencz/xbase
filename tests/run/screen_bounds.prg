// Where xbase's screen differs from the reference, which drops text that starts left of the screen and moves the
// console's cursor from outside the screen inconsistently (run with the Dump driver): device output starting left of
// the screen shows its visible part; the console's output from a cursor outside the screen starts at the nearest cell
// inside it. Wide characters take two cells: one that does not fit at the right edge is not shown, a character written
// over either half blanks the other, and a scroll that cuts one blanks its other half; a combining mark goes with the
// character before it, and with none when that one is not shown. A UTF-8 character split between two ?? is one
// character; a position near the largest integer moves to the right edge; a wide character on a screen one column wide
// is not shown. A color written U and then another letter is that color (the reference keeps writing it U), and a color
// string keeps five pairs, as Clipper's (the reference keeps more). SaveScreen() keeps what its bytes cannot give (wide
// characters, other code points, marks) after the cells, which RestScreen() brings back whole, also after a program
// changes the attribute bytes (shadows, by Stuff() and by Transform()); a cell whose character byte the program changed
// keeps that character.
PROCEDURE Main()
   LOCAL a := {}, cScreen, cShadow, i

   @ 0, -2 SAY "abcdef"
   AAdd( a, Pos() )
   @ 1, -10 SAY "hidden"
   AAdd( a, Pos() )
   SetPos( 2, -3 )
   ?? "neg"
   AAdd( a, Pos() )
   SetPos( 40, 5 )
   ?? "low"
   AAdd( a, Pos() )
   SetPos( 3, 90 )
   ?? "right"
   AAdd( a, Pos() )

   @ 6, 0 SAY "日本語"
   @ 7, 78 SAY "日本"
   AAdd( a, Pos() )
   @ 8, 0 SAY "日本語"
   @ 8, 1 SAY "x"
   @ 9, 0 SAY "日本語"
   @ 9, 2 SAY "y"
   @ 10, 0 SAY "e" + Chr( 204 ) + Chr( 129 ) + "a"
   AAdd( a, Pos() )
   @ 11, 0 SAY "ab日本語"
   Scroll( 11, 3, 11, 20, 0, 1 )
   SetPos( 12, 78 )
   ?? "中"
   AAdd( a, Pos() )
   @ 13, 0 SAY "日a本"
   Scroll( 13, 0, 13, 3, 0, -3 )
   @ 14, 0 SAY "日本"
   Scroll( 14, 1, 14, 3, 0, 2 )
   @ 15, 79 SAY "ab" + Chr( 204 ) + Chr( 129 )
   @ 16, 0 SAY "x"
   @ 16, 79 SAY "日" + Chr( 204 ) + Chr( 129 )
   SetPos( 17, 0 )
   ?? "a" + Chr( 195 )
   ?? Chr( 167 ) + "b"
   AAdd( a, Pos() )
   SetPos( 18, 2147483647 )
   DevOut( "abc" )
   AAdd( a, Pos() )
   __ScreenDump()
   SetMode( 2, 1 )
   SetPos( 0, 0 )
   ?? "日z"
   AAdd( a, Pos() )
   SetMode( 25, 80 )
   @ 20, 0 SAY "日本語 ação e" + Chr( 204 ) + Chr( 129 ) + " ─ x"
   cScreen := SaveScreen( 20, 0, 20, 19 )
   RestScreen( 21, 2, 21, 21, cScreen )
   OutStd( "saved:", Len( cScreen ), Chr( 10 ) )
   cShadow := cScreen
   FOR i := 2 TO Len( cShadow ) STEP 2
      cShadow := Stuff( cShadow, i, 1, Chr( 8 ) )
   NEXT
   RestScreen( 22, 0, 22, 19, cShadow )
   cShadow := cScreen
   FOR i := 2 TO Len( cShadow ) STEP 2
      cShadow := Stuff( cShadow, i, 1, Chr( 0 ) )
   NEXT
   RestScreen( 23, 0, 23, 19, cShadow )
   RestScreen( 24, 0, 24, 19, Transform( cScreen, Replicate( "X" + Chr( 30 ), Len( cScreen ) / 2 ) ) )
   RestScreen( 19, 0, 19, 19, Stuff( cScreen, 1, 1, "Z" ) )
   OutStd( "colors:", SetColor( "UW/N" ), SetColor( "UN/N" ), SetColor( "B/W,R/G,BG/R,GR/B,W/N,R/R,G/G" ), ;
      SetColor(), Chr( 10 ) )
   OutStd( "positions:" )
   AEval( a, {| p | OutStd( " " + p ) } )
   OutStd( Chr( 10 ) )
RETURN

STATIC FUNCTION Pos()
RETURN LTrim( Str( Row() ) ) + "," + LTrim( Str( Col() ) )
