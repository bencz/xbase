// The number functions the compiler does in place (Abs, Max, Min, Round, Sqrt, Exp, Log, Mod, Int) give what the
// runtime's functions give, which a macro calls: the same type, display width, decimals and value, for numbers at
// every limit (0 and -0, halves, past 2^52, the largest integers and INT64_MIN, infinities, NaN), places within and
// past 22 and not integers, bases of every sign and zero (whose error's block substitutes a value), dates and other
// types (errors). Each case shows only when they differ; then the count of cases.
MEMVAR x, y

PROCEDURE Main()
   LOCAL aVal, aPlaces, aBase, nCases := 0, nDiffs := 0, c, k, vx, vy
   LOCAL aOne := { {|| Abs( x ) }, {|| Sqrt( x ) }, {|| Exp( x ) }, {|| Log( x ) }, {|| Int( x ) } }
   PRIVATE x, y
   aVal := { 0, 0.0 * -1, 1, -1, 5, -5, 2.5, -2.5, 2.675, -2.675, 0.125, 1000000000000000.5, ;
      4503599627370497 * 1.0, 9223372036854775807, -9223372036854775807 - 1, 10 ^ 300 * 10 ^ 300, ;
      -( 10 ^ 300 ) * 10 ^ 300, Log( -1 ) * 0, 123.456, Val( "1.5" ), 7 / 2, SToD( "20240229" ), "a", NIL }
   aPlaces := { -3, -1, 0, 1, 2, 4, 22, 23, -23, 2.5, "2" }
   aBase := { 3, -3, 0.5, -0.5, 0, 7 / 2 }
   ErrorBlock( {| e | iif( e:canSubstitute, -99, Break( e ) ) } )
   FOR EACH vx IN aVal
      x := vx
      FOR EACH k => c IN { "Abs", "Sqrt", "Exp", "Log", "Int" }
         Compare( c + "( x )", aOne[ k ], c + "( M->x )", @nCases, @nDiffs )
      NEXT
      FOR EACH vy IN aPlaces
         y := vy
         Compare( "Round( x, y )", {|| Round( x, y ) }, "Round( M->x, M->y )", @nCases, @nDiffs )
      NEXT
      FOR EACH vy IN aBase
         y := vy
         Compare( "Mod( x, y )", {|| Mod( x, y ) }, "Mod( M->x, M->y )", @nCases, @nDiffs )
      NEXT
      FOR EACH vy IN aVal
         y := vy
         Compare( "Max( x, y )", {|| Max( x, y ) }, "Max( M->x, M->y )", @nCases, @nDiffs )
         Compare( "Min( x, y )", {|| Min( x, y ) }, "Min( M->x, M->y )", @nCases, @nDiffs )
      NEXT
   NEXT
   ? "cases", nCases, "differences", nDiffs
   ? Abs( -5 ), Abs( 5 ), Abs( -2.50 ), Round( 2.675, 2 ), Round( -2.5, 0 ), Round( 1234.5678, -2 ), Max( 1, 1.0 ), ;
      Min( SToD( "20240101" ), SToD( "20231231" ) ), Sqrt( 16 ), Mod( -7, 3 ), Mod( 7, -3 ), Mod( 7.5, 2 )
RETURN

// The block (a compiled call, done in place) against the same call compiled from text (the runtime's function).
STATIC PROCEDURE Compare( cWhat, bCompiled, cText, nCases, nDiffs )
   LOCAL cA := Shown( bCompiled ), cB := Shown( {|| &cText } )
   nCases++
   IF !( cA == cB )
      nDiffs++
      ? cWhat, Shown( {|| x } ), Shown( {|| y } ), cA, cB
   ENDIF
RETURN

STATIC FUNCTION Shown( b )
   LOCAL v, e
   BEGIN SEQUENCE
      v := Eval( b )
   RECOVER USING e
      RETURN "E" + LTrim( Str( e:subCode ) )
   END SEQUENCE
   DO CASE
   CASE ValType( v ) == "N"
      RETURN "N[" + Str( v ) + "]" + LTrim( Str( Len( Str( v ) ) ) ) + "/" + Str( v, 40, 12 )
   CASE ValType( v ) == "D"
      RETURN "D" + DToS( v )
   ENDCASE
RETURN ValType( v )
