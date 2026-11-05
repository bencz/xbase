// An element passed by reference must exist: the error comes at the call (see errors_byref_bound.err).
PROCEDURE Main()
   LOCAL a := { 1 }
   ? "start"
   SetTo( @a[ 3 ], 9 )
   ? "not reached"
   RETURN

PROCEDURE SetTo( v, n )
   ? "not called"
   v := n
   RETURN
