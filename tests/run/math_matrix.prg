// The number functions over a list of numbers (signs, halves just below a step, the display widths' limits, past
// 2^52, a sum with a rounding error): Int, Abs, Sqrt, Exp, Log (domain errors and infinities shown as their stars),
// Round with every place from -2 to 3, Mod with positive, negative, fractional and zero divisors, Max and Min of
// pairs; with literal arguments and values made at run time. Each result shows with Str() as its own width and
// decimals give it, and Str( x, 25, 6 ). As the reference, but past 2^52: Int, Abs and Mod keep a number exact
// there (the reference loses its last digits), Round leaves a number whose scaled value reaches 2^52 as it is (the
// reference moves 4503599627370497 to ...499), and a folded literal product has the default width.
PROCEDURE Main()
   LOCAL aVal := { 0, 1, -1, 0.5, -0.5, 1.5, -1.5, 2.5, ;
      -2.5, 2.675, -2.675, 0.125, 7, -7, 123.456, ;
      1000000000, -9999999999, 0.1 + 0.2, 100, 4503599627370497 * 1.0 }
   LOCAL x, y, n
   ? "literal"
   ? Shown( Int( 0 ) ), Shown( Abs( 0 ) ), ;
      Shown( Sqrt( Abs( 0 ) ) ), Shown( Exp( Min( 0, 50 ) ) )
   ?? "", Shown( Log( Abs( 0 ) ) )
   ?? "", Shown( Round( 0, -2 ) )
   ?? "", Shown( Round( 0, -1 ) )
   ?? "", Shown( Round( 0, 0 ) )
   ?? "", Shown( Round( 0, 1 ) )
   ?? "", Shown( Round( 0, 2 ) )
   ?? "", Shown( Round( 0, 3 ) )
   ? Shown( Int( 1 ) ), Shown( Abs( 1 ) ), ;
      Shown( Sqrt( Abs( 1 ) ) ), Shown( Exp( Min( 1, 50 ) ) )
   ?? "", Shown( Log( Abs( 1 ) ) )
   ?? "", Shown( Round( 1, -2 ) )
   ?? "", Shown( Round( 1, -1 ) )
   ?? "", Shown( Round( 1, 0 ) )
   ?? "", Shown( Round( 1, 1 ) )
   ?? "", Shown( Round( 1, 2 ) )
   ?? "", Shown( Round( 1, 3 ) )
   ? Shown( Int( -1 ) ), Shown( Abs( -1 ) ), ;
      Shown( Sqrt( Abs( -1 ) ) ), Shown( Exp( Min( -1, 50 ) ) )
   ?? "", Shown( Log( Abs( -1 ) ) )
   ?? "", Shown( Round( -1, -2 ) )
   ?? "", Shown( Round( -1, -1 ) )
   ?? "", Shown( Round( -1, 0 ) )
   ?? "", Shown( Round( -1, 1 ) )
   ?? "", Shown( Round( -1, 2 ) )
   ?? "", Shown( Round( -1, 3 ) )
   ? Shown( Int( 0.5 ) ), Shown( Abs( 0.5 ) ), ;
      Shown( Sqrt( Abs( 0.5 ) ) ), Shown( Exp( Min( 0.5, 50 ) ) )
   ?? "", Shown( Log( Abs( 0.5 ) ) )
   ?? "", Shown( Round( 0.5, -2 ) )
   ?? "", Shown( Round( 0.5, -1 ) )
   ?? "", Shown( Round( 0.5, 0 ) )
   ?? "", Shown( Round( 0.5, 1 ) )
   ?? "", Shown( Round( 0.5, 2 ) )
   ?? "", Shown( Round( 0.5, 3 ) )
   ? Shown( Int( -0.5 ) ), Shown( Abs( -0.5 ) ), ;
      Shown( Sqrt( Abs( -0.5 ) ) ), Shown( Exp( Min( -0.5, 50 ) ) )
   ?? "", Shown( Log( Abs( -0.5 ) ) )
   ?? "", Shown( Round( -0.5, -2 ) )
   ?? "", Shown( Round( -0.5, -1 ) )
   ?? "", Shown( Round( -0.5, 0 ) )
   ?? "", Shown( Round( -0.5, 1 ) )
   ?? "", Shown( Round( -0.5, 2 ) )
   ?? "", Shown( Round( -0.5, 3 ) )
   ? Shown( Int( 1.5 ) ), Shown( Abs( 1.5 ) ), ;
      Shown( Sqrt( Abs( 1.5 ) ) ), Shown( Exp( Min( 1.5, 50 ) ) )
   ?? "", Shown( Log( Abs( 1.5 ) ) )
   ?? "", Shown( Round( 1.5, -2 ) )
   ?? "", Shown( Round( 1.5, -1 ) )
   ?? "", Shown( Round( 1.5, 0 ) )
   ?? "", Shown( Round( 1.5, 1 ) )
   ?? "", Shown( Round( 1.5, 2 ) )
   ?? "", Shown( Round( 1.5, 3 ) )
   ? Shown( Int( -1.5 ) ), Shown( Abs( -1.5 ) ), ;
      Shown( Sqrt( Abs( -1.5 ) ) ), Shown( Exp( Min( -1.5, 50 ) ) )
   ?? "", Shown( Log( Abs( -1.5 ) ) )
   ?? "", Shown( Round( -1.5, -2 ) )
   ?? "", Shown( Round( -1.5, -1 ) )
   ?? "", Shown( Round( -1.5, 0 ) )
   ?? "", Shown( Round( -1.5, 1 ) )
   ?? "", Shown( Round( -1.5, 2 ) )
   ?? "", Shown( Round( -1.5, 3 ) )
   ? Shown( Int( 2.5 ) ), Shown( Abs( 2.5 ) ), ;
      Shown( Sqrt( Abs( 2.5 ) ) ), Shown( Exp( Min( 2.5, 50 ) ) )
   ?? "", Shown( Log( Abs( 2.5 ) ) )
   ?? "", Shown( Round( 2.5, -2 ) )
   ?? "", Shown( Round( 2.5, -1 ) )
   ?? "", Shown( Round( 2.5, 0 ) )
   ?? "", Shown( Round( 2.5, 1 ) )
   ?? "", Shown( Round( 2.5, 2 ) )
   ?? "", Shown( Round( 2.5, 3 ) )
   ? Shown( Int( -2.5 ) ), Shown( Abs( -2.5 ) ), ;
      Shown( Sqrt( Abs( -2.5 ) ) ), Shown( Exp( Min( -2.5, 50 ) ) )
   ?? "", Shown( Log( Abs( -2.5 ) ) )
   ?? "", Shown( Round( -2.5, -2 ) )
   ?? "", Shown( Round( -2.5, -1 ) )
   ?? "", Shown( Round( -2.5, 0 ) )
   ?? "", Shown( Round( -2.5, 1 ) )
   ?? "", Shown( Round( -2.5, 2 ) )
   ?? "", Shown( Round( -2.5, 3 ) )
   ? Shown( Int( 2.675 ) ), Shown( Abs( 2.675 ) ), ;
      Shown( Sqrt( Abs( 2.675 ) ) ), Shown( Exp( Min( 2.675, 50 ) ) )
   ?? "", Shown( Log( Abs( 2.675 ) ) )
   ?? "", Shown( Round( 2.675, -2 ) )
   ?? "", Shown( Round( 2.675, -1 ) )
   ?? "", Shown( Round( 2.675, 0 ) )
   ?? "", Shown( Round( 2.675, 1 ) )
   ?? "", Shown( Round( 2.675, 2 ) )
   ?? "", Shown( Round( 2.675, 3 ) )
   ? Shown( Int( -2.675 ) ), Shown( Abs( -2.675 ) ), ;
      Shown( Sqrt( Abs( -2.675 ) ) ), Shown( Exp( Min( -2.675, 50 ) ) )
   ?? "", Shown( Log( Abs( -2.675 ) ) )
   ?? "", Shown( Round( -2.675, -2 ) )
   ?? "", Shown( Round( -2.675, -1 ) )
   ?? "", Shown( Round( -2.675, 0 ) )
   ?? "", Shown( Round( -2.675, 1 ) )
   ?? "", Shown( Round( -2.675, 2 ) )
   ?? "", Shown( Round( -2.675, 3 ) )
   ? Shown( Int( 0.125 ) ), Shown( Abs( 0.125 ) ), ;
      Shown( Sqrt( Abs( 0.125 ) ) ), Shown( Exp( Min( 0.125, 50 ) ) )
   ?? "", Shown( Log( Abs( 0.125 ) ) )
   ?? "", Shown( Round( 0.125, -2 ) )
   ?? "", Shown( Round( 0.125, -1 ) )
   ?? "", Shown( Round( 0.125, 0 ) )
   ?? "", Shown( Round( 0.125, 1 ) )
   ?? "", Shown( Round( 0.125, 2 ) )
   ?? "", Shown( Round( 0.125, 3 ) )
   ? Shown( Int( 7 ) ), Shown( Abs( 7 ) ), ;
      Shown( Sqrt( Abs( 7 ) ) ), Shown( Exp( Min( 7, 50 ) ) )
   ?? "", Shown( Log( Abs( 7 ) ) )
   ?? "", Shown( Round( 7, -2 ) )
   ?? "", Shown( Round( 7, -1 ) )
   ?? "", Shown( Round( 7, 0 ) )
   ?? "", Shown( Round( 7, 1 ) )
   ?? "", Shown( Round( 7, 2 ) )
   ?? "", Shown( Round( 7, 3 ) )
   ? Shown( Int( -7 ) ), Shown( Abs( -7 ) ), ;
      Shown( Sqrt( Abs( -7 ) ) ), Shown( Exp( Min( -7, 50 ) ) )
   ?? "", Shown( Log( Abs( -7 ) ) )
   ?? "", Shown( Round( -7, -2 ) )
   ?? "", Shown( Round( -7, -1 ) )
   ?? "", Shown( Round( -7, 0 ) )
   ?? "", Shown( Round( -7, 1 ) )
   ?? "", Shown( Round( -7, 2 ) )
   ?? "", Shown( Round( -7, 3 ) )
   ? Shown( Int( 123.456 ) ), Shown( Abs( 123.456 ) ), ;
      Shown( Sqrt( Abs( 123.456 ) ) ), Shown( Exp( Min( 123.456, 50 ) ) )
   ?? "", Shown( Log( Abs( 123.456 ) ) )
   ?? "", Shown( Round( 123.456, -2 ) )
   ?? "", Shown( Round( 123.456, -1 ) )
   ?? "", Shown( Round( 123.456, 0 ) )
   ?? "", Shown( Round( 123.456, 1 ) )
   ?? "", Shown( Round( 123.456, 2 ) )
   ?? "", Shown( Round( 123.456, 3 ) )
   ? Shown( Int( 1000000000 ) ), Shown( Abs( 1000000000 ) ), ;
      Shown( Sqrt( Abs( 1000000000 ) ) ), Shown( Exp( Min( 1000000000, 50 ) ) )
   ?? "", Shown( Log( Abs( 1000000000 ) ) )
   ?? "", Shown( Round( 1000000000, -2 ) )
   ?? "", Shown( Round( 1000000000, -1 ) )
   ?? "", Shown( Round( 1000000000, 0 ) )
   ?? "", Shown( Round( 1000000000, 1 ) )
   ?? "", Shown( Round( 1000000000, 2 ) )
   ?? "", Shown( Round( 1000000000, 3 ) )
   ? Shown( Int( -9999999999 ) ), Shown( Abs( -9999999999 ) ), ;
      Shown( Sqrt( Abs( -9999999999 ) ) ), Shown( Exp( Min( -9999999999, 50 ) ) )
   ?? "", Shown( Log( Abs( -9999999999 ) ) )
   ?? "", Shown( Round( -9999999999, -2 ) )
   ?? "", Shown( Round( -9999999999, -1 ) )
   ?? "", Shown( Round( -9999999999, 0 ) )
   ?? "", Shown( Round( -9999999999, 1 ) )
   ?? "", Shown( Round( -9999999999, 2 ) )
   ?? "", Shown( Round( -9999999999, 3 ) )
   ? Shown( Int( 0.1 + 0.2 ) ), Shown( Abs( 0.1 + 0.2 ) ), ;
      Shown( Sqrt( Abs( 0.1 + 0.2 ) ) ), Shown( Exp( Min( 0.1 + 0.2, 50 ) ) )
   ?? "", Shown( Log( Abs( 0.1 + 0.2 ) ) )
   ?? "", Shown( Round( 0.1 + 0.2, -2 ) )
   ?? "", Shown( Round( 0.1 + 0.2, -1 ) )
   ?? "", Shown( Round( 0.1 + 0.2, 0 ) )
   ?? "", Shown( Round( 0.1 + 0.2, 1 ) )
   ?? "", Shown( Round( 0.1 + 0.2, 2 ) )
   ?? "", Shown( Round( 0.1 + 0.2, 3 ) )
   ? Shown( Int( 100 ) ), Shown( Abs( 100 ) ), ;
      Shown( Sqrt( Abs( 100 ) ) ), Shown( Exp( Min( 100, 50 ) ) )
   ?? "", Shown( Log( Abs( 100 ) ) )
   ?? "", Shown( Round( 100, -2 ) )
   ?? "", Shown( Round( 100, -1 ) )
   ?? "", Shown( Round( 100, 0 ) )
   ?? "", Shown( Round( 100, 1 ) )
   ?? "", Shown( Round( 100, 2 ) )
   ?? "", Shown( Round( 100, 3 ) )
   ? Shown( Int( 4503599627370497 * 1.0 ) ), Shown( Abs( 4503599627370497 * 1.0 ) ), ;
      Shown( Sqrt( Abs( 4503599627370497 * 1.0 ) ) ), Shown( Exp( Min( 4503599627370497 * 1.0, 50 ) ) )
   ?? "", Shown( Log( Abs( 4503599627370497 * 1.0 ) ) )
   ?? "", Shown( Round( 4503599627370497 * 1.0, -2 ) )
   ?? "", Shown( Round( 4503599627370497 * 1.0, -1 ) )
   ?? "", Shown( Round( 4503599627370497 * 1.0, 0 ) )
   ?? "", Shown( Round( 4503599627370497 * 1.0, 1 ) )
   ?? "", Shown( Round( 4503599627370497 * 1.0, 2 ) )
   ?? "", Shown( Round( 4503599627370497 * 1.0, 3 ) )
   ? "runtime"
   FOR EACH x IN aVal
      ? Shown( Int( x ) ), Shown( Abs( x ) ), Shown( Sqrt( Abs( x ) ) ), Shown( Exp( Min( x, 50 ) ) )
      ?? "", Shown( Log( Abs( x ) ) )
      FOR EACH n IN { -2, -1, 0, 1, 2, 3 }
         ?? "", Shown( Round( x, n ) )
      NEXT
   NEXT
   ? "mod"
   FOR EACH x IN aVal
      ? ""
      FOR EACH y IN { 3, -3, 0.5, 0, 1 }
         ?? "", Shown( Mod( x, y ) )
      NEXT
   NEXT
   ? "max min"
   FOR EACH x IN { 1, 2.5, -1, 0 }
      ? ""
      FOR EACH y IN { 2, 2.50, -1.0, 0, 1 }
         ?? "", Shown( Max( x, y ) ), Shown( Min( x, y ) )
      NEXT
   NEXT
   ? Shown( Sqrt( -4 ) ), Shown( Log( 0 ) ), Shown( Log( -1 ) ), Shown( Exp( 1000 ) ), Shown( Exp( -1000 ) )
   ? Max( SToD( "20240101" ), SToD( "20231231" ) ), Min( SToD( "" ), SToD( "20000101" ) )
RETURN

STATIC FUNCTION Shown( x )
RETURN "[" + Str( x ) + "|" + LTrim( Str( x, 25, 6 ) ) + "]"
