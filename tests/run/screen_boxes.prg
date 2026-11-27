// Frames, saved screens, held updates and the cursor's shape (run with the Dump driver): DispBox() with no frame, the
// numbers 1 and 2 and others, strings of eight and nine characters, shorter ones (the last repeated), an empty one (the
// box blanked), one cut by a Chr(0), one of CP437's bytes; a corner that is no number (no box); frames of one row, one
// column, one cell, two by two; corners in any order; frames past the screen's edges; the cursor after each; @ BOX, @
// TO, @ TO DOUBLE, with COLOR; box.ch's frames. SaveScreen() as two bytes a cell (the rectangle sorted and brought
// within the screen), changed as a string (its attribute bytes) and given back by RestScreen(), whole cells only, row
// by row, cut at the screen; SAVE SCREEN TO, RESTORE SCREEN FROM, SAVE SCREEN and RESTORE SCREEN (the cursor too,
// once). DispBegin(), DispEnd() and DispCount(); SetCursor() and SET CURSOR; @ SAY PICTURE (also after COLOR) and
// DevOutPict().
#include "box.ch"
#include "setcurs.ch"

PROCEDURE Main()
   LOCAL a := {}, s, i, cKept

   FOR i := 0 TO 24
      @ i, 0 SAY Replicate( ".", 80 )
   NEXT
   DispBox( 0, 0, 3, 6 )
   AAdd( a, Pos() )
   DispBox( 0, 8, 3, 14, 1 )
   DispBox( 0, 16, 3, 22, 2 )
   DispBox( 0, 24, 3, 30, "12345678" )
   DispBox( 0, 32, 3, 38, "123456789" )
   DispBox( 0, 40, 3, 46, "ab" )
   DispBox( 0, 48, 3, 54, "" )
   DispBox( 0, 56, 3, 62, 3 )
   DispBox( 0, 64, 3, 70, 1.7 )
   DispBox( 0, 72, 3, 78, "1" + Chr( 0 ) + "345678" )
   AAdd( a, Pos() )
   DispBox( 5, 0, 5, 10, "12345678" )
   DispBox( 7, 0, 10, 0, "12345678" )
   DispBox( 5, 12, 5, 12, "12345678" )
   DispBox( 5, 14, 6, 15, "123456789" )
   DispBox( 12, 10, 9, 5, "12345678" )
   AAdd( a, Pos() )
   DispBox( 12, 70, 14, 90, "12345678" )
   DispBox( 22, 20, 30, 25, "12345678" )
   DispBox( -1, 30, 2, 35, "12345678" )
   DispBox( 16, -2, 19, 3, "12345678" )
   DispBox( 12, 20, 14, 26, "123456789", "B/W" )
   AAdd( a, Pos() )
   @ 16, 8, 18, 14 BOX "abcdefgh"
   @ 16, 16 TO 18, 22
   @ 16, 24 TO 18, 30 DOUBLE COLOR "R/G"
   AAdd( a, Pos() )
   @ 20, 0 TO 20, 10
   @ 20, 12 TO 22, 12 DOUBLE
   @ 6, 40, 8, 46 BOX B_SINGLE
   @ 6, 48, 8, 54 BOX B_DOUBLE + " "
   @ 6, 56, 8, 62 BOX B_SINGLE_DOUBLE
   @ 6, 64, 8, 70 BOX B_DOUBLE_SINGLE
   DispBox( NIL, 72, 3, 78 )
   DispBox( "x", 72, 3, 78 )
   AAdd( a, Pos() )
   DispBox( 10, 40, 12, 46, Chr( 201 ) + Chr( 205 ) + Chr( 187 ) + Chr( 186 ) + Chr( 188 ) + Chr( 205 ) + Chr( 200 ) + ;
      Chr( 186 ) )
   __ScreenDump()

   s := SaveScreen( 0, 0, 1, 3 )
   OutStd( "lengths", Len( s ), Len( SaveScreen() ), Len( SaveScreen( 5, 5 ) ), Len( SaveScreen( 3, 3, 1, 1 ) ), ;
      Len( SaveScreen( 24, 78, 26, 81 ) ), Len( SaveScreen( -2, -2, 0, 0 ) ), Len( SaveScreen( 30, 90, 31, 91 ) ), ;
      Chr( 10 ) )
   CLS
   RestScreen( 10, 40, 11, 43, s )
   RestScreen( 10, 50, 11, 53, Left( s, 4 ) )
   RestScreen( 10, 60, 10, 61, s )
   RestScreen( 24, 78, 25, 81, s )
   RestScreen( 13, 0, 14, 3, Stuff( s, 2, 1, Chr( 30 ) ) )
   FOR i := 2 TO Len( s ) STEP 2
      s := Stuff( s, i, 1, Chr( 8 ) )
   NEXT
   RestScreen( 15, 0, 16, 3, s )
   RestScreen()
   AAdd( a, Pos() )
   @ 3, 0 SAY "kept"
   SAVE SCREEN TO cKept
   SetPos( 15, 15 )
   SAVE SCREEN
   SetPos( 2, 2 )
   CLS
   RESTORE SCREEN FROM cKept
   @ 3, 10 SAY "then"
   RESTORE SCREEN
   AAdd( a, Pos() )
   @ 4, 0 SAY "after"
   RESTORE SCREEN
   OutStd( "disp", DispCount(), DispBegin(), DispCount(), DispBegin(), DispCount(), DispEnd(), DispEnd(), DispEnd(), ;
      DispCount(), Chr( 10 ) )
   OutStd( "cursor", SetCursor(), SetCursor( SC_NONE ), SetCursor(), SetCursor( 9 ), SetCursor( SC_INSERT ), ;
      SetCursor(), Chr( 10 ) )
   SET CURSOR ON
   OutStd( "set cursor on", SetCursor(), Chr( 10 ) )
   SET CURSOR OFF
   OutStd( "set cursor off", SetCursor(), Chr( 10 ) )
   SET CURSOR ( .T. )
   OutStd( "set cursor (.T.)", SetCursor(), Chr( 10 ) )
   @ 20, 0 SAY 3.14159 PICTURE "99.99"
   @ 21, 0 SAY "abc" PICTURE "@!" COLOR "W+/B"
   @ 21, 10 SAY 2.5 COLOR "R/W" PICTURE "9.99"
   SetPos( 22, 0 )
   DevOutPict( 1234.5, "@E 9,999.99" )
   DevOutPict( Date() - Date(), "999", "GR/N" )
   AAdd( a, Pos() )
   OutStd( "positions:" )
   AEval( a, {| p | OutStd( " " + p ) } )
   OutStd( Chr( 10 ) )
RETURN

STATIC FUNCTION Pos()
RETURN LTrim( Str( Row() ) ) + "," + LTrim( Str( Col() ) )
