// Console and standard stream output, over the __ConsoleWrite() and __StdWrite() primitives (rtl/console.c), each
// call one write that no other thread's output splits.

// QOut( [xValues...] ): a new line, then the values separated by spaces.
FUNCTION QOut( ... )
   __ConsoleWrite( .T., ... )
RETURN NIL

// QQOut( [xValues...] ): the values separated by spaces, on the current line.
FUNCTION QQOut( ... )
   __ConsoleWrite( .F., ... )
RETURN NIL

// OutStd( [xValues...] ): the values separated by spaces, to the standard output (not the console).
FUNCTION OutStd( ... )
   __StdWrite( 1, ... )
RETURN NIL

// OutErr( [xValues...] ): the values separated by spaces, to the standard error.
FUNCTION OutErr( ... )
   __StdWrite( 2, ... )
RETURN NIL
