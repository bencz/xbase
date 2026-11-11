// Generators: FOR EACH over a function that yields 3,000,000 numbers (one step of the state machine each), and a
// second one that filters the first.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), x, s := 0
   FOR EACH x IN Evens( Range( 1, 3000000 ) )
      s += x
   NEXT
   ? "generator", ElapsedMs() - t, IIf( s == 2250001500000, "ok", "WRONG" )
   RETURN

FUNCTION Range( a, b )
   LOCAL i
   FOR i := a TO b
      YIELD i
   NEXT
   RETURN

FUNCTION Evens( e )
   LOCAL x
   FOR EACH x IN e
      IF x % 2 == 0
         YIELD x
      ENDIF
   NEXT
   RETURN
