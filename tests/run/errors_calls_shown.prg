// An error 150 calls deep: the default handler lists the innermost 100 levels of the call stack, then says that
// there are more, and ends the program with exit code 1.
PROCEDURE Main()
   ? "before"
   Deep( 150 )
RETURN

FUNCTION Deep( n )
   IF n == 0
      RETURN 1 + "a"
   ENDIF
RETURN Deep( n - 1 )
