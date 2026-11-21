// A function whose optimization makes the compilation slow (256 reads and writes of an element, past kLargeFunction
// once lowered) is optimized as any other, with a warning that names it where it starts.
#define S4( x ) x ; x ; x ; x
#define S16( x ) S4( S4( x ) )
PROCEDURE Main()
   ? Big( 3 )
RETURN

STATIC FUNCTION Big( n )
   LOCAL a := Array( 10 ), i := 1
   AFill( a, 0 )
   S16( S16( a[ i ] += n ) )
RETURN a[ 1 ]
