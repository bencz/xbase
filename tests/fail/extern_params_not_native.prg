// PARAMS (and ARGS) read the caller's frame: only a NATIVE function can.
EXTERN FUNCTION MyArgs( n ) PARAMS

PROCEDURE Main()
   ? MyArgs( 1 )
   RETURN
