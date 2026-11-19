// Recursive calls with three arguments: the Takeuchi function Tak(24, 16, 8), 10 times.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), rep, ok := .T.
   FOR rep := 1 TO 10
      ok := ok .AND. Tak( 24, 16, 8 ) == 9
   NEXT
   ? "tak", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN

STATIC FUNCTION Tak( x, y, z )
   IF y < x
      RETURN Tak( Tak( x - 1, y, z ), Tak( y - 1, z, x ), Tak( z - 1, x, y ) )
   ENDIF
   RETURN z
