// ALWAYS runs while a request may wait to go on: RETURN cannot leave it.
FUNCTION Main()
   BEGIN SEQUENCE
      ? 1
   ALWAYS
      RETURN 2
   END SEQUENCE
RETURN 3
