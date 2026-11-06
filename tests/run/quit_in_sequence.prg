// QUIT goes through BEGIN SEQUENCE: RECOVER takes only a BREAK.
PROCEDURE Main()
   ? "start"
   BEGIN SEQUENCE
      QUIT
   RECOVER
      ? "not here: QUIT is not a BREAK"
   END
   ? "not reached"
RETURN
