// A message written with arguments is a method call, not an instance variable: it cannot be assigned.
PROCEDURE Main()
   LOCAL o := ErrorNew()
   o:cargo() := 3
   RETURN
