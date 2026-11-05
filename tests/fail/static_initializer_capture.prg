PROCEDURE Main()
   LOCAL x := 5
   STATIC b := { || x }
   ? Eval( b )
   RETURN
