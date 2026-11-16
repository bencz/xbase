// Arguments that start with // are runtime options (//WORKERS:n, any other ignored), which the program does not
// receive; the others keep their order.
PROCEDURE Main( ... )
   LOCAL i
   ? PCount()
   FOR i := 1 TO PCount()
      ?? " [" + PValue( i ) + "]"
   NEXT
   ? TaskRun( {|| "tasks run" } ):Result
RETURN
