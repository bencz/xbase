// Console output, over the __ConsoleWrite() primitive (rtl/console.c).

// QOut( [xValues...] ): a new line, then the values separated by spaces.
FUNCTION QOut( ... )
   __ConsoleWrite( Chr( 10 ) )
   QQOut( ... )
RETURN NIL

// QQOut( [xValues...] ): the values separated by spaces, on the current line.
FUNCTION QQOut( ... )
   LOCAL i
   FOR i := 1 TO PCount()
      IF i > 1
         __ConsoleWrite( " " )
      ENDIF
      __ConsoleWrite( PValue( i ) )
   NEXT
RETURN NIL
