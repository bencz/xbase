// An error block that returns without recovering from an error that takes no substitute, 150 calls deep: the
// unrecoverable error lists the innermost 100 levels of the call stack, then says that there are more.
PROCEDURE Main()
   ? "before"
   ErrorBlock( {|| NIL } )
   Deep( 150 )
RETURN

FUNCTION Deep( n )
   IF n == 0
      RETURN Do( "NOSUCHFUNCTION" )
   ENDIF
RETURN Deep( n - 1 )
