// PARAMS is part of a declaration: one without it conflicts with the runtime's.
EXTERN FUNCTION PValue( nArg, [xNew] ) NATIVE

PROCEDURE Main()
   ? PValue( 1 )
   RETURN
