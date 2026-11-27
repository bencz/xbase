// The console on the screen (run with the Dump driver, which writes the screen when the program ends): ? starts a new
// line, ?? goes on in the line; Chr(10) goes to the next line's start, Chr(13) to the line's start, Chr(8) one column
// back (from a line's start to the last column of the line before), Chr(7) shows nothing, other control characters show
// in their cells; a line wraps once its last column is written; past the bottom row the screen scrolls up; Row() and
// Col() follow; Scroll() of the whole screen and SetPos( 0, 0 ) clear it for the next part.
PROCEDURE Main()
   LOCAL i, a := {}

   ?? "ab" + Chr( 10 ) + "cd"
   AAdd( a, Pos() )
   ?? "ef" + Chr( 13 ) + "g"
   AAdd( a, Pos() )
   ?? "hi" + Chr( 8 ) + "j" + Chr( 7 ) + "k" + Chr( 9 ) + "l" + Chr( 0 ) + "m"
   AAdd( a, Pos() )
   SetPos( 3, 0 )
   ?? Chr( 8 ) + "n"
   AAdd( a, Pos() )
   ? "one", 2, .T., NIL, CToD( "" )
   AAdd( a, Pos() )
   SetPos( 6, 70 )
   ?? Replicate( "w", 25 )
   AAdd( a, Pos() )
   SetPos( 9, 70 )
   ?? Replicate( "z", 10 )
   AAdd( a, Pos() )
   ? "after the full line"
   AAdd( a, Pos() )
   __ScreenDump()

   Scroll()
   SetPos( 0, 0 )
   FOR i := 1 TO 30
      ? "line", i
   NEXT
   AAdd( a, Pos() )
   ?? Replicate( "x", 85 )
   AAdd( a, Pos() )
   OutStd( "positions:" )
   AEval( a, {| p | OutStd( " " + p ) } )
   OutStd( Chr( 10 ) )
RETURN

STATIC FUNCTION Pos()
RETURN LTrim( Str( Row() ) ) + "," + LTrim( Str( Col() ) )
