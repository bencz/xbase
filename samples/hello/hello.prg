// Greets whoever its arguments name, or the world.
PROCEDURE Main( ... )
   LOCAL aNames := { ... }, cName
   IF Empty( aNames )
      ? "Hello, world!"
   ENDIF
   FOR EACH cName IN aNames
      ? "Hello, " + cName + "!"
   NEXT
RETURN
