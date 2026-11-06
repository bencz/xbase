// The default error block ends the program: an error in a sequence reaches RECOVER only through a BREAK.
PROCEDURE Main()
   LOCAL x
   ? "start"
   BEGIN SEQUENCE
      x := "a" + 1
   RECOVER
      ? "not here: the default error block does not BREAK"
   END
   ? "not reached"
RETURN
