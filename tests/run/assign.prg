// The value of an assignment is computed completely before it is stored.
PROCEDURE Main()
   LOCAL x := 5, y := 7, z := NIL
   x := x++
   y := IIf( .T., y++, 0 )
   ? x, y
   z := 5
   z := QOut( "called" )
   ? ValType( z ), z
   IF( x == 5, QOut( "inline if as statement" ), QOut( "wrong" ) )
   RETURN
