// The stack error that nothing recovers from: ErrorSys reports it and the program ends with exit code 1.
PROCEDURE Main()
   ? "before"
   Down( 1 )
   ? "never"
RETURN

FUNCTION Down( n )
RETURN Down( n + 1 )
