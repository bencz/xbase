// Past 16 significant digits a number shows its first 16 exact digits rounded half up on the next one, as Clipper:
// exact ties (a half after 16 integer digits, .25 and .5625 cut at the 16th digit, 10^15 + 0.5, 9999999999999999.5
// carrying into a 17th digit), values just past and short of a tie, negative ones, integers above 2^53 (their exact
// digits: 2^60 + 512 is 1152921504606847488), and the same through Transform(), values computed at run time and
// read from macros.
PROCEDURE Main()
   LOCAL x

   ? Str( 1286742750677284.5, 20, 2 ), Str( -1538125745624864.5, 22, 4 )
   ? Str( 349057097527390.25, 30, 6 ), Str( -6156500468247.5625, 30, 13 )
   ? Str( 10 ^ 15 + 0.5, 22, 2 ), Str( 9999999999999999.5, 22, 1 )
   ? Str( 1286742750677284.25, 22, 2 ), Str( 1286742750677283.5, 22, 2 ), Str( 1286742750677284.75, 22, 2 )
   ? Str( 2 ^ 60 + 512, 25 ), Str( 2 ^ 70, 25 )
   ? Transform( 1286742750677284.5, "99999999999999999.99" )
   ? Transform( -349057097527390.25, "@( 9999999999999999.999999" )
   x := 1286742750677284 + Val( "0.5" )
   ? "computed:", Str( x, 20, 1 ), Str( x * 2 + 1, 20 ), Str( -x, 21, 3 )
   ? "macro:", &( "Str( 1286742750677284.5, 20, 2 )" ), &( "Str( 10 ^ 15 + 0.5, 22, 2 )" )
RETURN
