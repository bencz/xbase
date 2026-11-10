// Console and standard stream output, over the __ConsoleWrite() and __StdWrite() primitives (rtl/console.c).

// QOut( [xValues...] ): a new line, then the values separated by spaces.
FUNCTION QOut( ... )
   __ConsoleWrite( Chr( 10 ) )
   QQOut( ... )
RETURN NIL

// QQOut( [xValues...] ): the values separated by spaces, on the current line. Its own loop, not StdList()'s: passing
// the values on through one more call costs QOut() 13% more cycles.
FUNCTION QQOut( ... )
   LOCAL i
   FOR i := 1 TO PCount()
      IF i > 1
         __ConsoleWrite( " " )
      ENDIF
      __ConsoleWrite( PValue( i ) )
   NEXT
RETURN NIL

// OutStd( [xValues...] ): the values separated by spaces, to the standard output (not the console).
FUNCTION OutStd( ... )
   StdList( 1, ... )
RETURN NIL

// OutErr( [xValues...] ): the values separated by spaces, to the standard error.
FUNCTION OutErr( ... )
   StdList( 2, ... )
RETURN NIL

// The values separated by spaces, to standard stream nHandle (1 output, 2 error).
STATIC PROCEDURE StdList( nHandle, ... )
   LOCAL i
   FOR i := 2 TO PCount()
      IF i > 2
         __StdWrite( nHandle, " " )
      ENDIF
      __StdWrite( nHandle, PValue( i ) )
   NEXT
RETURN
