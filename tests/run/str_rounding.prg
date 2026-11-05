// Numbers shown with fewer decimals than they have round half up on the first digit not shown; a value whose first
// significant digit lies further right shows as 0 (with no minus sign).
PROCEDURE Main()
   LOCAL n := 1000000.25
   ? Str( 0.0009, 10, 2 ), Str( 0.006, 10, 0 ), Str( -0.0007, 10, 2 ), Str( 0.005, 10, 2 ), Str( 0.0049, 10, 2 )
   ? Str( 0.009, 10, 2 ), Str( 0.5, 10, 0 ), Str( -0.5, 10, 0 ), Str( 2.675, 10, 2 ), Str( 1.995, 10, 2 )
   ? Str( 123.456, 10, 1 ), Str( 99.95, 10, 1 ), Str( 0.00049, 10, 3 ), Str( 0.0005, 10, 3 )
   ? 1 / n, 1 / 3, 2 / 3
   RETURN
