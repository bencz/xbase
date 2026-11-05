// Recursive calls: Fib( 37 ).
PROCEDURE Main()
   LOCAL t := ElapsedMs(), r := Fib( 37 )
   ? "fib", ElapsedMs() - t, IIf( r == 24157817, "ok", "WRONG" )
   RETURN

FUNCTION Fib( n )
   IF n < 2
      RETURN n
   ENDIF
   RETURN Fib( n - 1 ) + Fib( n - 2 )
