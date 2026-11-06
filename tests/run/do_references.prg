// Do() passes a reference on only where the function takes one: a runtime
// function receives the value; a name ends at a NUL. ErrorLevel() keeps
// numbers wrapped to 32 bits, and anything else as 0.
PROCEDURE Main()
   LOCAL s := "abc", a := { 1 }
   ? Do( "Upper", @s ), Do( "AAdd", @a, 2 ), Len( a ), Do( "Len", @s )
   ? Do( "upper" + Chr( 0 ) + "x", "nul" )
   ErrorLevel( 7 )
   ? ErrorLevel( "x" ), ErrorLevel( 2 ^ 33 ), ErrorLevel( NIL ), ErrorLevel()
RETURN
