// A name cannot be a LOCAL and a memory variable of the same function.
PROCEDURE Main()
   LOCAL x := 1
   PRIVATE x := 2
   ? x
RETURN
