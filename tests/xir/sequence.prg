// BEGIN SEQUENCE: the body's blocks unwind to a handler that takes a BREAK
// (seq.catch), stores its value (seq.value) and goes on with RECOVER; the
// variables stay in memory (no promotion in a function with a sequence).
FUNCTION Main( n )
   LOCAL x := 1
   BEGIN SEQUENCE
      x := 10 / n
   RECOVER USING x
      x := -x
   END
   RETURN x
