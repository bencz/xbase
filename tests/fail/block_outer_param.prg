PROCEDURE Main()
   LOCAL n := 1
   ? Eval( {| x | Eval( {| y | x * y }, 3 ) }, 2 )
   RETURN
