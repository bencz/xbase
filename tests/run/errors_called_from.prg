// The default error block shows the call stack after the error.
PROCEDURE Main()
   ? "start"
   F( 1 )
RETURN

STATIC PROCEDURE F( n )
   ? SubStr( n, 1 )
RETURN
