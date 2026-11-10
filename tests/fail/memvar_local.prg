// A function's MEMVAR and LOCAL cannot name the same variable.
PROCEDURE Main()
   MEMVAR x
   LOCAL x := 1
   ? x
RETURN
