// An error that allows nothing (a bound error) whose error block returns ends the program.
PROCEDURE Main()
   LOCAL a := {}
   ErrorBlock( {| e | 1 } )
   ? "start"
   ? a[ 3 ]
   ? "not reached"
RETURN
