// A program of two modules compiled separately: this one calls the functions of the other (tests/run/modules.d), one
// declared with EXTERN FUNCTION (its argument count checked), one not (found when linking, as in Clipper).
// LOCAL a[n] creates the array itself, even though this module defines its own Array() (the expected output is that
// of the program without it).
EXTERN FUNCTION Helper( nValue )

PROCEDURE Main()
   LOCAL a[ 3 ]
   ? Helper( 2 ), Counter(), Counter()
   ? ValType( a ), Len( a )
   RETURN

FUNCTION Array( n )
   RETURN "own Array" + Str( n, 2 )
