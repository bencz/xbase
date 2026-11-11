// A macro cannot be passed by reference.
MEMVAR c
PROCEDURE Main()
   c := "x"
   QOut( @&c )
RETURN
