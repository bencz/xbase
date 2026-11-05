// A STATIC codeblock calling itself, a variable named EACH, an empty SWITCH, Val() with text before the dot.
STATIC s_fact := { | n | iif( n <= 1, 1, n * Eval( s_fact, n - 1 ) ) }
PROCEDURE Main()
   LOCAL each, a := { 1, 2 }
   FOR each := 1 TO 2
      ?? each, ""
   NEXT
   ? Eval( s_fact, 5 )
   SWITCH NIL
   ENDSWITCH
   SWITCH Date()
   ENDSWITCH
   ? Val( "1x.25" ), Len( Str( Val( "1x.25" ) ) ), Val( "12abc.5" ), Val( "1.2x5" ), Val( "x.5" ), Val( "-1x.25" )
   FOR EACH each IN a
      ?? each, ""
   NEXT
RETURN
