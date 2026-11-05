// The same through a parameter passed by reference whose element is gone.
PROCEDURE Main()
   LOCAL a := { 1, 2, 3 }
   ? "start"
   Change( @a[ 3 ], a )
   RETURN

PROCEDURE Change( v, arr )
   ASize( arr, 0 )
   v := "x" + 1
   RETURN
