// Colors (run with the Dump driver, whose dump lists each row's attributes): SetColor() reads pairs of foreground and
// background (letters N B G BG R RB GR W in any case and order, U, I, X, numbers, + and * on either side, blanks, empty
// halves and pairs, more slashes) and gives them back written its way; an empty string gives the defaults; the
// unselected pair follows an enhanced one given without it. ColorSelect() picks the pair the output (? too) writes in
// and that Scroll(), @ CLEAR and the console's scrolling blank with; SetColor() picks the standard pair again. A COLOR
// clause, and the color of DevOut(), DispOut() and DispOutAt(), takes its string's first pair (the standard one when it
// gives none); the output sets the colors to that string and back, so the standard pair is selected after it. SET COLOR
// TO, SET COLOUR TO, SetBlink(), IsColor(). Random color strings read as the reference reads them.
PROCEDURE Main()
   LOCAL c, i, cOld
   PRIVATE cMacro := "GR+/B,W/R"

   OutStd( "default " + SetColor() + Chr( 10 ) )
   i := 0
   FOR EACH c IN { "W/N", "N/W", "B/N", "G/N", "BG/N", "R/N", "RB/N", "GR/N", "w+/b", "N/W*", "W+/B*", "U", "I", "X", ;
         "U/R", "I/B", "I/R", "R/I", "I+", "I/U", "X/B", "B/X", "WX/N", "XW/N", "U+/W", "W/U", "15/1", "7/0", "R/12", ;
         "255/3", "-1/2", "16/0", "GR+/RB", "BG+*/GR", "*W/N", "W/N+", "+R/B", "G B/N", " W / B ", "W/N/R", ;
         "R/", "/R", "N", "+", "*", "/", "BOGUS", "W/N,R/W", ",G/R", "B/W,,,,R/G", "", ",,,,G/B", "2N", "1BNG", "9 1", ;
         "N/8 1", "9X", "9N", "R/XB", "/XG", "R/XI", "IX", "I3/N", "IN/N", "IU/N", "I9", "+WI2R", "BI 2/", "9/1/", ;
         "B1/9/9", "+R/9/G", "cM", "Q", "?", "-", "OS", "W/N,Q", "W/N,R/W,,,Q" }
      cOld := SetColor( c )
      @ i % 25, Int( i / 25 ) * 3 SAY "x"
      OutStd( PadR( "[" + c + "]", 14 ) + " was " + PadR( cOld, 24 ) + " is " + SetColor() + Chr( 10 ) )
      i++
   NEXT

   Random()
   SetColor( "W/N,R/W,G/N,B/N,GR/N" )
   FOR i := 0 TO 6
      ColorSelect( i )
      @ i, 10 SAY "y"
   NEXT
   ColorSelect( 0 )
   @ 0, 12 SAY "z" COLOR "B/W"
   @ 1, 12 SAY "z" COLOR "B/W,R/G"
   @ 2, 12 SAY "z" COLOR ",R/G"
   @ 3, 12 SAY "z" COLOR ""
   @ 4, 12 SAY "z" COLOR "BOGUS"
   SetPos( 5, 12 )
   DevOut( "d", "GR/B" )
   DispOut( "e", "BG+/R" )
   DevOut( "f", 7 )
   DispOutAt( 6, 12, "g", "N/G" )
   ColorSelect( 1 )
   SetPos( 7, 12 )
   ?? "q"
   DevOut( "s" )
   Scroll( 8, 12, 8, 15 )
   @ 9, 12 CLEAR TO 9, 15
   SetColor( "R/B" )
   @ 10, 12 SAY "standard again"
   Scroll( 11, 12, 11, 20, 1 )
   SET COLOR TO W+/B, GR/N
   OutStd( "set color to " + SetColor() + Chr( 10 ) )
   SET COLOR TO &cMacro
   OutStd( "set color to &macro " + SetColor() + Chr( 10 ) )
   SET COLOUR TO
   OutStd( "set colour to " + SetColor() + Chr( 10 ) )
   SET COLOR TO ( "BG/N" )
   OutStd( "set color to (expr) " + SetColor() + Chr( 10 ) )
   OutStd( "blink", SetBlink(), SetBlink( .F. ), SetBlink(), SetBlink( .T. ), "iscolor", IsColor(), Chr( 10 ) )
   SetColor( "W/N,R/W,G/B,B/G,GR/N" )
   ColorSelect( 1 )
   SetPos( 12, 12 )
   DevOut( "a", ",R/G" )
   DevOut( "b" )
   ColorSelect( 1 )
   DevOut( "c", "" )
   DevOut( "d" )
   ColorSelect( 1 )
   @ 12, 20 SAY "e" COLOR "BG/N,R/B"
   @ 12, 21 SAY "f"
   ColorSelect( 1 )
   DevOut( "g", 7 )
   DevOut( "h" )
   SetColor( "GR/RB" )
   SetPos( 24, 30 )
   ? "scrolled"
   SetColor( "" )
RETURN

// Color strings made of random characters (no U, which the reference keeps writing after another letter): each as
// SetColor() reads and writes it.
STATIC PROCEDURE Random()
   LOCAL nSeed := 7, i, j, c, cChars := "NBGRWIX+*/0129 ?q,"
   FOR i := 1 TO 300
      c := ""
      FOR j := 0 TO i % 8
         nSeed := ( nSeed * 1103515245 + 12345 ) % 2147483648
         c += SubStr( cChars, Int( nSeed / 65536 ) % Len( cChars ) + 1, 1 )
      NEXT
      SetColor( "" )
      SetColor( c )
      OutStd( "[" + c + "] " + SetColor() + Chr( 10 ) )
   NEXT
RETURN
