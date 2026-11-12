// The stack error whose handler runs out of the reserve left for it: nothing more can run, the process ends with
// "xbrt: stack overflow" and exit code 1.
PROCEDURE Main()
   ? "before"
   ErrorBlock( {| e | Handle( e ) } )
   Down( 1 )
   ? "never"
RETURN

FUNCTION Down( n )
RETURN Down( n + 1 )

FUNCTION Handle( e )
RETURN Handle( e )
