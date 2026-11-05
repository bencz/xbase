// Round() against Clipper results, including decimal halves stored just below .5.
PROCEDURE Main()
   LOCAL i, v, n := 0, a := { 0.005, 0.015, 0.025, 0.035, 0.045, 0.055, 0.065, 0.075, 0.085, 0.095, 1.005, 1.015, 1.025, 2.675, 8.345, 1.115, 10.235, 100.785, 1234.5675 }
   FOR i := 1 TO 400
      v := i / 1000 + 0.0005
      ?? Str( Round( v, 3 ), 8, 3 )
      ?? Str( Round( i * 1.1 / 200, 2 ), 8, 2 )
      ?? Str( Round( -i / 8 + 0.0625, 3 ), 10, 3 )
      ?? Str( Round( i * 0.35, 0 ) )
   NEXT
   FOR EACH v IN a
      ? Round( v, 2 ), Round( v, 3 ), Round( -v, 2 ), Round( v * 1000, -1 )
   NEXT
RETURN
