// The screen's device output and clearing (run with the Dump driver): @ SAY of each type (a number as ? shows it, a
// logical as T or F, NIL, a date, an array showing nothing), text cut at the right edge with Col() past it, rows past
// the bottom or above the top not shown; DevPos(), DevOut(), DispOut(), DispOutAt(), which moves the cursor;
// DispOutAt() without a value, which leaves it; SetPos() anywhere, Row() and Col() as given; @ <row>, <col> clearing
// the rest of the row; @ CLEAR to the bottom right and @ CLEAR TO a rectangle, each with the cursor at its top left;
// Scroll() up, down, left, right and both, by more than the rectangle holds, with coordinates past the screen; CLS and
// CLEAR SCREEN; SetMode() keeping the cells that fit, and the size it is not given.
PROCEDURE Main()
   LOCAL i, a := {}

   FOR i := 0 TO 24
      @ i, 0 SAY Replicate( Chr( 65 + i ), 80 )
   NEXT
   @ 0, 0 SAY 42
   @ 1, 0 SAY -3.25
   @ 2, 0 SAY .T.
   @ 2, 2 SAY .F.
   @ 3, 0 SAY NIL
   @ 4, 0 SAY CToD( "" )
   @ 5, 0 SAY { 1, 2 }
   AAdd( a, Pos() )
   @ 6, 76 SAY "123456"
   AAdd( a, Pos() )
   @ 25, 0 SAY "below"
   @ 40, 3 SAY "far below"
   AAdd( a, Pos() )
   DevPos( 7, 10 )
   DevOut( "dev" )
   DevOut( 7 )
   AAdd( a, Pos() )
   DispOut( "disp" )
   DispOutAt( 8, 20, "at" )
   AAdd( a, Pos() )
   DispOutAt( 8, 40 )
   AAdd( a, Pos() )
   SetPos( 100, 200 )
   AAdd( a, Pos() )
   SetPos( 9, 5 )
   AAdd( a, Pos() )
   @ 10, 40
   AAdd( a, Pos() )
   @ 11, 70 CLEAR TO 12, 75
   AAdd( a, Pos() )
   __ScreenDump()

   Scroll( 1, 1, 3, 10, 1 )
   Scroll( 5, 1, 7, 10, -1 )
   Scroll( 9, 1, 10, 10, 0, 2 )
   Scroll( 12, 1, 13, 10, 0, -3 )
   Scroll( 15, 1, 17, 10, 1, -1 )
   Scroll( 19, 1, 20, 10, 5 )
   Scroll( 22, 70, 30, 100, 1 )
   Scroll( 4, 30, 2, 40, 1 )
   AAdd( a, Pos() )
   @ 14, 50 CLEAR
   AAdd( a, Pos() )
   __ScreenDump()

   CLS
   AAdd( a, Pos() )
   @ 3, 4 SAY "kept"
   @ 20, 70 SAY "cut"
   CLEAR SCREEN
   AAdd( a, Pos() )
   @ 1, 1 SAY "top left"
   @ 24, 79 SAY "Z"
   AAdd( a, Pos() )
   OutStd( "setmode:", SetMode( 10, 40 ), SetMode( 0, 5 ), SetMode( 12, NIL ), MaxRow(), MaxCol(), ;
      SetMode( NIL, 60 ), MaxRow(), MaxCol(), SetMode( 30, 100 ), Chr( 10 ) )
   AAdd( a, Pos() )
   OutStd( "positions:" )
   AEval( a, {| p | OutStd( " " + p ) } )
   OutStd( Chr( 10 ) )
RETURN

STATIC FUNCTION Pos()
RETURN LTrim( Str( Row() ) ) + "," + LTrim( Str( Col() ) )
