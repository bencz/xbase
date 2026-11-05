// Seconds() (since midnight, SET DECIMALS decimals), Time() ("hh:mm:ss"), ElapsedMs() (milliseconds since the
// program started, never going back): their kinds, ranges and formats.
PROCEDURE Main()
   LOCAL s := Seconds(), t := Time(), e1 := ElapsedMs(), e2, i, n := 0
   ? "seconds", ValType( s ), s >= 0, s < 86400, Len( Str( s ) )
   ? "time", ValType( t ), Len( t ), SubStr( t, 3, 1 ) == ":", SubStr( t, 6, 1 ) == ":", ;
      Val( Left( t, 2 ) ) < 24, Val( SubStr( t, 4, 2 ) ) < 60, Val( Right( t, 2 ) ) <= 60
   FOR i := 1 TO 200000
      n += i
   NEXT
   e2 := ElapsedMs()
   ? "elapsed", ValType( e1 ), e1 >= 0, e2 >= e1, e2 - e1 < 60000, n
   ? "abbreviated", ValType( Seco() )
   RETURN
