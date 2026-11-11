// YIELD cannot suspend inside BEGIN SEQUENCE (its error block and handlers belong to one step).
FUNCTION Main()
   BEGIN SEQUENCE
      YIELD 1
   END SEQUENCE
RETURN
