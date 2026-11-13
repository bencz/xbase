// A macro call is not a variable: it cannot be passed by reference.
MEMVAR c
PROCEDURE Main()
   c := "Upper"
   QOut( @&c( "x" ) )
RETURN
