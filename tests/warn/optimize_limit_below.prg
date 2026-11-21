// Half the statements of optimize_limit: below kLargeFunction once lowered: no warning.
#define S4( x ) x ; x ; x ; x
#define S8( x ) S4( x ) ; S4( x )
#define S16( x ) S4( S4( x ) )
PROCEDURE Main()
   ? Big( 3 )
RETURN

STATIC FUNCTION Big( n )
   LOCAL a := Array( 10 ), i := 1
   AFill( a, 0 )
   S8( S16( a[ i ] += n ) )
RETURN a[ 1 ]
