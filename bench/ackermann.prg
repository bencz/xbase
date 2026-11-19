// Deep recursion: Ackermann's function A(3, 9) (11 million calls, 4093 levels deep), 3 times.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), rep, ok := .T.
   FOR rep := 1 TO 3
      ok := ok .AND. Ack( 3, 9 ) == 4093
   NEXT
   ? "ackermann", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN

STATIC FUNCTION Ack( m, n )
   IF m == 0
      RETURN n + 1
   ELSEIF n == 0
      RETURN Ack( m - 1, 1 )
   ENDIF
   RETURN Ack( m - 1, Ack( m, n - 1 ) )
