// RETURN cannot leave the body of a BEGIN SEQUENCE (Clipper rule).
PROCEDURE Main()
   BEGIN SEQUENCE
      RETURN
   END
   RETURN
