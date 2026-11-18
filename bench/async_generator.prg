// Asynchronous generators: a pipeline of three (a source of 1,000,000 numbers that yields the thread every 1000, a
// filter of the even ones, a map that doubles them) consumed by AWAIT FOR EACH: most steps go on at once, some
// suspend and resume.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), x, s := 0
   AWAIT FOR EACH x IN Doubled( Evens( Source( 1000000 ) ) )
      s += x
   NEXT
   ? "async_generator", ElapsedMs() - t, IIf( s == 500001000000, "ok", "WRONG" )
RETURN

ASYNC FUNCTION Source( n )
   LOCAL k
   FOR k := 1 TO n
      IF k % 1000 == 0
         AWAIT TaskYield()
      ENDIF
      YIELD k
   NEXT
RETURN

ASYNC FUNCTION Evens( g )
   LOCAL x
   AWAIT FOR EACH x IN g
      IF x % 2 == 0
         YIELD x
      ENDIF
   NEXT
RETURN

ASYNC FUNCTION Doubled( g )
   LOCAL x
   AWAIT FOR EACH x IN g
      YIELD x * 2
   NEXT
RETURN
