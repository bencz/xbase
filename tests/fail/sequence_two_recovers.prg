// A BEGIN SEQUENCE has one RECOVER.
PROCEDURE Main()
   BEGIN SEQUENCE
      ? 1
   RECOVER
      ? 2
   RECOVER
      ? 3
   END
   RETURN
