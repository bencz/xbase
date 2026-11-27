// Screen functions written over the screen primitives (rtl/screen.c; docs/terminal.md).

// What SAVE SCREEN kept for RESTORE SCREEN: the screen and the cursor's row and column (NIL: nothing kept). The
// program's one place, which every thread shares.
STATIC s_aKept

// DevOutPict( xValue, cPicture, [cColor] ): the value as the picture formats it (Transform()), as DevOut() shows it.
FUNCTION DevOutPict( xValue, cPicture, cColor )
   DevOut( Transform( xValue, cPicture ), cColor )
RETURN NIL

// __ScreenKeep(), __ScreenBack(): SAVE SCREEN keeps the whole screen and the cursor, RESTORE SCREEN brings them back,
// once.
FUNCTION __ScreenKeep()
   s_aKept := { SaveScreen( 0, 0, MaxRow(), MaxCol() ), Row(), Col() }
RETURN NIL

FUNCTION __ScreenBack()
   LOCAL aKept := s_aKept
   IF aKept != NIL
      s_aKept := NIL
      RestScreen( 0, 0, MaxRow(), MaxCol(), aKept[ 1 ] )
      SetPos( aKept[ 2 ], aKept[ 3 ] )
   ENDIF
RETURN NIL
