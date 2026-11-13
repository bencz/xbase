// A macro folds what the compiler folds, with the same rules (an exact division of integers is an integer, an integer
// constant's display width, a quotient's decimals SET DECIMALS when the code runs), through the operators, the bit
// operators and functions, Len() of a literal, an IIF() on a folded condition (comparisons, .NOT., .AND., .OR.) and a
// list: each text gives what the same code compiled gives, also in a codeblock made before SET DECIMALS changes.
PROCEDURE Main()
   LOCAL aText, aCompiled, aMacro := {}, c, i, n
   aText := { "1000000000 * 1", "999999999 / 3", "7 % 3", "-( 9999999999 )", "- 5", "2 ^ 10", "1 / 3 * 1.5", "1 / 3", ;
      "7.5 % 2", "9223372036854775807 + 1", "4611686018427387904 * 4", "( 1 + 2 ) * 3.25", "-1.50 * 2", "10 / 4", ;
      "1 / 0 + 0", "+7 - -3", "1000000000 + 0.5", "123456789012 - 1", "( 6 | 0 ) / 3", "~5 / 2", ;
      "( 1 << 40 ) / 1024", "-16 >> 2", "( 5 ^^ 3 ) * 1000000000", 'Len( "abc" ) / 3', ;
      'Len( { 1, "x", NIL } ) * 1000000000', "BitAnd( 12, 10 ) / 2", "BitNot( 0 ) % 7", "iif( .T., 6, 0 ) / 3", ;
      "iif( 1 < 2, 9, 0 ) / 3", "iif( 2.5 >= 3, 9, 8 ) / 4", 'iif( "ab" == "ab", 6, 0 ) / 4', ;
      'iif( "a" $ "cab", 6, 0 ) / 3', "iif( .NOT. .F., 9, 0 ) % 4", "iif( .F. < .T., 2, 0 ) ^ 2", ;
      "iif( .T. .AND. 1 != 1, 3, 12 ) / 4", 'iif( .F. .OR. "a" = "a", 15, 0 ) / 5', "( 0, 6 ) / 3", ;
      '( "x", NIL, 1000000000 ) * 1' }
   aCompiled := { {|| 1000000000 * 1 }, {|| 999999999 / 3 }, {|| 7 % 3 }, {|| -( 9999999999 ) }, {|| - 5 }, ;
      {|| 2 ^ 10 }, {|| 1 / 3 * 1.5 }, {|| 1 / 3 }, {|| 7.5 % 2 }, {|| 9223372036854775807 + 1 }, ;
      {|| 4611686018427387904 * 4 }, {|| ( 1 + 2 ) * 3.25 }, {|| -1.50 * 2 }, {|| 10 / 4 }, {|| 1 / 0 + 0 }, ;
      {|| +7 - -3 }, {|| 1000000000 + 0.5 }, {|| 123456789012 - 1 }, {|| ( 6 | 0 ) / 3 }, {|| ~5 / 2 }, ;
      {|| ( 1 << 40 ) / 1024 }, {|| -16 >> 2 }, {|| ( 5 ^^ 3 ) * 1000000000 }, {|| Len( "abc" ) / 3 }, ;
      {|| Len( { 1, "x", NIL } ) * 1000000000 }, {|| BitAnd( 12, 10 ) / 2 }, {|| BitNot( 0 ) % 7 }, ;
      {|| iif( .T., 6, 0 ) / 3 }, {|| iif( 1 < 2, 9, 0 ) / 3 }, {|| iif( 2.5 >= 3, 9, 8 ) / 4 }, ;
      {|| iif( "ab" == "ab", 6, 0 ) / 4 }, {|| iif( "a" $ "cab", 6, 0 ) / 3 }, {|| iif( .NOT. .F., 9, 0 ) % 4 }, ;
      {|| iif( .F. < .T., 2, 0 ) ^ 2 }, {|| iif( .T. .AND. 1 != 1, 3, 12 ) / 4 }, ;
      {|| iif( .F. .OR. "a" = "a", 15, 0 ) / 5 }, {|| ( 0, 6 ) / 3 }, {|| ( "x", NIL, 1000000000 ) * 1 } }
   ErrorBlock( {| e | iif( e:canSubstitute, -1, Break( e ) ) } )
   FOR EACH c IN aText
      AAdd( aMacro, &( "{|| " + c + " }" ) )
   NEXT
   FOR EACH n IN { 2, 5, 0 }
      SET DECIMALS TO n
      ? "decimals", n
      FOR i := 1 TO Len( aText )
         ? PadR( aText[ i ], 36 ), Shown( Eval( aCompiled[ i ] ) ), Shown( &( aText[ i ] ) ), ;
            Shown( Eval( aMacro[ i ] ) )
      NEXT
   NEXT
RETURN

STATIC FUNCTION Shown( x )
RETURN "[" + Str( x ) + "]"
