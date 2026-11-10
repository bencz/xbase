// A file-wide MEMVAR and STATIC cannot name the same variable.
STATIC y := 1
MEMVAR y
PROCEDURE Main()
   ? y
RETURN
