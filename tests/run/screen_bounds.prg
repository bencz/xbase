// Where xbase's screen differs from the reference, which drops text that starts left of the screen and moves the
// console's cursor from outside the screen inconsistently (run with the Dump driver): device output starting left of
// the screen shows its visible part; the console's output from a cursor outside the screen starts at the nearest cell
// inside it. Wide characters take two cells: one that does not fit at the right edge is not shown, a character written
// over either half blanks the other, and a scroll that cuts one blanks its other half; a combining mark goes with the
// character before it, and with none when that one is not shown. A UTF-8 character split between two ?? is one
// character; a position near the largest integer moves to the right edge; a wide character on a screen one column wide
// is not shown.
PROCEDURE Main()
   LOCAL a := {}

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
   OutStd( "positions:" )
   AEval( a, {| p | OutStd( " " + p ) } )
   OutStd( Chr( 10 ) )
RETURN

STATIC FUNCTION Pos()
RETURN LTrim( Str( Row() ) ) + "," + LTrim( Str( Col() ) )
