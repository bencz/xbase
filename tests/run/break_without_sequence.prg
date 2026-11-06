// A BREAK with no BEGIN SEQUENCE active ends the program quietly.
PROCEDURE Main()
   ? "start"
   F()
   ? "not reached"
RETURN

PROCEDURE F()
   BREAK "nobody"
RETURN
