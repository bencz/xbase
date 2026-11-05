// SET commands come from std.ch and call Set(); folded and run-time
// divisions both use the current SET DECIMALS.
PROCEDURE Main()
   LOCAL a := "abc", n10 := 10, old
   ? 10 / 4, n10 / 4, a = "ab"
   SET DECIMALS TO 4
   SET EXACT ON
   ? 10 / 4, n10 / 4, a = "ab", a = "abc  "
   old := Set( _SET_DECIMALS, 1 )
   ? old, Set( _SET_DECIMALS ), n10 / 3
   SET DECIMALS TO
   ? n10 / 4
   SET EXACT OFF
   ? a = "ab", Set( _SET_EXACT, "on" ), Set( _SET_EXACT )
   ? Set( _SET_DECIMALS, 70000 ), Set( _SET_DECIMALS ), Set( _SET_DECIMALS, "5" ), Set( _SET_DECIMALS, 2 )
   RETURN
