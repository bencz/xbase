// YIELD cannot suspend inside a BEGIN SEQUENCE with ALWAYS: a generator left before its end would never run it.
FUNCTION Main()
   BEGIN SEQUENCE
      YIELD 1
   ALWAYS
      ? "always"
   END SEQUENCE
RETURN
