// BEGIN SEQUENCE: the body's blocks unwind to a handler that takes a BREAK
// (seq.catch), stores its value (seq.value) and goes on with RECOVER. A
// variable the handler does not read before writing it (x: RECOVER USING x)
// becomes an SSA value; one it reads (Kept's t, read after the sequence) does
// too, but keeps its stores and is loaded where the handler starts.
FUNCTION Main( n )
   LOCAL x := 1
   BEGIN SEQUENCE
      x := 10 / n
   RECOVER USING x
      x := -x
   END
   RETURN x

FUNCTION Kept( n )
   LOCAL t := 0, i
   BEGIN SEQUENCE
      FOR i := 1 TO n
         t += 10 / i
      NEXT
   END
   RETURN t
