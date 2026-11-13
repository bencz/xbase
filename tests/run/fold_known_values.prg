// Arithmetic and comparisons of values the compiler knows only once calls are inlined or LOCALs become values
// (function parameters given literals, LOCALs assigned constants, #define constants used through them): the results,
// folded, are those of the run-time operators, with their display widths and decimals (an integer computed at run
// time has no width of its own, a double's decimals follow the operator), integer overflow into a double, / and %
// (SET DECIMALS at run time: left to it), unary minus, the bit operators (xbase's own: Harbour has none) and the
// comparisons of integers with doubles.
#define BIG 9223372036854775807

PROCEDURE Main()
   LOCAL n := 3, d := 2.01, k := 1, big := BIG, q
   ? Mul( 2.01, 1 ), Mul( 2, 1 ), Mul( 1.5, 1.25 ), Add( 1.5, 2.25 ), Add( 1, 0.001 ), Sub( 10, 0.5 )
   ? d * k, n * n, n + d, n - d, d * d, -d, n * 1000000000
   ? Str( Mul( 2.01, 1 ) ), Len( Str( Mul( 2, 1 ) ) ), Len( Str( n * n ) ), Str( Add( 1.5, 2.25 ), 10, 3 )
   ? Add( big, 1 ), Mul( big, 2 ), Sub( -big, 2 ), big + 1 == big + 2
   ? Div( 10, 4 ), Div( 10, 5 ), Div( 1, 3 ), Mod( 10, 3 ), Mod( -7, 2 ), Mod( 7.5, 2 ), Mod( 7, -1 )
   ? Less( 3, 1 ), Less( 1, 3 ), Less( 2, 2.0 ), Same( 2, 2.0 ), Same( 0.1 + 0.2, 0.3 ), Same( 1.0, 1 )
   ? n <= 1, n >= 3, d > 2, d == 2.01, n != 3, n * 2 == 6, d * 100 == 201
   q := iif( Less( n, 5 ), "small", "large" )
   ? q, Pick( 1 ), Pick( 2 ), Pick( 0 )
   ? Neg( 5 ), Neg( 2.50 ), Neg( -BIG - 1 ), Neg( Neg( 7 ) ) + 1, Len( Str( Neg( 3 ) ) ), Neg( 0.001 )
   ? Bits( 12, 10 ), Bits( -1, 3 ), Bits( 2.9, 1 ), Bits( BIG, 62 )
   ? ValType( Mul( 2, 3 ) ), ValType( Div( 1, 3 ) ), Mul( 2, 3 ) / 4, Int( Mul( 2.5, 2 ) )
RETURN

FUNCTION Mul( a, b )
RETURN a * b

FUNCTION Add( a, b )
RETURN a + b

FUNCTION Sub( a, b )
RETURN a - b

FUNCTION Div( a, b )
RETURN a / b

FUNCTION Mod( a, b )
RETURN a % b

FUNCTION Less( a, b )
RETURN a < b

FUNCTION Same( a, b )
RETURN a == b

FUNCTION Pick( k )
   IF k * 2 > 3
      RETURN "two"
   ELSEIF k + 1 >= 2
      RETURN "one"
   ENDIF
RETURN "none"

FUNCTION Neg( a )
RETURN -a

FUNCTION Bits( a, b )
RETURN Str( a & b, 20 ) + Str( a | b, 20 ) + Str( a ^^ b, 20 ) + Str( a << 1, 21 ) + Str( a >> b, 20 ) + Str( ~a, 21 )
