// Int( x / y ), lowered as one operation when both are integers: the truncated quotient of each sign, the display
// width it gets (10, or 20 past 9999999999 or -999999999; a quotient on either bound computed as the double, which
// may lie past it), operands on and past -2^52 and 2^52 (beyond them the double's rounding: as the division then
// Int() give them), a divisor of -1, a zero divisor (the division's error),
// and doubles, strings and NIL as operands, read from an array (not known when compiling), from typed loop locals and
// from a double a loop carries.
PROCEDURE Main()
   LOCAL aCase := { { 7, 2 }, { -7, 2 }, { 7, -2 }, { -7, -2 }, { 0, 5 }, { 15, 16 }, { -15, 16 }, { 16, 16 }, ;
                    { 5, -1 }, { 99999999990, 10 }, { 99999999999, 10 }, { -99999999999, 10 }, ;
                    { 100000000000, 10 }, { -100000000000, 10 }, { 199999999980, 10 }, ;
                    { -9999999990, 10 }, { -9999999995, 10 }, { -999999999, 1 }, { -1000000000, 1 }, ;
                    { -1999999998, 2 }, { -1999999999, 2 }, { -2000000000, 2 }, { -2000000001, 2 }, ;
                    { 9999999999, 1 }, { 10000000000, 1 }, { 4503599627370495, 2 }, { -4503599627370496, 3 }, ;
                    { -4503599627370497, 3 }, { 7, -4503599627370496 }, { -4503599627370496, -4503599627370496 }, ;
                    { 4503599627370496, 4503599627370496 }, { 4503599627370495, -1 }, { -9223372036854775807, -1 }, ;
                    { 4503599627370495, 1 }, { 4503599627370496, 3 }, { 9007199254740993, 1 }, ;
                    { 9223372036854775807, 2 }, { -9223372036854775807, 1 }, { 3, 4503599627370497 }, ;
                    { 7.5, 2 }, { 7, 2.5 }, { -7.5, 2.5 }, { 7, 0 }, { 7.5, 0 }, { "x", 2 }, { 7, NIL } }
   LOCAL c, r, i, n := 0, a := 1000, b := 3, d := 7.5
   FOR EACH c IN aCase
      ? Shown( c[ 1 ] ), "/", Shown( c[ 2 ] ), "->", Quotient( c[ 1 ], c[ 2 ] )
   NEXT
   FOR i := 1 TO 40
      n += Int( a / b )
      a := Int( a / 2 ) + i
      b := Int( -b / 2 ) + 7
   NEXT
   ? "loop:", n, a, b
   n := 0
   FOR i := 1 TO 6
      n += Int( d / 2 )
      d := d * 3 + 0.25
   NEXT
   ? "double:", n, d
RETURN

STATIC FUNCTION Quotient( x, y )
   LOCAL r, e
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      r := Int( x / y )
   RECOVER USING e
      RETURN "error " + LTrim( Str( e:subCode ) ) + " " + e:operation
   END SEQUENCE
RETURN ValType( r ) + " [" + Str( r ) + "] " + LTrim( Str( Len( Str( r ) ) ) )

STATIC FUNCTION Shown( x )
RETURN iif( ValType( x ) == "N", LTrim( Str( x ) ), ValType( x ) )
