// A program of two modules: this one declares a function of the other (tests/run/modules.d) with EXTERN FUNCTION.
// LOCAL a[n] creates the array itself, even though this module defines its own Array() (the expected output is that
// of the program without it).
EXTERN FUNCTION Helper( nValue )
EXTERN FUNCTION Counter()

PROCEDURE Main()
   LOCAL a[ 3 ]
   ? Helper( 2 ), Counter(), Counter()
   ? ValType( a ), Len( a )
   RETURN

FUNCTION Array( n )
   RETURN "own Array" + Str( n, 2 )
