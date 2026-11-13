// Number constants met once calls are inlined: + - * and unary - fold as the run-time operators compute them (an
// integer computed at run time has width 0, a product's decimals the sum of its operands'), comparisons fold to
// logicals and their branches go; / and % (SET DECIMALS at run time) and an overflowing product stay.
FUNCTION Main()
   RETURN { Mul( 2.01, 1 ), Mul( 3, 4 ), -Mul( 1.5, 2 ), Mul( 9223372036854775807, 2 ), Mul( 7, 2 ) % 4, ;
      iif( Mul( 3, 1 ) <= 1, "le", "gt" ) }

STATIC FUNCTION Mul( a, b )
RETURN a * b
