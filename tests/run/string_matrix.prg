// Every pair of string values with every comparison operator, as the five ways a program meets them: literals
// (folded when compiled, where SET EXACT allows), the parameters of small functions called with literals (inlined),
// elements of an array made at run time, macros compiled from text, and values read from the command line (<n>
// stands for Chr( n )). The operators SET EXACT changes run under both settings. Each line is one left value against
// every right value, T or F.
PROCEDURE Main( ... )
   LOCAL aVal, aIn := {}, cArg, aText
   aVal := { "", " ", "a", "A", "ab", "ab ", "abc", ;
      "b", " a", ( "a" + Chr( 0 ) ), Chr( 0 ), Chr( 200 ), "aB" }
   aText := { '""', '" "', '"a"', '"A"', '"ab"', '"ab "', '"abc"', '"b"', '" a"', ;
      '"a" + Chr( 0 )', 'Chr( 0 )', 'Chr( 200 )', '"aB"' }
   FOR EACH cArg IN { ... }
      AAdd( aIn, Decoded( cArg ) )
   NEXT
   SET EXACT OFF
   ShowEeq( aVal, aText, aIn )
   SET EXACT OFF
   ShowHas( aVal, aText, aIn )
   SET EXACT OFF
   ShowEq( aVal, aText, aIn )
   SET EXACT OFF
   ShowNe( aVal, aText, aIn )
   SET EXACT OFF
   ShowLt( aVal, aText, aIn )
   SET EXACT OFF
   ShowLe( aVal, aText, aIn )
   SET EXACT OFF
   ShowGt( aVal, aText, aIn )
   SET EXACT OFF
   ShowGe( aVal, aText, aIn )
   SET EXACT ON
   ShowEqExact( aVal, aText, aIn )
   SET EXACT ON
   ShowNeExact( aVal, aText, aIn )
   SET EXACT ON
   ShowLtExact( aVal, aText, aIn )
   SET EXACT ON
   ShowLeExact( aVal, aText, aIn )
   SET EXACT ON
   ShowGtExact( aVal, aText, aIn )
   SET EXACT ON
   ShowGeExact( aVal, aText, aIn )
   SET EXACT OFF
RETURN

STATIC PROCEDURE ShowEeq( aVal, aText, aIn )
   LOCAL c
   ? "literal =="
   c := TF( "" == "" ) + TF( "" == " " ) + TF( "" == "a" ) + TF( "" == "A" ) + TF( "" == "ab" ) + TF( "" == "ab " )
   c += TF( "" == "abc" ) + TF( "" == "b" ) + TF( "" == " a" ) + TF( "" == ( "a" + Chr( 0 ) ) ) + TF( "" == Chr( 0 ) )
   c += TF( "" == Chr( 200 ) ) + TF( "" == "aB" )
   ? c
   c := TF( " " == "" ) + TF( " " == " " ) + TF( " " == "a" ) + TF( " " == "A" ) + TF( " " == "ab" )
   c += TF( " " == "ab " ) + TF( " " == "abc" ) + TF( " " == "b" ) + TF( " " == " a" ) + TF( " " == ( "a" + Chr( 0 ) ) )
   c += TF( " " == Chr( 0 ) ) + TF( " " == Chr( 200 ) ) + TF( " " == "aB" )
   ? c
   c := TF( "a" == "" ) + TF( "a" == " " ) + TF( "a" == "a" ) + TF( "a" == "A" ) + TF( "a" == "ab" )
   c += TF( "a" == "ab " ) + TF( "a" == "abc" ) + TF( "a" == "b" ) + TF( "a" == " a" ) + TF( "a" == ( "a" + Chr( 0 ) ) )
   c += TF( "a" == Chr( 0 ) ) + TF( "a" == Chr( 200 ) ) + TF( "a" == "aB" )
   ? c
   c := TF( "A" == "" ) + TF( "A" == " " ) + TF( "A" == "a" ) + TF( "A" == "A" ) + TF( "A" == "ab" )
   c += TF( "A" == "ab " ) + TF( "A" == "abc" ) + TF( "A" == "b" ) + TF( "A" == " a" ) + TF( "A" == ( "a" + Chr( 0 ) ) )
   c += TF( "A" == Chr( 0 ) ) + TF( "A" == Chr( 200 ) ) + TF( "A" == "aB" )
   ? c
   c := TF( "ab" == "" ) + TF( "ab" == " " ) + TF( "ab" == "a" ) + TF( "ab" == "A" ) + TF( "ab" == "ab" )
   c += TF( "ab" == "ab " ) + TF( "ab" == "abc" ) + TF( "ab" == "b" ) + TF( "ab" == " a" )
   c += TF( "ab" == ( "a" + Chr( 0 ) ) ) + TF( "ab" == Chr( 0 ) ) + TF( "ab" == Chr( 200 ) ) + TF( "ab" == "aB" )
   ? c
   c := TF( "ab " == "" ) + TF( "ab " == " " ) + TF( "ab " == "a" ) + TF( "ab " == "A" ) + TF( "ab " == "ab" )
   c += TF( "ab " == "ab " ) + TF( "ab " == "abc" ) + TF( "ab " == "b" ) + TF( "ab " == " a" )
   c += TF( "ab " == ( "a" + Chr( 0 ) ) ) + TF( "ab " == Chr( 0 ) ) + TF( "ab " == Chr( 200 ) ) + TF( "ab " == "aB" )
   ? c
   c := TF( "abc" == "" ) + TF( "abc" == " " ) + TF( "abc" == "a" ) + TF( "abc" == "A" ) + TF( "abc" == "ab" )
   c += TF( "abc" == "ab " ) + TF( "abc" == "abc" ) + TF( "abc" == "b" ) + TF( "abc" == " a" )
   c += TF( "abc" == ( "a" + Chr( 0 ) ) ) + TF( "abc" == Chr( 0 ) ) + TF( "abc" == Chr( 200 ) ) + TF( "abc" == "aB" )
   ? c
   c := TF( "b" == "" ) + TF( "b" == " " ) + TF( "b" == "a" ) + TF( "b" == "A" ) + TF( "b" == "ab" )
   c += TF( "b" == "ab " ) + TF( "b" == "abc" ) + TF( "b" == "b" ) + TF( "b" == " a" ) + TF( "b" == ( "a" + Chr( 0 ) ) )
   c += TF( "b" == Chr( 0 ) ) + TF( "b" == Chr( 200 ) ) + TF( "b" == "aB" )
   ? c
   c := TF( " a" == "" ) + TF( " a" == " " ) + TF( " a" == "a" ) + TF( " a" == "A" ) + TF( " a" == "ab" )
   c += TF( " a" == "ab " ) + TF( " a" == "abc" ) + TF( " a" == "b" ) + TF( " a" == " a" )
   c += TF( " a" == ( "a" + Chr( 0 ) ) ) + TF( " a" == Chr( 0 ) ) + TF( " a" == Chr( 200 ) ) + TF( " a" == "aB" )
   ? c
   c := TF( ( "a" + Chr( 0 ) ) == "" ) + TF( ( "a" + Chr( 0 ) ) == " " ) + TF( ( "a" + Chr( 0 ) ) == "a" )
   c += TF( ( "a" + Chr( 0 ) ) == "A" ) + TF( ( "a" + Chr( 0 ) ) == "ab" ) + TF( ( "a" + Chr( 0 ) ) == "ab " )
   c += TF( ( "a" + Chr( 0 ) ) == "abc" ) + TF( ( "a" + Chr( 0 ) ) == "b" ) + TF( ( "a" + Chr( 0 ) ) == " a" )
   c += TF( ( "a" + Chr( 0 ) ) == ( "a" + Chr( 0 ) ) ) + TF( ( "a" + Chr( 0 ) ) == Chr( 0 ) )
   c += TF( ( "a" + Chr( 0 ) ) == Chr( 200 ) ) + TF( ( "a" + Chr( 0 ) ) == "aB" )
   ? c
   c := TF( Chr( 0 ) == "" ) + TF( Chr( 0 ) == " " ) + TF( Chr( 0 ) == "a" ) + TF( Chr( 0 ) == "A" )
   c += TF( Chr( 0 ) == "ab" ) + TF( Chr( 0 ) == "ab " ) + TF( Chr( 0 ) == "abc" ) + TF( Chr( 0 ) == "b" )
   c += TF( Chr( 0 ) == " a" ) + TF( Chr( 0 ) == ( "a" + Chr( 0 ) ) ) + TF( Chr( 0 ) == Chr( 0 ) )
   c += TF( Chr( 0 ) == Chr( 200 ) ) + TF( Chr( 0 ) == "aB" )
   ? c
   c := TF( Chr( 200 ) == "" ) + TF( Chr( 200 ) == " " ) + TF( Chr( 200 ) == "a" ) + TF( Chr( 200 ) == "A" )
   c += TF( Chr( 200 ) == "ab" ) + TF( Chr( 200 ) == "ab " ) + TF( Chr( 200 ) == "abc" ) + TF( Chr( 200 ) == "b" )
   c += TF( Chr( 200 ) == " a" ) + TF( Chr( 200 ) == ( "a" + Chr( 0 ) ) ) + TF( Chr( 200 ) == Chr( 0 ) )
   c += TF( Chr( 200 ) == Chr( 200 ) ) + TF( Chr( 200 ) == "aB" )
   ? c
   c := TF( "aB" == "" ) + TF( "aB" == " " ) + TF( "aB" == "a" ) + TF( "aB" == "A" ) + TF( "aB" == "ab" )
   c += TF( "aB" == "ab " ) + TF( "aB" == "abc" ) + TF( "aB" == "b" ) + TF( "aB" == " a" )
   c += TF( "aB" == ( "a" + Chr( 0 ) ) ) + TF( "aB" == Chr( 0 ) ) + TF( "aB" == Chr( 200 ) ) + TF( "aB" == "aB" )
   ? c
   ? "inlined =="
   c := TF( Eeq( "", "" ) ) + TF( Eeq( "", " " ) ) + TF( Eeq( "", "a" ) ) + TF( Eeq( "", "A" ) ) + TF( Eeq( "", "ab" ) )
   c += TF( Eeq( "", "ab " ) ) + TF( Eeq( "", "abc" ) ) + TF( Eeq( "", "b" ) ) + TF( Eeq( "", " a" ) )
   c += TF( Eeq( "", ( "a" + Chr( 0 ) ) ) ) + TF( Eeq( "", Chr( 0 ) ) ) + TF( Eeq( "", Chr( 200 ) ) )
   c += TF( Eeq( "", "aB" ) )
   ? c
   c := TF( Eeq( " ", "" ) ) + TF( Eeq( " ", " " ) ) + TF( Eeq( " ", "a" ) ) + TF( Eeq( " ", "A" ) )
   c += TF( Eeq( " ", "ab" ) ) + TF( Eeq( " ", "ab " ) ) + TF( Eeq( " ", "abc" ) ) + TF( Eeq( " ", "b" ) )
   c += TF( Eeq( " ", " a" ) ) + TF( Eeq( " ", ( "a" + Chr( 0 ) ) ) ) + TF( Eeq( " ", Chr( 0 ) ) )
   c += TF( Eeq( " ", Chr( 200 ) ) ) + TF( Eeq( " ", "aB" ) )
   ? c
   c := TF( Eeq( "a", "" ) ) + TF( Eeq( "a", " " ) ) + TF( Eeq( "a", "a" ) ) + TF( Eeq( "a", "A" ) )
   c += TF( Eeq( "a", "ab" ) ) + TF( Eeq( "a", "ab " ) ) + TF( Eeq( "a", "abc" ) ) + TF( Eeq( "a", "b" ) )
   c += TF( Eeq( "a", " a" ) ) + TF( Eeq( "a", ( "a" + Chr( 0 ) ) ) ) + TF( Eeq( "a", Chr( 0 ) ) )
   c += TF( Eeq( "a", Chr( 200 ) ) ) + TF( Eeq( "a", "aB" ) )
   ? c
   c := TF( Eeq( "A", "" ) ) + TF( Eeq( "A", " " ) ) + TF( Eeq( "A", "a" ) ) + TF( Eeq( "A", "A" ) )
   c += TF( Eeq( "A", "ab" ) ) + TF( Eeq( "A", "ab " ) ) + TF( Eeq( "A", "abc" ) ) + TF( Eeq( "A", "b" ) )
   c += TF( Eeq( "A", " a" ) ) + TF( Eeq( "A", ( "a" + Chr( 0 ) ) ) ) + TF( Eeq( "A", Chr( 0 ) ) )
   c += TF( Eeq( "A", Chr( 200 ) ) ) + TF( Eeq( "A", "aB" ) )
   ? c
   c := TF( Eeq( "ab", "" ) ) + TF( Eeq( "ab", " " ) ) + TF( Eeq( "ab", "a" ) ) + TF( Eeq( "ab", "A" ) )
   c += TF( Eeq( "ab", "ab" ) ) + TF( Eeq( "ab", "ab " ) ) + TF( Eeq( "ab", "abc" ) ) + TF( Eeq( "ab", "b" ) )
   c += TF( Eeq( "ab", " a" ) ) + TF( Eeq( "ab", ( "a" + Chr( 0 ) ) ) ) + TF( Eeq( "ab", Chr( 0 ) ) )
   c += TF( Eeq( "ab", Chr( 200 ) ) ) + TF( Eeq( "ab", "aB" ) )
   ? c
   c := TF( Eeq( "ab ", "" ) ) + TF( Eeq( "ab ", " " ) ) + TF( Eeq( "ab ", "a" ) ) + TF( Eeq( "ab ", "A" ) )
   c += TF( Eeq( "ab ", "ab" ) ) + TF( Eeq( "ab ", "ab " ) ) + TF( Eeq( "ab ", "abc" ) ) + TF( Eeq( "ab ", "b" ) )
   c += TF( Eeq( "ab ", " a" ) ) + TF( Eeq( "ab ", ( "a" + Chr( 0 ) ) ) ) + TF( Eeq( "ab ", Chr( 0 ) ) )
   c += TF( Eeq( "ab ", Chr( 200 ) ) ) + TF( Eeq( "ab ", "aB" ) )
   ? c
   c := TF( Eeq( "abc", "" ) ) + TF( Eeq( "abc", " " ) ) + TF( Eeq( "abc", "a" ) ) + TF( Eeq( "abc", "A" ) )
   c += TF( Eeq( "abc", "ab" ) ) + TF( Eeq( "abc", "ab " ) ) + TF( Eeq( "abc", "abc" ) ) + TF( Eeq( "abc", "b" ) )
   c += TF( Eeq( "abc", " a" ) ) + TF( Eeq( "abc", ( "a" + Chr( 0 ) ) ) ) + TF( Eeq( "abc", Chr( 0 ) ) )
   c += TF( Eeq( "abc", Chr( 200 ) ) ) + TF( Eeq( "abc", "aB" ) )
   ? c
   c := TF( Eeq( "b", "" ) ) + TF( Eeq( "b", " " ) ) + TF( Eeq( "b", "a" ) ) + TF( Eeq( "b", "A" ) )
   c += TF( Eeq( "b", "ab" ) ) + TF( Eeq( "b", "ab " ) ) + TF( Eeq( "b", "abc" ) ) + TF( Eeq( "b", "b" ) )
   c += TF( Eeq( "b", " a" ) ) + TF( Eeq( "b", ( "a" + Chr( 0 ) ) ) ) + TF( Eeq( "b", Chr( 0 ) ) )
   c += TF( Eeq( "b", Chr( 200 ) ) ) + TF( Eeq( "b", "aB" ) )
   ? c
   c := TF( Eeq( " a", "" ) ) + TF( Eeq( " a", " " ) ) + TF( Eeq( " a", "a" ) ) + TF( Eeq( " a", "A" ) )
   c += TF( Eeq( " a", "ab" ) ) + TF( Eeq( " a", "ab " ) ) + TF( Eeq( " a", "abc" ) ) + TF( Eeq( " a", "b" ) )
   c += TF( Eeq( " a", " a" ) ) + TF( Eeq( " a", ( "a" + Chr( 0 ) ) ) ) + TF( Eeq( " a", Chr( 0 ) ) )
   c += TF( Eeq( " a", Chr( 200 ) ) ) + TF( Eeq( " a", "aB" ) )
   ? c
   c := TF( Eeq( ( "a" + Chr( 0 ) ), "" ) ) + TF( Eeq( ( "a" + Chr( 0 ) ), " " ) )
   c += TF( Eeq( ( "a" + Chr( 0 ) ), "a" ) ) + TF( Eeq( ( "a" + Chr( 0 ) ), "A" ) )
   c += TF( Eeq( ( "a" + Chr( 0 ) ), "ab" ) ) + TF( Eeq( ( "a" + Chr( 0 ) ), "ab " ) )
   c += TF( Eeq( ( "a" + Chr( 0 ) ), "abc" ) ) + TF( Eeq( ( "a" + Chr( 0 ) ), "b" ) )
   c += TF( Eeq( ( "a" + Chr( 0 ) ), " a" ) ) + TF( Eeq( ( "a" + Chr( 0 ) ), ( "a" + Chr( 0 ) ) ) )
   c += TF( Eeq( ( "a" + Chr( 0 ) ), Chr( 0 ) ) ) + TF( Eeq( ( "a" + Chr( 0 ) ), Chr( 200 ) ) )
   c += TF( Eeq( ( "a" + Chr( 0 ) ), "aB" ) )
   ? c
   c := TF( Eeq( Chr( 0 ), "" ) ) + TF( Eeq( Chr( 0 ), " " ) ) + TF( Eeq( Chr( 0 ), "a" ) ) + TF( Eeq( Chr( 0 ), "A" ) )
   c += TF( Eeq( Chr( 0 ), "ab" ) ) + TF( Eeq( Chr( 0 ), "ab " ) ) + TF( Eeq( Chr( 0 ), "abc" ) )
   c += TF( Eeq( Chr( 0 ), "b" ) ) + TF( Eeq( Chr( 0 ), " a" ) ) + TF( Eeq( Chr( 0 ), ( "a" + Chr( 0 ) ) ) )
   c += TF( Eeq( Chr( 0 ), Chr( 0 ) ) ) + TF( Eeq( Chr( 0 ), Chr( 200 ) ) ) + TF( Eeq( Chr( 0 ), "aB" ) )
   ? c
   c := TF( Eeq( Chr( 200 ), "" ) ) + TF( Eeq( Chr( 200 ), " " ) ) + TF( Eeq( Chr( 200 ), "a" ) )
   c += TF( Eeq( Chr( 200 ), "A" ) ) + TF( Eeq( Chr( 200 ), "ab" ) ) + TF( Eeq( Chr( 200 ), "ab " ) )
   c += TF( Eeq( Chr( 200 ), "abc" ) ) + TF( Eeq( Chr( 200 ), "b" ) ) + TF( Eeq( Chr( 200 ), " a" ) )
   c += TF( Eeq( Chr( 200 ), ( "a" + Chr( 0 ) ) ) ) + TF( Eeq( Chr( 200 ), Chr( 0 ) ) )
   c += TF( Eeq( Chr( 200 ), Chr( 200 ) ) ) + TF( Eeq( Chr( 200 ), "aB" ) )
   ? c
   c := TF( Eeq( "aB", "" ) ) + TF( Eeq( "aB", " " ) ) + TF( Eeq( "aB", "a" ) ) + TF( Eeq( "aB", "A" ) )
   c += TF( Eeq( "aB", "ab" ) ) + TF( Eeq( "aB", "ab " ) ) + TF( Eeq( "aB", "abc" ) ) + TF( Eeq( "aB", "b" ) )
   c += TF( Eeq( "aB", " a" ) ) + TF( Eeq( "aB", ( "a" + Chr( 0 ) ) ) ) + TF( Eeq( "aB", Chr( 0 ) ) )
   c += TF( Eeq( "aB", Chr( 200 ) ) ) + TF( Eeq( "aB", "aB" ) )
   ? c
   ? "runtime =="
   Table( aVal, {| a, b | a == b } )
   ? "macro =="
   Table( aText, {| a, b | &( a + " == " + b ) } )
   ? "input =="
   Table( aIn, {| a, b | a == b } )
RETURN

STATIC PROCEDURE ShowHas( aVal, aText, aIn )
   LOCAL c
   ? "literal $"
   c := TF( "" $ "" ) + TF( "" $ " " ) + TF( "" $ "a" ) + TF( "" $ "A" ) + TF( "" $ "ab" ) + TF( "" $ "ab " )
   c += TF( "" $ "abc" ) + TF( "" $ "b" ) + TF( "" $ " a" ) + TF( "" $ ( "a" + Chr( 0 ) ) ) + TF( "" $ Chr( 0 ) )
   c += TF( "" $ Chr( 200 ) ) + TF( "" $ "aB" )
   ? c
   c := TF( " " $ "" ) + TF( " " $ " " ) + TF( " " $ "a" ) + TF( " " $ "A" ) + TF( " " $ "ab" ) + TF( " " $ "ab " )
   c += TF( " " $ "abc" ) + TF( " " $ "b" ) + TF( " " $ " a" ) + TF( " " $ ( "a" + Chr( 0 ) ) ) + TF( " " $ Chr( 0 ) )
   c += TF( " " $ Chr( 200 ) ) + TF( " " $ "aB" )
   ? c
   c := TF( "a" $ "" ) + TF( "a" $ " " ) + TF( "a" $ "a" ) + TF( "a" $ "A" ) + TF( "a" $ "ab" ) + TF( "a" $ "ab " )
   c += TF( "a" $ "abc" ) + TF( "a" $ "b" ) + TF( "a" $ " a" ) + TF( "a" $ ( "a" + Chr( 0 ) ) ) + TF( "a" $ Chr( 0 ) )
   c += TF( "a" $ Chr( 200 ) ) + TF( "a" $ "aB" )
   ? c
   c := TF( "A" $ "" ) + TF( "A" $ " " ) + TF( "A" $ "a" ) + TF( "A" $ "A" ) + TF( "A" $ "ab" ) + TF( "A" $ "ab " )
   c += TF( "A" $ "abc" ) + TF( "A" $ "b" ) + TF( "A" $ " a" ) + TF( "A" $ ( "a" + Chr( 0 ) ) ) + TF( "A" $ Chr( 0 ) )
   c += TF( "A" $ Chr( 200 ) ) + TF( "A" $ "aB" )
   ? c
   c := TF( "ab" $ "" ) + TF( "ab" $ " " ) + TF( "ab" $ "a" ) + TF( "ab" $ "A" ) + TF( "ab" $ "ab" )
   c += TF( "ab" $ "ab " ) + TF( "ab" $ "abc" ) + TF( "ab" $ "b" ) + TF( "ab" $ " a" ) + TF( "ab" $ ( "a" + Chr( 0 ) ) )
   c += TF( "ab" $ Chr( 0 ) ) + TF( "ab" $ Chr( 200 ) ) + TF( "ab" $ "aB" )
   ? c
   c := TF( "ab " $ "" ) + TF( "ab " $ " " ) + TF( "ab " $ "a" ) + TF( "ab " $ "A" ) + TF( "ab " $ "ab" )
   c += TF( "ab " $ "ab " ) + TF( "ab " $ "abc" ) + TF( "ab " $ "b" ) + TF( "ab " $ " a" )
   c += TF( "ab " $ ( "a" + Chr( 0 ) ) ) + TF( "ab " $ Chr( 0 ) ) + TF( "ab " $ Chr( 200 ) ) + TF( "ab " $ "aB" )
   ? c
   c := TF( "abc" $ "" ) + TF( "abc" $ " " ) + TF( "abc" $ "a" ) + TF( "abc" $ "A" ) + TF( "abc" $ "ab" )
   c += TF( "abc" $ "ab " ) + TF( "abc" $ "abc" ) + TF( "abc" $ "b" ) + TF( "abc" $ " a" )
   c += TF( "abc" $ ( "a" + Chr( 0 ) ) ) + TF( "abc" $ Chr( 0 ) ) + TF( "abc" $ Chr( 200 ) ) + TF( "abc" $ "aB" )
   ? c
   c := TF( "b" $ "" ) + TF( "b" $ " " ) + TF( "b" $ "a" ) + TF( "b" $ "A" ) + TF( "b" $ "ab" ) + TF( "b" $ "ab " )
   c += TF( "b" $ "abc" ) + TF( "b" $ "b" ) + TF( "b" $ " a" ) + TF( "b" $ ( "a" + Chr( 0 ) ) ) + TF( "b" $ Chr( 0 ) )
   c += TF( "b" $ Chr( 200 ) ) + TF( "b" $ "aB" )
   ? c
   c := TF( " a" $ "" ) + TF( " a" $ " " ) + TF( " a" $ "a" ) + TF( " a" $ "A" ) + TF( " a" $ "ab" )
   c += TF( " a" $ "ab " ) + TF( " a" $ "abc" ) + TF( " a" $ "b" ) + TF( " a" $ " a" ) + TF( " a" $ ( "a" + Chr( 0 ) ) )
   c += TF( " a" $ Chr( 0 ) ) + TF( " a" $ Chr( 200 ) ) + TF( " a" $ "aB" )
   ? c
   c := TF( ( "a" + Chr( 0 ) ) $ "" ) + TF( ( "a" + Chr( 0 ) ) $ " " ) + TF( ( "a" + Chr( 0 ) ) $ "a" )
   c += TF( ( "a" + Chr( 0 ) ) $ "A" ) + TF( ( "a" + Chr( 0 ) ) $ "ab" ) + TF( ( "a" + Chr( 0 ) ) $ "ab " )
   c += TF( ( "a" + Chr( 0 ) ) $ "abc" ) + TF( ( "a" + Chr( 0 ) ) $ "b" ) + TF( ( "a" + Chr( 0 ) ) $ " a" )
   c += TF( ( "a" + Chr( 0 ) ) $ ( "a" + Chr( 0 ) ) ) + TF( ( "a" + Chr( 0 ) ) $ Chr( 0 ) )
   c += TF( ( "a" + Chr( 0 ) ) $ Chr( 200 ) ) + TF( ( "a" + Chr( 0 ) ) $ "aB" )
   ? c
   c := TF( Chr( 0 ) $ "" ) + TF( Chr( 0 ) $ " " ) + TF( Chr( 0 ) $ "a" ) + TF( Chr( 0 ) $ "A" ) + TF( Chr( 0 ) $ "ab" )
   c += TF( Chr( 0 ) $ "ab " ) + TF( Chr( 0 ) $ "abc" ) + TF( Chr( 0 ) $ "b" ) + TF( Chr( 0 ) $ " a" )
   c += TF( Chr( 0 ) $ ( "a" + Chr( 0 ) ) ) + TF( Chr( 0 ) $ Chr( 0 ) ) + TF( Chr( 0 ) $ Chr( 200 ) )
   c += TF( Chr( 0 ) $ "aB" )
   ? c
   c := TF( Chr( 200 ) $ "" ) + TF( Chr( 200 ) $ " " ) + TF( Chr( 200 ) $ "a" ) + TF( Chr( 200 ) $ "A" )
   c += TF( Chr( 200 ) $ "ab" ) + TF( Chr( 200 ) $ "ab " ) + TF( Chr( 200 ) $ "abc" ) + TF( Chr( 200 ) $ "b" )
   c += TF( Chr( 200 ) $ " a" ) + TF( Chr( 200 ) $ ( "a" + Chr( 0 ) ) ) + TF( Chr( 200 ) $ Chr( 0 ) )
   c += TF( Chr( 200 ) $ Chr( 200 ) ) + TF( Chr( 200 ) $ "aB" )
   ? c
   c := TF( "aB" $ "" ) + TF( "aB" $ " " ) + TF( "aB" $ "a" ) + TF( "aB" $ "A" ) + TF( "aB" $ "ab" )
   c += TF( "aB" $ "ab " ) + TF( "aB" $ "abc" ) + TF( "aB" $ "b" ) + TF( "aB" $ " a" ) + TF( "aB" $ ( "a" + Chr( 0 ) ) )
   c += TF( "aB" $ Chr( 0 ) ) + TF( "aB" $ Chr( 200 ) ) + TF( "aB" $ "aB" )
   ? c
   ? "inlined $"
   c := TF( Has( "", "" ) ) + TF( Has( "", " " ) ) + TF( Has( "", "a" ) ) + TF( Has( "", "A" ) ) + TF( Has( "", "ab" ) )
   c += TF( Has( "", "ab " ) ) + TF( Has( "", "abc" ) ) + TF( Has( "", "b" ) ) + TF( Has( "", " a" ) )
   c += TF( Has( "", ( "a" + Chr( 0 ) ) ) ) + TF( Has( "", Chr( 0 ) ) ) + TF( Has( "", Chr( 200 ) ) )
   c += TF( Has( "", "aB" ) )
   ? c
   c := TF( Has( " ", "" ) ) + TF( Has( " ", " " ) ) + TF( Has( " ", "a" ) ) + TF( Has( " ", "A" ) )
   c += TF( Has( " ", "ab" ) ) + TF( Has( " ", "ab " ) ) + TF( Has( " ", "abc" ) ) + TF( Has( " ", "b" ) )
   c += TF( Has( " ", " a" ) ) + TF( Has( " ", ( "a" + Chr( 0 ) ) ) ) + TF( Has( " ", Chr( 0 ) ) )
   c += TF( Has( " ", Chr( 200 ) ) ) + TF( Has( " ", "aB" ) )
   ? c
   c := TF( Has( "a", "" ) ) + TF( Has( "a", " " ) ) + TF( Has( "a", "a" ) ) + TF( Has( "a", "A" ) )
   c += TF( Has( "a", "ab" ) ) + TF( Has( "a", "ab " ) ) + TF( Has( "a", "abc" ) ) + TF( Has( "a", "b" ) )
   c += TF( Has( "a", " a" ) ) + TF( Has( "a", ( "a" + Chr( 0 ) ) ) ) + TF( Has( "a", Chr( 0 ) ) )
   c += TF( Has( "a", Chr( 200 ) ) ) + TF( Has( "a", "aB" ) )
   ? c
   c := TF( Has( "A", "" ) ) + TF( Has( "A", " " ) ) + TF( Has( "A", "a" ) ) + TF( Has( "A", "A" ) )
   c += TF( Has( "A", "ab" ) ) + TF( Has( "A", "ab " ) ) + TF( Has( "A", "abc" ) ) + TF( Has( "A", "b" ) )
   c += TF( Has( "A", " a" ) ) + TF( Has( "A", ( "a" + Chr( 0 ) ) ) ) + TF( Has( "A", Chr( 0 ) ) )
   c += TF( Has( "A", Chr( 200 ) ) ) + TF( Has( "A", "aB" ) )
   ? c
   c := TF( Has( "ab", "" ) ) + TF( Has( "ab", " " ) ) + TF( Has( "ab", "a" ) ) + TF( Has( "ab", "A" ) )
   c += TF( Has( "ab", "ab" ) ) + TF( Has( "ab", "ab " ) ) + TF( Has( "ab", "abc" ) ) + TF( Has( "ab", "b" ) )
   c += TF( Has( "ab", " a" ) ) + TF( Has( "ab", ( "a" + Chr( 0 ) ) ) ) + TF( Has( "ab", Chr( 0 ) ) )
   c += TF( Has( "ab", Chr( 200 ) ) ) + TF( Has( "ab", "aB" ) )
   ? c
   c := TF( Has( "ab ", "" ) ) + TF( Has( "ab ", " " ) ) + TF( Has( "ab ", "a" ) ) + TF( Has( "ab ", "A" ) )
   c += TF( Has( "ab ", "ab" ) ) + TF( Has( "ab ", "ab " ) ) + TF( Has( "ab ", "abc" ) ) + TF( Has( "ab ", "b" ) )
   c += TF( Has( "ab ", " a" ) ) + TF( Has( "ab ", ( "a" + Chr( 0 ) ) ) ) + TF( Has( "ab ", Chr( 0 ) ) )
   c += TF( Has( "ab ", Chr( 200 ) ) ) + TF( Has( "ab ", "aB" ) )
   ? c
   c := TF( Has( "abc", "" ) ) + TF( Has( "abc", " " ) ) + TF( Has( "abc", "a" ) ) + TF( Has( "abc", "A" ) )
   c += TF( Has( "abc", "ab" ) ) + TF( Has( "abc", "ab " ) ) + TF( Has( "abc", "abc" ) ) + TF( Has( "abc", "b" ) )
   c += TF( Has( "abc", " a" ) ) + TF( Has( "abc", ( "a" + Chr( 0 ) ) ) ) + TF( Has( "abc", Chr( 0 ) ) )
   c += TF( Has( "abc", Chr( 200 ) ) ) + TF( Has( "abc", "aB" ) )
   ? c
   c := TF( Has( "b", "" ) ) + TF( Has( "b", " " ) ) + TF( Has( "b", "a" ) ) + TF( Has( "b", "A" ) )
   c += TF( Has( "b", "ab" ) ) + TF( Has( "b", "ab " ) ) + TF( Has( "b", "abc" ) ) + TF( Has( "b", "b" ) )
   c += TF( Has( "b", " a" ) ) + TF( Has( "b", ( "a" + Chr( 0 ) ) ) ) + TF( Has( "b", Chr( 0 ) ) )
   c += TF( Has( "b", Chr( 200 ) ) ) + TF( Has( "b", "aB" ) )
   ? c
   c := TF( Has( " a", "" ) ) + TF( Has( " a", " " ) ) + TF( Has( " a", "a" ) ) + TF( Has( " a", "A" ) )
   c += TF( Has( " a", "ab" ) ) + TF( Has( " a", "ab " ) ) + TF( Has( " a", "abc" ) ) + TF( Has( " a", "b" ) )
   c += TF( Has( " a", " a" ) ) + TF( Has( " a", ( "a" + Chr( 0 ) ) ) ) + TF( Has( " a", Chr( 0 ) ) )
   c += TF( Has( " a", Chr( 200 ) ) ) + TF( Has( " a", "aB" ) )
   ? c
   c := TF( Has( ( "a" + Chr( 0 ) ), "" ) ) + TF( Has( ( "a" + Chr( 0 ) ), " " ) )
   c += TF( Has( ( "a" + Chr( 0 ) ), "a" ) ) + TF( Has( ( "a" + Chr( 0 ) ), "A" ) )
   c += TF( Has( ( "a" + Chr( 0 ) ), "ab" ) ) + TF( Has( ( "a" + Chr( 0 ) ), "ab " ) )
   c += TF( Has( ( "a" + Chr( 0 ) ), "abc" ) ) + TF( Has( ( "a" + Chr( 0 ) ), "b" ) )
   c += TF( Has( ( "a" + Chr( 0 ) ), " a" ) ) + TF( Has( ( "a" + Chr( 0 ) ), ( "a" + Chr( 0 ) ) ) )
   c += TF( Has( ( "a" + Chr( 0 ) ), Chr( 0 ) ) ) + TF( Has( ( "a" + Chr( 0 ) ), Chr( 200 ) ) )
   c += TF( Has( ( "a" + Chr( 0 ) ), "aB" ) )
   ? c
   c := TF( Has( Chr( 0 ), "" ) ) + TF( Has( Chr( 0 ), " " ) ) + TF( Has( Chr( 0 ), "a" ) ) + TF( Has( Chr( 0 ), "A" ) )
   c += TF( Has( Chr( 0 ), "ab" ) ) + TF( Has( Chr( 0 ), "ab " ) ) + TF( Has( Chr( 0 ), "abc" ) )
   c += TF( Has( Chr( 0 ), "b" ) ) + TF( Has( Chr( 0 ), " a" ) ) + TF( Has( Chr( 0 ), ( "a" + Chr( 0 ) ) ) )
   c += TF( Has( Chr( 0 ), Chr( 0 ) ) ) + TF( Has( Chr( 0 ), Chr( 200 ) ) ) + TF( Has( Chr( 0 ), "aB" ) )
   ? c
   c := TF( Has( Chr( 200 ), "" ) ) + TF( Has( Chr( 200 ), " " ) ) + TF( Has( Chr( 200 ), "a" ) )
   c += TF( Has( Chr( 200 ), "A" ) ) + TF( Has( Chr( 200 ), "ab" ) ) + TF( Has( Chr( 200 ), "ab " ) )
   c += TF( Has( Chr( 200 ), "abc" ) ) + TF( Has( Chr( 200 ), "b" ) ) + TF( Has( Chr( 200 ), " a" ) )
   c += TF( Has( Chr( 200 ), ( "a" + Chr( 0 ) ) ) ) + TF( Has( Chr( 200 ), Chr( 0 ) ) )
   c += TF( Has( Chr( 200 ), Chr( 200 ) ) ) + TF( Has( Chr( 200 ), "aB" ) )
   ? c
   c := TF( Has( "aB", "" ) ) + TF( Has( "aB", " " ) ) + TF( Has( "aB", "a" ) ) + TF( Has( "aB", "A" ) )
   c += TF( Has( "aB", "ab" ) ) + TF( Has( "aB", "ab " ) ) + TF( Has( "aB", "abc" ) ) + TF( Has( "aB", "b" ) )
   c += TF( Has( "aB", " a" ) ) + TF( Has( "aB", ( "a" + Chr( 0 ) ) ) ) + TF( Has( "aB", Chr( 0 ) ) )
   c += TF( Has( "aB", Chr( 200 ) ) ) + TF( Has( "aB", "aB" ) )
   ? c
   ? "runtime $"
   Table( aVal, {| a, b | a $ b } )
   ? "macro $"
   Table( aText, {| a, b | &( a + " $ " + b ) } )
   ? "input $"
   Table( aIn, {| a, b | a $ b } )
RETURN

STATIC PROCEDURE ShowEq( aVal, aText, aIn )
   LOCAL c
   ? "literal ="
   c := TF( "" = "" ) + TF( "" = " " ) + TF( "" = "a" ) + TF( "" = "A" ) + TF( "" = "ab" ) + TF( "" = "ab " )
   c += TF( "" = "abc" ) + TF( "" = "b" ) + TF( "" = " a" ) + TF( "" = ( "a" + Chr( 0 ) ) ) + TF( "" = Chr( 0 ) )
   c += TF( "" = Chr( 200 ) ) + TF( "" = "aB" )
   ? c
   c := TF( " " = "" ) + TF( " " = " " ) + TF( " " = "a" ) + TF( " " = "A" ) + TF( " " = "ab" ) + TF( " " = "ab " )
   c += TF( " " = "abc" ) + TF( " " = "b" ) + TF( " " = " a" ) + TF( " " = ( "a" + Chr( 0 ) ) ) + TF( " " = Chr( 0 ) )
   c += TF( " " = Chr( 200 ) ) + TF( " " = "aB" )
   ? c
   c := TF( "a" = "" ) + TF( "a" = " " ) + TF( "a" = "a" ) + TF( "a" = "A" ) + TF( "a" = "ab" ) + TF( "a" = "ab " )
   c += TF( "a" = "abc" ) + TF( "a" = "b" ) + TF( "a" = " a" ) + TF( "a" = ( "a" + Chr( 0 ) ) ) + TF( "a" = Chr( 0 ) )
   c += TF( "a" = Chr( 200 ) ) + TF( "a" = "aB" )
   ? c
   c := TF( "A" = "" ) + TF( "A" = " " ) + TF( "A" = "a" ) + TF( "A" = "A" ) + TF( "A" = "ab" ) + TF( "A" = "ab " )
   c += TF( "A" = "abc" ) + TF( "A" = "b" ) + TF( "A" = " a" ) + TF( "A" = ( "a" + Chr( 0 ) ) ) + TF( "A" = Chr( 0 ) )
   c += TF( "A" = Chr( 200 ) ) + TF( "A" = "aB" )
   ? c
   c := TF( "ab" = "" ) + TF( "ab" = " " ) + TF( "ab" = "a" ) + TF( "ab" = "A" ) + TF( "ab" = "ab" )
   c += TF( "ab" = "ab " ) + TF( "ab" = "abc" ) + TF( "ab" = "b" ) + TF( "ab" = " a" ) + TF( "ab" = ( "a" + Chr( 0 ) ) )
   c += TF( "ab" = Chr( 0 ) ) + TF( "ab" = Chr( 200 ) ) + TF( "ab" = "aB" )
   ? c
   c := TF( "ab " = "" ) + TF( "ab " = " " ) + TF( "ab " = "a" ) + TF( "ab " = "A" ) + TF( "ab " = "ab" )
   c += TF( "ab " = "ab " ) + TF( "ab " = "abc" ) + TF( "ab " = "b" ) + TF( "ab " = " a" )
   c += TF( "ab " = ( "a" + Chr( 0 ) ) ) + TF( "ab " = Chr( 0 ) ) + TF( "ab " = Chr( 200 ) ) + TF( "ab " = "aB" )
   ? c
   c := TF( "abc" = "" ) + TF( "abc" = " " ) + TF( "abc" = "a" ) + TF( "abc" = "A" ) + TF( "abc" = "ab" )
   c += TF( "abc" = "ab " ) + TF( "abc" = "abc" ) + TF( "abc" = "b" ) + TF( "abc" = " a" )
   c += TF( "abc" = ( "a" + Chr( 0 ) ) ) + TF( "abc" = Chr( 0 ) ) + TF( "abc" = Chr( 200 ) ) + TF( "abc" = "aB" )
   ? c
   c := TF( "b" = "" ) + TF( "b" = " " ) + TF( "b" = "a" ) + TF( "b" = "A" ) + TF( "b" = "ab" ) + TF( "b" = "ab " )
   c += TF( "b" = "abc" ) + TF( "b" = "b" ) + TF( "b" = " a" ) + TF( "b" = ( "a" + Chr( 0 ) ) ) + TF( "b" = Chr( 0 ) )
   c += TF( "b" = Chr( 200 ) ) + TF( "b" = "aB" )
   ? c
   c := TF( " a" = "" ) + TF( " a" = " " ) + TF( " a" = "a" ) + TF( " a" = "A" ) + TF( " a" = "ab" )
   c += TF( " a" = "ab " ) + TF( " a" = "abc" ) + TF( " a" = "b" ) + TF( " a" = " a" ) + TF( " a" = ( "a" + Chr( 0 ) ) )
   c += TF( " a" = Chr( 0 ) ) + TF( " a" = Chr( 200 ) ) + TF( " a" = "aB" )
   ? c
   c := TF( ( "a" + Chr( 0 ) ) = "" ) + TF( ( "a" + Chr( 0 ) ) = " " ) + TF( ( "a" + Chr( 0 ) ) = "a" )
   c += TF( ( "a" + Chr( 0 ) ) = "A" ) + TF( ( "a" + Chr( 0 ) ) = "ab" ) + TF( ( "a" + Chr( 0 ) ) = "ab " )
   c += TF( ( "a" + Chr( 0 ) ) = "abc" ) + TF( ( "a" + Chr( 0 ) ) = "b" ) + TF( ( "a" + Chr( 0 ) ) = " a" )
   c += TF( ( "a" + Chr( 0 ) ) = ( "a" + Chr( 0 ) ) ) + TF( ( "a" + Chr( 0 ) ) = Chr( 0 ) )
   c += TF( ( "a" + Chr( 0 ) ) = Chr( 200 ) ) + TF( ( "a" + Chr( 0 ) ) = "aB" )
   ? c
   c := TF( Chr( 0 ) = "" ) + TF( Chr( 0 ) = " " ) + TF( Chr( 0 ) = "a" ) + TF( Chr( 0 ) = "A" ) + TF( Chr( 0 ) = "ab" )
   c += TF( Chr( 0 ) = "ab " ) + TF( Chr( 0 ) = "abc" ) + TF( Chr( 0 ) = "b" ) + TF( Chr( 0 ) = " a" )
   c += TF( Chr( 0 ) = ( "a" + Chr( 0 ) ) ) + TF( Chr( 0 ) = Chr( 0 ) ) + TF( Chr( 0 ) = Chr( 200 ) )
   c += TF( Chr( 0 ) = "aB" )
   ? c
   c := TF( Chr( 200 ) = "" ) + TF( Chr( 200 ) = " " ) + TF( Chr( 200 ) = "a" ) + TF( Chr( 200 ) = "A" )
   c += TF( Chr( 200 ) = "ab" ) + TF( Chr( 200 ) = "ab " ) + TF( Chr( 200 ) = "abc" ) + TF( Chr( 200 ) = "b" )
   c += TF( Chr( 200 ) = " a" ) + TF( Chr( 200 ) = ( "a" + Chr( 0 ) ) ) + TF( Chr( 200 ) = Chr( 0 ) )
   c += TF( Chr( 200 ) = Chr( 200 ) ) + TF( Chr( 200 ) = "aB" )
   ? c
   c := TF( "aB" = "" ) + TF( "aB" = " " ) + TF( "aB" = "a" ) + TF( "aB" = "A" ) + TF( "aB" = "ab" )
   c += TF( "aB" = "ab " ) + TF( "aB" = "abc" ) + TF( "aB" = "b" ) + TF( "aB" = " a" ) + TF( "aB" = ( "a" + Chr( 0 ) ) )
   c += TF( "aB" = Chr( 0 ) ) + TF( "aB" = Chr( 200 ) ) + TF( "aB" = "aB" )
   ? c
   ? "inlined ="
   c := TF( Eq( "", "" ) ) + TF( Eq( "", " " ) ) + TF( Eq( "", "a" ) ) + TF( Eq( "", "A" ) ) + TF( Eq( "", "ab" ) )
   c += TF( Eq( "", "ab " ) ) + TF( Eq( "", "abc" ) ) + TF( Eq( "", "b" ) ) + TF( Eq( "", " a" ) )
   c += TF( Eq( "", ( "a" + Chr( 0 ) ) ) ) + TF( Eq( "", Chr( 0 ) ) ) + TF( Eq( "", Chr( 200 ) ) )
   c += TF( Eq( "", "aB" ) )
   ? c
   c := TF( Eq( " ", "" ) ) + TF( Eq( " ", " " ) ) + TF( Eq( " ", "a" ) ) + TF( Eq( " ", "A" ) ) + TF( Eq( " ", "ab" ) )
   c += TF( Eq( " ", "ab " ) ) + TF( Eq( " ", "abc" ) ) + TF( Eq( " ", "b" ) ) + TF( Eq( " ", " a" ) )
   c += TF( Eq( " ", ( "a" + Chr( 0 ) ) ) ) + TF( Eq( " ", Chr( 0 ) ) ) + TF( Eq( " ", Chr( 200 ) ) )
   c += TF( Eq( " ", "aB" ) )
   ? c
   c := TF( Eq( "a", "" ) ) + TF( Eq( "a", " " ) ) + TF( Eq( "a", "a" ) ) + TF( Eq( "a", "A" ) ) + TF( Eq( "a", "ab" ) )
   c += TF( Eq( "a", "ab " ) ) + TF( Eq( "a", "abc" ) ) + TF( Eq( "a", "b" ) ) + TF( Eq( "a", " a" ) )
   c += TF( Eq( "a", ( "a" + Chr( 0 ) ) ) ) + TF( Eq( "a", Chr( 0 ) ) ) + TF( Eq( "a", Chr( 200 ) ) )
   c += TF( Eq( "a", "aB" ) )
   ? c
   c := TF( Eq( "A", "" ) ) + TF( Eq( "A", " " ) ) + TF( Eq( "A", "a" ) ) + TF( Eq( "A", "A" ) ) + TF( Eq( "A", "ab" ) )
   c += TF( Eq( "A", "ab " ) ) + TF( Eq( "A", "abc" ) ) + TF( Eq( "A", "b" ) ) + TF( Eq( "A", " a" ) )
   c += TF( Eq( "A", ( "a" + Chr( 0 ) ) ) ) + TF( Eq( "A", Chr( 0 ) ) ) + TF( Eq( "A", Chr( 200 ) ) )
   c += TF( Eq( "A", "aB" ) )
   ? c
   c := TF( Eq( "ab", "" ) ) + TF( Eq( "ab", " " ) ) + TF( Eq( "ab", "a" ) ) + TF( Eq( "ab", "A" ) )
   c += TF( Eq( "ab", "ab" ) ) + TF( Eq( "ab", "ab " ) ) + TF( Eq( "ab", "abc" ) ) + TF( Eq( "ab", "b" ) )
   c += TF( Eq( "ab", " a" ) ) + TF( Eq( "ab", ( "a" + Chr( 0 ) ) ) ) + TF( Eq( "ab", Chr( 0 ) ) )
   c += TF( Eq( "ab", Chr( 200 ) ) ) + TF( Eq( "ab", "aB" ) )
   ? c
   c := TF( Eq( "ab ", "" ) ) + TF( Eq( "ab ", " " ) ) + TF( Eq( "ab ", "a" ) ) + TF( Eq( "ab ", "A" ) )
   c += TF( Eq( "ab ", "ab" ) ) + TF( Eq( "ab ", "ab " ) ) + TF( Eq( "ab ", "abc" ) ) + TF( Eq( "ab ", "b" ) )
   c += TF( Eq( "ab ", " a" ) ) + TF( Eq( "ab ", ( "a" + Chr( 0 ) ) ) ) + TF( Eq( "ab ", Chr( 0 ) ) )
   c += TF( Eq( "ab ", Chr( 200 ) ) ) + TF( Eq( "ab ", "aB" ) )
   ? c
   c := TF( Eq( "abc", "" ) ) + TF( Eq( "abc", " " ) ) + TF( Eq( "abc", "a" ) ) + TF( Eq( "abc", "A" ) )
   c += TF( Eq( "abc", "ab" ) ) + TF( Eq( "abc", "ab " ) ) + TF( Eq( "abc", "abc" ) ) + TF( Eq( "abc", "b" ) )
   c += TF( Eq( "abc", " a" ) ) + TF( Eq( "abc", ( "a" + Chr( 0 ) ) ) ) + TF( Eq( "abc", Chr( 0 ) ) )
   c += TF( Eq( "abc", Chr( 200 ) ) ) + TF( Eq( "abc", "aB" ) )
   ? c
   c := TF( Eq( "b", "" ) ) + TF( Eq( "b", " " ) ) + TF( Eq( "b", "a" ) ) + TF( Eq( "b", "A" ) ) + TF( Eq( "b", "ab" ) )
   c += TF( Eq( "b", "ab " ) ) + TF( Eq( "b", "abc" ) ) + TF( Eq( "b", "b" ) ) + TF( Eq( "b", " a" ) )
   c += TF( Eq( "b", ( "a" + Chr( 0 ) ) ) ) + TF( Eq( "b", Chr( 0 ) ) ) + TF( Eq( "b", Chr( 200 ) ) )
   c += TF( Eq( "b", "aB" ) )
   ? c
   c := TF( Eq( " a", "" ) ) + TF( Eq( " a", " " ) ) + TF( Eq( " a", "a" ) ) + TF( Eq( " a", "A" ) )
   c += TF( Eq( " a", "ab" ) ) + TF( Eq( " a", "ab " ) ) + TF( Eq( " a", "abc" ) ) + TF( Eq( " a", "b" ) )
   c += TF( Eq( " a", " a" ) ) + TF( Eq( " a", ( "a" + Chr( 0 ) ) ) ) + TF( Eq( " a", Chr( 0 ) ) )
   c += TF( Eq( " a", Chr( 200 ) ) ) + TF( Eq( " a", "aB" ) )
   ? c
   c := TF( Eq( ( "a" + Chr( 0 ) ), "" ) ) + TF( Eq( ( "a" + Chr( 0 ) ), " " ) ) + TF( Eq( ( "a" + Chr( 0 ) ), "a" ) )
   c += TF( Eq( ( "a" + Chr( 0 ) ), "A" ) ) + TF( Eq( ( "a" + Chr( 0 ) ), "ab" ) )
   c += TF( Eq( ( "a" + Chr( 0 ) ), "ab " ) ) + TF( Eq( ( "a" + Chr( 0 ) ), "abc" ) )
   c += TF( Eq( ( "a" + Chr( 0 ) ), "b" ) ) + TF( Eq( ( "a" + Chr( 0 ) ), " a" ) )
   c += TF( Eq( ( "a" + Chr( 0 ) ), ( "a" + Chr( 0 ) ) ) ) + TF( Eq( ( "a" + Chr( 0 ) ), Chr( 0 ) ) )
   c += TF( Eq( ( "a" + Chr( 0 ) ), Chr( 200 ) ) ) + TF( Eq( ( "a" + Chr( 0 ) ), "aB" ) )
   ? c
   c := TF( Eq( Chr( 0 ), "" ) ) + TF( Eq( Chr( 0 ), " " ) ) + TF( Eq( Chr( 0 ), "a" ) ) + TF( Eq( Chr( 0 ), "A" ) )
   c += TF( Eq( Chr( 0 ), "ab" ) ) + TF( Eq( Chr( 0 ), "ab " ) ) + TF( Eq( Chr( 0 ), "abc" ) )
   c += TF( Eq( Chr( 0 ), "b" ) ) + TF( Eq( Chr( 0 ), " a" ) ) + TF( Eq( Chr( 0 ), ( "a" + Chr( 0 ) ) ) )
   c += TF( Eq( Chr( 0 ), Chr( 0 ) ) ) + TF( Eq( Chr( 0 ), Chr( 200 ) ) ) + TF( Eq( Chr( 0 ), "aB" ) )
   ? c
   c := TF( Eq( Chr( 200 ), "" ) ) + TF( Eq( Chr( 200 ), " " ) ) + TF( Eq( Chr( 200 ), "a" ) )
   c += TF( Eq( Chr( 200 ), "A" ) ) + TF( Eq( Chr( 200 ), "ab" ) ) + TF( Eq( Chr( 200 ), "ab " ) )
   c += TF( Eq( Chr( 200 ), "abc" ) ) + TF( Eq( Chr( 200 ), "b" ) ) + TF( Eq( Chr( 200 ), " a" ) )
   c += TF( Eq( Chr( 200 ), ( "a" + Chr( 0 ) ) ) ) + TF( Eq( Chr( 200 ), Chr( 0 ) ) )
   c += TF( Eq( Chr( 200 ), Chr( 200 ) ) ) + TF( Eq( Chr( 200 ), "aB" ) )
   ? c
   c := TF( Eq( "aB", "" ) ) + TF( Eq( "aB", " " ) ) + TF( Eq( "aB", "a" ) ) + TF( Eq( "aB", "A" ) )
   c += TF( Eq( "aB", "ab" ) ) + TF( Eq( "aB", "ab " ) ) + TF( Eq( "aB", "abc" ) ) + TF( Eq( "aB", "b" ) )
   c += TF( Eq( "aB", " a" ) ) + TF( Eq( "aB", ( "a" + Chr( 0 ) ) ) ) + TF( Eq( "aB", Chr( 0 ) ) )
   c += TF( Eq( "aB", Chr( 200 ) ) ) + TF( Eq( "aB", "aB" ) )
   ? c
   ? "runtime ="
   Table( aVal, {| a, b | a = b } )
   ? "macro ="
   Table( aText, {| a, b | &( a + " = " + b ) } )
   ? "input ="
   Table( aIn, {| a, b | a = b } )
RETURN

STATIC PROCEDURE ShowNe( aVal, aText, aIn )
   LOCAL c
   ? "literal !="
   c := TF( "" != "" ) + TF( "" != " " ) + TF( "" != "a" ) + TF( "" != "A" ) + TF( "" != "ab" ) + TF( "" != "ab " )
   c += TF( "" != "abc" ) + TF( "" != "b" ) + TF( "" != " a" ) + TF( "" != ( "a" + Chr( 0 ) ) ) + TF( "" != Chr( 0 ) )
   c += TF( "" != Chr( 200 ) ) + TF( "" != "aB" )
   ? c
   c := TF( " " != "" ) + TF( " " != " " ) + TF( " " != "a" ) + TF( " " != "A" ) + TF( " " != "ab" )
   c += TF( " " != "ab " ) + TF( " " != "abc" ) + TF( " " != "b" ) + TF( " " != " a" ) + TF( " " != ( "a" + Chr( 0 ) ) )
   c += TF( " " != Chr( 0 ) ) + TF( " " != Chr( 200 ) ) + TF( " " != "aB" )
   ? c
   c := TF( "a" != "" ) + TF( "a" != " " ) + TF( "a" != "a" ) + TF( "a" != "A" ) + TF( "a" != "ab" )
   c += TF( "a" != "ab " ) + TF( "a" != "abc" ) + TF( "a" != "b" ) + TF( "a" != " a" ) + TF( "a" != ( "a" + Chr( 0 ) ) )
   c += TF( "a" != Chr( 0 ) ) + TF( "a" != Chr( 200 ) ) + TF( "a" != "aB" )
   ? c
   c := TF( "A" != "" ) + TF( "A" != " " ) + TF( "A" != "a" ) + TF( "A" != "A" ) + TF( "A" != "ab" )
   c += TF( "A" != "ab " ) + TF( "A" != "abc" ) + TF( "A" != "b" ) + TF( "A" != " a" ) + TF( "A" != ( "a" + Chr( 0 ) ) )
   c += TF( "A" != Chr( 0 ) ) + TF( "A" != Chr( 200 ) ) + TF( "A" != "aB" )
   ? c
   c := TF( "ab" != "" ) + TF( "ab" != " " ) + TF( "ab" != "a" ) + TF( "ab" != "A" ) + TF( "ab" != "ab" )
   c += TF( "ab" != "ab " ) + TF( "ab" != "abc" ) + TF( "ab" != "b" ) + TF( "ab" != " a" )
   c += TF( "ab" != ( "a" + Chr( 0 ) ) ) + TF( "ab" != Chr( 0 ) ) + TF( "ab" != Chr( 200 ) ) + TF( "ab" != "aB" )
   ? c
   c := TF( "ab " != "" ) + TF( "ab " != " " ) + TF( "ab " != "a" ) + TF( "ab " != "A" ) + TF( "ab " != "ab" )
   c += TF( "ab " != "ab " ) + TF( "ab " != "abc" ) + TF( "ab " != "b" ) + TF( "ab " != " a" )
   c += TF( "ab " != ( "a" + Chr( 0 ) ) ) + TF( "ab " != Chr( 0 ) ) + TF( "ab " != Chr( 200 ) ) + TF( "ab " != "aB" )
   ? c
   c := TF( "abc" != "" ) + TF( "abc" != " " ) + TF( "abc" != "a" ) + TF( "abc" != "A" ) + TF( "abc" != "ab" )
   c += TF( "abc" != "ab " ) + TF( "abc" != "abc" ) + TF( "abc" != "b" ) + TF( "abc" != " a" )
   c += TF( "abc" != ( "a" + Chr( 0 ) ) ) + TF( "abc" != Chr( 0 ) ) + TF( "abc" != Chr( 200 ) ) + TF( "abc" != "aB" )
   ? c
   c := TF( "b" != "" ) + TF( "b" != " " ) + TF( "b" != "a" ) + TF( "b" != "A" ) + TF( "b" != "ab" )
   c += TF( "b" != "ab " ) + TF( "b" != "abc" ) + TF( "b" != "b" ) + TF( "b" != " a" ) + TF( "b" != ( "a" + Chr( 0 ) ) )
   c += TF( "b" != Chr( 0 ) ) + TF( "b" != Chr( 200 ) ) + TF( "b" != "aB" )
   ? c
   c := TF( " a" != "" ) + TF( " a" != " " ) + TF( " a" != "a" ) + TF( " a" != "A" ) + TF( " a" != "ab" )
   c += TF( " a" != "ab " ) + TF( " a" != "abc" ) + TF( " a" != "b" ) + TF( " a" != " a" )
   c += TF( " a" != ( "a" + Chr( 0 ) ) ) + TF( " a" != Chr( 0 ) ) + TF( " a" != Chr( 200 ) ) + TF( " a" != "aB" )
   ? c
   c := TF( ( "a" + Chr( 0 ) ) != "" ) + TF( ( "a" + Chr( 0 ) ) != " " ) + TF( ( "a" + Chr( 0 ) ) != "a" )
   c += TF( ( "a" + Chr( 0 ) ) != "A" ) + TF( ( "a" + Chr( 0 ) ) != "ab" ) + TF( ( "a" + Chr( 0 ) ) != "ab " )
   c += TF( ( "a" + Chr( 0 ) ) != "abc" ) + TF( ( "a" + Chr( 0 ) ) != "b" ) + TF( ( "a" + Chr( 0 ) ) != " a" )
   c += TF( ( "a" + Chr( 0 ) ) != ( "a" + Chr( 0 ) ) ) + TF( ( "a" + Chr( 0 ) ) != Chr( 0 ) )
   c += TF( ( "a" + Chr( 0 ) ) != Chr( 200 ) ) + TF( ( "a" + Chr( 0 ) ) != "aB" )
   ? c
   c := TF( Chr( 0 ) != "" ) + TF( Chr( 0 ) != " " ) + TF( Chr( 0 ) != "a" ) + TF( Chr( 0 ) != "A" )
   c += TF( Chr( 0 ) != "ab" ) + TF( Chr( 0 ) != "ab " ) + TF( Chr( 0 ) != "abc" ) + TF( Chr( 0 ) != "b" )
   c += TF( Chr( 0 ) != " a" ) + TF( Chr( 0 ) != ( "a" + Chr( 0 ) ) ) + TF( Chr( 0 ) != Chr( 0 ) )
   c += TF( Chr( 0 ) != Chr( 200 ) ) + TF( Chr( 0 ) != "aB" )
   ? c
   c := TF( Chr( 200 ) != "" ) + TF( Chr( 200 ) != " " ) + TF( Chr( 200 ) != "a" ) + TF( Chr( 200 ) != "A" )
   c += TF( Chr( 200 ) != "ab" ) + TF( Chr( 200 ) != "ab " ) + TF( Chr( 200 ) != "abc" ) + TF( Chr( 200 ) != "b" )
   c += TF( Chr( 200 ) != " a" ) + TF( Chr( 200 ) != ( "a" + Chr( 0 ) ) ) + TF( Chr( 200 ) != Chr( 0 ) )
   c += TF( Chr( 200 ) != Chr( 200 ) ) + TF( Chr( 200 ) != "aB" )
   ? c
   c := TF( "aB" != "" ) + TF( "aB" != " " ) + TF( "aB" != "a" ) + TF( "aB" != "A" ) + TF( "aB" != "ab" )
   c += TF( "aB" != "ab " ) + TF( "aB" != "abc" ) + TF( "aB" != "b" ) + TF( "aB" != " a" )
   c += TF( "aB" != ( "a" + Chr( 0 ) ) ) + TF( "aB" != Chr( 0 ) ) + TF( "aB" != Chr( 200 ) ) + TF( "aB" != "aB" )
   ? c
   ? "inlined !="
   c := TF( Ne( "", "" ) ) + TF( Ne( "", " " ) ) + TF( Ne( "", "a" ) ) + TF( Ne( "", "A" ) ) + TF( Ne( "", "ab" ) )
   c += TF( Ne( "", "ab " ) ) + TF( Ne( "", "abc" ) ) + TF( Ne( "", "b" ) ) + TF( Ne( "", " a" ) )
   c += TF( Ne( "", ( "a" + Chr( 0 ) ) ) ) + TF( Ne( "", Chr( 0 ) ) ) + TF( Ne( "", Chr( 200 ) ) )
   c += TF( Ne( "", "aB" ) )
   ? c
   c := TF( Ne( " ", "" ) ) + TF( Ne( " ", " " ) ) + TF( Ne( " ", "a" ) ) + TF( Ne( " ", "A" ) ) + TF( Ne( " ", "ab" ) )
   c += TF( Ne( " ", "ab " ) ) + TF( Ne( " ", "abc" ) ) + TF( Ne( " ", "b" ) ) + TF( Ne( " ", " a" ) )
   c += TF( Ne( " ", ( "a" + Chr( 0 ) ) ) ) + TF( Ne( " ", Chr( 0 ) ) ) + TF( Ne( " ", Chr( 200 ) ) )
   c += TF( Ne( " ", "aB" ) )
   ? c
   c := TF( Ne( "a", "" ) ) + TF( Ne( "a", " " ) ) + TF( Ne( "a", "a" ) ) + TF( Ne( "a", "A" ) ) + TF( Ne( "a", "ab" ) )
   c += TF( Ne( "a", "ab " ) ) + TF( Ne( "a", "abc" ) ) + TF( Ne( "a", "b" ) ) + TF( Ne( "a", " a" ) )
   c += TF( Ne( "a", ( "a" + Chr( 0 ) ) ) ) + TF( Ne( "a", Chr( 0 ) ) ) + TF( Ne( "a", Chr( 200 ) ) )
   c += TF( Ne( "a", "aB" ) )
   ? c
   c := TF( Ne( "A", "" ) ) + TF( Ne( "A", " " ) ) + TF( Ne( "A", "a" ) ) + TF( Ne( "A", "A" ) ) + TF( Ne( "A", "ab" ) )
   c += TF( Ne( "A", "ab " ) ) + TF( Ne( "A", "abc" ) ) + TF( Ne( "A", "b" ) ) + TF( Ne( "A", " a" ) )
   c += TF( Ne( "A", ( "a" + Chr( 0 ) ) ) ) + TF( Ne( "A", Chr( 0 ) ) ) + TF( Ne( "A", Chr( 200 ) ) )
   c += TF( Ne( "A", "aB" ) )
   ? c
   c := TF( Ne( "ab", "" ) ) + TF( Ne( "ab", " " ) ) + TF( Ne( "ab", "a" ) ) + TF( Ne( "ab", "A" ) )
   c += TF( Ne( "ab", "ab" ) ) + TF( Ne( "ab", "ab " ) ) + TF( Ne( "ab", "abc" ) ) + TF( Ne( "ab", "b" ) )
   c += TF( Ne( "ab", " a" ) ) + TF( Ne( "ab", ( "a" + Chr( 0 ) ) ) ) + TF( Ne( "ab", Chr( 0 ) ) )
   c += TF( Ne( "ab", Chr( 200 ) ) ) + TF( Ne( "ab", "aB" ) )
   ? c
   c := TF( Ne( "ab ", "" ) ) + TF( Ne( "ab ", " " ) ) + TF( Ne( "ab ", "a" ) ) + TF( Ne( "ab ", "A" ) )
   c += TF( Ne( "ab ", "ab" ) ) + TF( Ne( "ab ", "ab " ) ) + TF( Ne( "ab ", "abc" ) ) + TF( Ne( "ab ", "b" ) )
   c += TF( Ne( "ab ", " a" ) ) + TF( Ne( "ab ", ( "a" + Chr( 0 ) ) ) ) + TF( Ne( "ab ", Chr( 0 ) ) )
   c += TF( Ne( "ab ", Chr( 200 ) ) ) + TF( Ne( "ab ", "aB" ) )
   ? c
   c := TF( Ne( "abc", "" ) ) + TF( Ne( "abc", " " ) ) + TF( Ne( "abc", "a" ) ) + TF( Ne( "abc", "A" ) )
   c += TF( Ne( "abc", "ab" ) ) + TF( Ne( "abc", "ab " ) ) + TF( Ne( "abc", "abc" ) ) + TF( Ne( "abc", "b" ) )
   c += TF( Ne( "abc", " a" ) ) + TF( Ne( "abc", ( "a" + Chr( 0 ) ) ) ) + TF( Ne( "abc", Chr( 0 ) ) )
   c += TF( Ne( "abc", Chr( 200 ) ) ) + TF( Ne( "abc", "aB" ) )
   ? c
   c := TF( Ne( "b", "" ) ) + TF( Ne( "b", " " ) ) + TF( Ne( "b", "a" ) ) + TF( Ne( "b", "A" ) ) + TF( Ne( "b", "ab" ) )
   c += TF( Ne( "b", "ab " ) ) + TF( Ne( "b", "abc" ) ) + TF( Ne( "b", "b" ) ) + TF( Ne( "b", " a" ) )
   c += TF( Ne( "b", ( "a" + Chr( 0 ) ) ) ) + TF( Ne( "b", Chr( 0 ) ) ) + TF( Ne( "b", Chr( 200 ) ) )
   c += TF( Ne( "b", "aB" ) )
   ? c
   c := TF( Ne( " a", "" ) ) + TF( Ne( " a", " " ) ) + TF( Ne( " a", "a" ) ) + TF( Ne( " a", "A" ) )
   c += TF( Ne( " a", "ab" ) ) + TF( Ne( " a", "ab " ) ) + TF( Ne( " a", "abc" ) ) + TF( Ne( " a", "b" ) )
   c += TF( Ne( " a", " a" ) ) + TF( Ne( " a", ( "a" + Chr( 0 ) ) ) ) + TF( Ne( " a", Chr( 0 ) ) )
   c += TF( Ne( " a", Chr( 200 ) ) ) + TF( Ne( " a", "aB" ) )
   ? c
   c := TF( Ne( ( "a" + Chr( 0 ) ), "" ) ) + TF( Ne( ( "a" + Chr( 0 ) ), " " ) ) + TF( Ne( ( "a" + Chr( 0 ) ), "a" ) )
   c += TF( Ne( ( "a" + Chr( 0 ) ), "A" ) ) + TF( Ne( ( "a" + Chr( 0 ) ), "ab" ) )
   c += TF( Ne( ( "a" + Chr( 0 ) ), "ab " ) ) + TF( Ne( ( "a" + Chr( 0 ) ), "abc" ) )
   c += TF( Ne( ( "a" + Chr( 0 ) ), "b" ) ) + TF( Ne( ( "a" + Chr( 0 ) ), " a" ) )
   c += TF( Ne( ( "a" + Chr( 0 ) ), ( "a" + Chr( 0 ) ) ) ) + TF( Ne( ( "a" + Chr( 0 ) ), Chr( 0 ) ) )
   c += TF( Ne( ( "a" + Chr( 0 ) ), Chr( 200 ) ) ) + TF( Ne( ( "a" + Chr( 0 ) ), "aB" ) )
   ? c
   c := TF( Ne( Chr( 0 ), "" ) ) + TF( Ne( Chr( 0 ), " " ) ) + TF( Ne( Chr( 0 ), "a" ) ) + TF( Ne( Chr( 0 ), "A" ) )
   c += TF( Ne( Chr( 0 ), "ab" ) ) + TF( Ne( Chr( 0 ), "ab " ) ) + TF( Ne( Chr( 0 ), "abc" ) )
   c += TF( Ne( Chr( 0 ), "b" ) ) + TF( Ne( Chr( 0 ), " a" ) ) + TF( Ne( Chr( 0 ), ( "a" + Chr( 0 ) ) ) )
   c += TF( Ne( Chr( 0 ), Chr( 0 ) ) ) + TF( Ne( Chr( 0 ), Chr( 200 ) ) ) + TF( Ne( Chr( 0 ), "aB" ) )
   ? c
   c := TF( Ne( Chr( 200 ), "" ) ) + TF( Ne( Chr( 200 ), " " ) ) + TF( Ne( Chr( 200 ), "a" ) )
   c += TF( Ne( Chr( 200 ), "A" ) ) + TF( Ne( Chr( 200 ), "ab" ) ) + TF( Ne( Chr( 200 ), "ab " ) )
   c += TF( Ne( Chr( 200 ), "abc" ) ) + TF( Ne( Chr( 200 ), "b" ) ) + TF( Ne( Chr( 200 ), " a" ) )
   c += TF( Ne( Chr( 200 ), ( "a" + Chr( 0 ) ) ) ) + TF( Ne( Chr( 200 ), Chr( 0 ) ) )
   c += TF( Ne( Chr( 200 ), Chr( 200 ) ) ) + TF( Ne( Chr( 200 ), "aB" ) )
   ? c
   c := TF( Ne( "aB", "" ) ) + TF( Ne( "aB", " " ) ) + TF( Ne( "aB", "a" ) ) + TF( Ne( "aB", "A" ) )
   c += TF( Ne( "aB", "ab" ) ) + TF( Ne( "aB", "ab " ) ) + TF( Ne( "aB", "abc" ) ) + TF( Ne( "aB", "b" ) )
   c += TF( Ne( "aB", " a" ) ) + TF( Ne( "aB", ( "a" + Chr( 0 ) ) ) ) + TF( Ne( "aB", Chr( 0 ) ) )
   c += TF( Ne( "aB", Chr( 200 ) ) ) + TF( Ne( "aB", "aB" ) )
   ? c
   ? "runtime !="
   Table( aVal, {| a, b | a != b } )
   ? "macro !="
   Table( aText, {| a, b | &( a + " != " + b ) } )
   ? "input !="
   Table( aIn, {| a, b | a != b } )
RETURN

STATIC PROCEDURE ShowLt( aVal, aText, aIn )
   LOCAL c
   ? "literal <"
   c := TF( "" < "" ) + TF( "" < " " ) + TF( "" < "a" ) + TF( "" < "A" ) + TF( "" < "ab" ) + TF( "" < "ab " )
   c += TF( "" < "abc" ) + TF( "" < "b" ) + TF( "" < " a" ) + TF( "" < ( "a" + Chr( 0 ) ) ) + TF( "" < Chr( 0 ) )
   c += TF( "" < Chr( 200 ) ) + TF( "" < "aB" )
   ? c
   c := TF( " " < "" ) + TF( " " < " " ) + TF( " " < "a" ) + TF( " " < "A" ) + TF( " " < "ab" ) + TF( " " < "ab " )
   c += TF( " " < "abc" ) + TF( " " < "b" ) + TF( " " < " a" ) + TF( " " < ( "a" + Chr( 0 ) ) ) + TF( " " < Chr( 0 ) )
   c += TF( " " < Chr( 200 ) ) + TF( " " < "aB" )
   ? c
   c := TF( "a" < "" ) + TF( "a" < " " ) + TF( "a" < "a" ) + TF( "a" < "A" ) + TF( "a" < "ab" ) + TF( "a" < "ab " )
   c += TF( "a" < "abc" ) + TF( "a" < "b" ) + TF( "a" < " a" ) + TF( "a" < ( "a" + Chr( 0 ) ) ) + TF( "a" < Chr( 0 ) )
   c += TF( "a" < Chr( 200 ) ) + TF( "a" < "aB" )
   ? c
   c := TF( "A" < "" ) + TF( "A" < " " ) + TF( "A" < "a" ) + TF( "A" < "A" ) + TF( "A" < "ab" ) + TF( "A" < "ab " )
   c += TF( "A" < "abc" ) + TF( "A" < "b" ) + TF( "A" < " a" ) + TF( "A" < ( "a" + Chr( 0 ) ) ) + TF( "A" < Chr( 0 ) )
   c += TF( "A" < Chr( 200 ) ) + TF( "A" < "aB" )
   ? c
   c := TF( "ab" < "" ) + TF( "ab" < " " ) + TF( "ab" < "a" ) + TF( "ab" < "A" ) + TF( "ab" < "ab" )
   c += TF( "ab" < "ab " ) + TF( "ab" < "abc" ) + TF( "ab" < "b" ) + TF( "ab" < " a" ) + TF( "ab" < ( "a" + Chr( 0 ) ) )
   c += TF( "ab" < Chr( 0 ) ) + TF( "ab" < Chr( 200 ) ) + TF( "ab" < "aB" )
   ? c
   c := TF( "ab " < "" ) + TF( "ab " < " " ) + TF( "ab " < "a" ) + TF( "ab " < "A" ) + TF( "ab " < "ab" )
   c += TF( "ab " < "ab " ) + TF( "ab " < "abc" ) + TF( "ab " < "b" ) + TF( "ab " < " a" )
   c += TF( "ab " < ( "a" + Chr( 0 ) ) ) + TF( "ab " < Chr( 0 ) ) + TF( "ab " < Chr( 200 ) ) + TF( "ab " < "aB" )
   ? c
   c := TF( "abc" < "" ) + TF( "abc" < " " ) + TF( "abc" < "a" ) + TF( "abc" < "A" ) + TF( "abc" < "ab" )
   c += TF( "abc" < "ab " ) + TF( "abc" < "abc" ) + TF( "abc" < "b" ) + TF( "abc" < " a" )
   c += TF( "abc" < ( "a" + Chr( 0 ) ) ) + TF( "abc" < Chr( 0 ) ) + TF( "abc" < Chr( 200 ) ) + TF( "abc" < "aB" )
   ? c
   c := TF( "b" < "" ) + TF( "b" < " " ) + TF( "b" < "a" ) + TF( "b" < "A" ) + TF( "b" < "ab" ) + TF( "b" < "ab " )
   c += TF( "b" < "abc" ) + TF( "b" < "b" ) + TF( "b" < " a" ) + TF( "b" < ( "a" + Chr( 0 ) ) ) + TF( "b" < Chr( 0 ) )
   c += TF( "b" < Chr( 200 ) ) + TF( "b" < "aB" )
   ? c
   c := TF( " a" < "" ) + TF( " a" < " " ) + TF( " a" < "a" ) + TF( " a" < "A" ) + TF( " a" < "ab" )
   c += TF( " a" < "ab " ) + TF( " a" < "abc" ) + TF( " a" < "b" ) + TF( " a" < " a" ) + TF( " a" < ( "a" + Chr( 0 ) ) )
   c += TF( " a" < Chr( 0 ) ) + TF( " a" < Chr( 200 ) ) + TF( " a" < "aB" )
   ? c
   c := TF( ( "a" + Chr( 0 ) ) < "" ) + TF( ( "a" + Chr( 0 ) ) < " " ) + TF( ( "a" + Chr( 0 ) ) < "a" )
   c += TF( ( "a" + Chr( 0 ) ) < "A" ) + TF( ( "a" + Chr( 0 ) ) < "ab" ) + TF( ( "a" + Chr( 0 ) ) < "ab " )
   c += TF( ( "a" + Chr( 0 ) ) < "abc" ) + TF( ( "a" + Chr( 0 ) ) < "b" ) + TF( ( "a" + Chr( 0 ) ) < " a" )
   c += TF( ( "a" + Chr( 0 ) ) < ( "a" + Chr( 0 ) ) ) + TF( ( "a" + Chr( 0 ) ) < Chr( 0 ) )
   c += TF( ( "a" + Chr( 0 ) ) < Chr( 200 ) ) + TF( ( "a" + Chr( 0 ) ) < "aB" )
   ? c
   c := TF( Chr( 0 ) < "" ) + TF( Chr( 0 ) < " " ) + TF( Chr( 0 ) < "a" ) + TF( Chr( 0 ) < "A" ) + TF( Chr( 0 ) < "ab" )
   c += TF( Chr( 0 ) < "ab " ) + TF( Chr( 0 ) < "abc" ) + TF( Chr( 0 ) < "b" ) + TF( Chr( 0 ) < " a" )
   c += TF( Chr( 0 ) < ( "a" + Chr( 0 ) ) ) + TF( Chr( 0 ) < Chr( 0 ) ) + TF( Chr( 0 ) < Chr( 200 ) )
   c += TF( Chr( 0 ) < "aB" )
   ? c
   c := TF( Chr( 200 ) < "" ) + TF( Chr( 200 ) < " " ) + TF( Chr( 200 ) < "a" ) + TF( Chr( 200 ) < "A" )
   c += TF( Chr( 200 ) < "ab" ) + TF( Chr( 200 ) < "ab " ) + TF( Chr( 200 ) < "abc" ) + TF( Chr( 200 ) < "b" )
   c += TF( Chr( 200 ) < " a" ) + TF( Chr( 200 ) < ( "a" + Chr( 0 ) ) ) + TF( Chr( 200 ) < Chr( 0 ) )
   c += TF( Chr( 200 ) < Chr( 200 ) ) + TF( Chr( 200 ) < "aB" )
   ? c
   c := TF( "aB" < "" ) + TF( "aB" < " " ) + TF( "aB" < "a" ) + TF( "aB" < "A" ) + TF( "aB" < "ab" )
   c += TF( "aB" < "ab " ) + TF( "aB" < "abc" ) + TF( "aB" < "b" ) + TF( "aB" < " a" ) + TF( "aB" < ( "a" + Chr( 0 ) ) )
   c += TF( "aB" < Chr( 0 ) ) + TF( "aB" < Chr( 200 ) ) + TF( "aB" < "aB" )
   ? c
   ? "inlined <"
   c := TF( Lt( "", "" ) ) + TF( Lt( "", " " ) ) + TF( Lt( "", "a" ) ) + TF( Lt( "", "A" ) ) + TF( Lt( "", "ab" ) )
   c += TF( Lt( "", "ab " ) ) + TF( Lt( "", "abc" ) ) + TF( Lt( "", "b" ) ) + TF( Lt( "", " a" ) )
   c += TF( Lt( "", ( "a" + Chr( 0 ) ) ) ) + TF( Lt( "", Chr( 0 ) ) ) + TF( Lt( "", Chr( 200 ) ) )
   c += TF( Lt( "", "aB" ) )
   ? c
   c := TF( Lt( " ", "" ) ) + TF( Lt( " ", " " ) ) + TF( Lt( " ", "a" ) ) + TF( Lt( " ", "A" ) ) + TF( Lt( " ", "ab" ) )
   c += TF( Lt( " ", "ab " ) ) + TF( Lt( " ", "abc" ) ) + TF( Lt( " ", "b" ) ) + TF( Lt( " ", " a" ) )
   c += TF( Lt( " ", ( "a" + Chr( 0 ) ) ) ) + TF( Lt( " ", Chr( 0 ) ) ) + TF( Lt( " ", Chr( 200 ) ) )
   c += TF( Lt( " ", "aB" ) )
   ? c
   c := TF( Lt( "a", "" ) ) + TF( Lt( "a", " " ) ) + TF( Lt( "a", "a" ) ) + TF( Lt( "a", "A" ) ) + TF( Lt( "a", "ab" ) )
   c += TF( Lt( "a", "ab " ) ) + TF( Lt( "a", "abc" ) ) + TF( Lt( "a", "b" ) ) + TF( Lt( "a", " a" ) )
   c += TF( Lt( "a", ( "a" + Chr( 0 ) ) ) ) + TF( Lt( "a", Chr( 0 ) ) ) + TF( Lt( "a", Chr( 200 ) ) )
   c += TF( Lt( "a", "aB" ) )
   ? c
   c := TF( Lt( "A", "" ) ) + TF( Lt( "A", " " ) ) + TF( Lt( "A", "a" ) ) + TF( Lt( "A", "A" ) ) + TF( Lt( "A", "ab" ) )
   c += TF( Lt( "A", "ab " ) ) + TF( Lt( "A", "abc" ) ) + TF( Lt( "A", "b" ) ) + TF( Lt( "A", " a" ) )
   c += TF( Lt( "A", ( "a" + Chr( 0 ) ) ) ) + TF( Lt( "A", Chr( 0 ) ) ) + TF( Lt( "A", Chr( 200 ) ) )
   c += TF( Lt( "A", "aB" ) )
   ? c
   c := TF( Lt( "ab", "" ) ) + TF( Lt( "ab", " " ) ) + TF( Lt( "ab", "a" ) ) + TF( Lt( "ab", "A" ) )
   c += TF( Lt( "ab", "ab" ) ) + TF( Lt( "ab", "ab " ) ) + TF( Lt( "ab", "abc" ) ) + TF( Lt( "ab", "b" ) )
   c += TF( Lt( "ab", " a" ) ) + TF( Lt( "ab", ( "a" + Chr( 0 ) ) ) ) + TF( Lt( "ab", Chr( 0 ) ) )
   c += TF( Lt( "ab", Chr( 200 ) ) ) + TF( Lt( "ab", "aB" ) )
   ? c
   c := TF( Lt( "ab ", "" ) ) + TF( Lt( "ab ", " " ) ) + TF( Lt( "ab ", "a" ) ) + TF( Lt( "ab ", "A" ) )
   c += TF( Lt( "ab ", "ab" ) ) + TF( Lt( "ab ", "ab " ) ) + TF( Lt( "ab ", "abc" ) ) + TF( Lt( "ab ", "b" ) )
   c += TF( Lt( "ab ", " a" ) ) + TF( Lt( "ab ", ( "a" + Chr( 0 ) ) ) ) + TF( Lt( "ab ", Chr( 0 ) ) )
   c += TF( Lt( "ab ", Chr( 200 ) ) ) + TF( Lt( "ab ", "aB" ) )
   ? c
   c := TF( Lt( "abc", "" ) ) + TF( Lt( "abc", " " ) ) + TF( Lt( "abc", "a" ) ) + TF( Lt( "abc", "A" ) )
   c += TF( Lt( "abc", "ab" ) ) + TF( Lt( "abc", "ab " ) ) + TF( Lt( "abc", "abc" ) ) + TF( Lt( "abc", "b" ) )
   c += TF( Lt( "abc", " a" ) ) + TF( Lt( "abc", ( "a" + Chr( 0 ) ) ) ) + TF( Lt( "abc", Chr( 0 ) ) )
   c += TF( Lt( "abc", Chr( 200 ) ) ) + TF( Lt( "abc", "aB" ) )
   ? c
   c := TF( Lt( "b", "" ) ) + TF( Lt( "b", " " ) ) + TF( Lt( "b", "a" ) ) + TF( Lt( "b", "A" ) ) + TF( Lt( "b", "ab" ) )
   c += TF( Lt( "b", "ab " ) ) + TF( Lt( "b", "abc" ) ) + TF( Lt( "b", "b" ) ) + TF( Lt( "b", " a" ) )
   c += TF( Lt( "b", ( "a" + Chr( 0 ) ) ) ) + TF( Lt( "b", Chr( 0 ) ) ) + TF( Lt( "b", Chr( 200 ) ) )
   c += TF( Lt( "b", "aB" ) )
   ? c
   c := TF( Lt( " a", "" ) ) + TF( Lt( " a", " " ) ) + TF( Lt( " a", "a" ) ) + TF( Lt( " a", "A" ) )
   c += TF( Lt( " a", "ab" ) ) + TF( Lt( " a", "ab " ) ) + TF( Lt( " a", "abc" ) ) + TF( Lt( " a", "b" ) )
   c += TF( Lt( " a", " a" ) ) + TF( Lt( " a", ( "a" + Chr( 0 ) ) ) ) + TF( Lt( " a", Chr( 0 ) ) )
   c += TF( Lt( " a", Chr( 200 ) ) ) + TF( Lt( " a", "aB" ) )
   ? c
   c := TF( Lt( ( "a" + Chr( 0 ) ), "" ) ) + TF( Lt( ( "a" + Chr( 0 ) ), " " ) ) + TF( Lt( ( "a" + Chr( 0 ) ), "a" ) )
   c += TF( Lt( ( "a" + Chr( 0 ) ), "A" ) ) + TF( Lt( ( "a" + Chr( 0 ) ), "ab" ) )
   c += TF( Lt( ( "a" + Chr( 0 ) ), "ab " ) ) + TF( Lt( ( "a" + Chr( 0 ) ), "abc" ) )
   c += TF( Lt( ( "a" + Chr( 0 ) ), "b" ) ) + TF( Lt( ( "a" + Chr( 0 ) ), " a" ) )
   c += TF( Lt( ( "a" + Chr( 0 ) ), ( "a" + Chr( 0 ) ) ) ) + TF( Lt( ( "a" + Chr( 0 ) ), Chr( 0 ) ) )
   c += TF( Lt( ( "a" + Chr( 0 ) ), Chr( 200 ) ) ) + TF( Lt( ( "a" + Chr( 0 ) ), "aB" ) )
   ? c
   c := TF( Lt( Chr( 0 ), "" ) ) + TF( Lt( Chr( 0 ), " " ) ) + TF( Lt( Chr( 0 ), "a" ) ) + TF( Lt( Chr( 0 ), "A" ) )
   c += TF( Lt( Chr( 0 ), "ab" ) ) + TF( Lt( Chr( 0 ), "ab " ) ) + TF( Lt( Chr( 0 ), "abc" ) )
   c += TF( Lt( Chr( 0 ), "b" ) ) + TF( Lt( Chr( 0 ), " a" ) ) + TF( Lt( Chr( 0 ), ( "a" + Chr( 0 ) ) ) )
   c += TF( Lt( Chr( 0 ), Chr( 0 ) ) ) + TF( Lt( Chr( 0 ), Chr( 200 ) ) ) + TF( Lt( Chr( 0 ), "aB" ) )
   ? c
   c := TF( Lt( Chr( 200 ), "" ) ) + TF( Lt( Chr( 200 ), " " ) ) + TF( Lt( Chr( 200 ), "a" ) )
   c += TF( Lt( Chr( 200 ), "A" ) ) + TF( Lt( Chr( 200 ), "ab" ) ) + TF( Lt( Chr( 200 ), "ab " ) )
   c += TF( Lt( Chr( 200 ), "abc" ) ) + TF( Lt( Chr( 200 ), "b" ) ) + TF( Lt( Chr( 200 ), " a" ) )
   c += TF( Lt( Chr( 200 ), ( "a" + Chr( 0 ) ) ) ) + TF( Lt( Chr( 200 ), Chr( 0 ) ) )
   c += TF( Lt( Chr( 200 ), Chr( 200 ) ) ) + TF( Lt( Chr( 200 ), "aB" ) )
   ? c
   c := TF( Lt( "aB", "" ) ) + TF( Lt( "aB", " " ) ) + TF( Lt( "aB", "a" ) ) + TF( Lt( "aB", "A" ) )
   c += TF( Lt( "aB", "ab" ) ) + TF( Lt( "aB", "ab " ) ) + TF( Lt( "aB", "abc" ) ) + TF( Lt( "aB", "b" ) )
   c += TF( Lt( "aB", " a" ) ) + TF( Lt( "aB", ( "a" + Chr( 0 ) ) ) ) + TF( Lt( "aB", Chr( 0 ) ) )
   c += TF( Lt( "aB", Chr( 200 ) ) ) + TF( Lt( "aB", "aB" ) )
   ? c
   ? "runtime <"
   Table( aVal, {| a, b | a < b } )
   ? "macro <"
   Table( aText, {| a, b | &( a + " < " + b ) } )
   ? "input <"
   Table( aIn, {| a, b | a < b } )
RETURN

STATIC PROCEDURE ShowLe( aVal, aText, aIn )
   LOCAL c
   ? "literal <="
   c := TF( "" <= "" ) + TF( "" <= " " ) + TF( "" <= "a" ) + TF( "" <= "A" ) + TF( "" <= "ab" ) + TF( "" <= "ab " )
   c += TF( "" <= "abc" ) + TF( "" <= "b" ) + TF( "" <= " a" ) + TF( "" <= ( "a" + Chr( 0 ) ) ) + TF( "" <= Chr( 0 ) )
   c += TF( "" <= Chr( 200 ) ) + TF( "" <= "aB" )
   ? c
   c := TF( " " <= "" ) + TF( " " <= " " ) + TF( " " <= "a" ) + TF( " " <= "A" ) + TF( " " <= "ab" )
   c += TF( " " <= "ab " ) + TF( " " <= "abc" ) + TF( " " <= "b" ) + TF( " " <= " a" ) + TF( " " <= ( "a" + Chr( 0 ) ) )
   c += TF( " " <= Chr( 0 ) ) + TF( " " <= Chr( 200 ) ) + TF( " " <= "aB" )
   ? c
   c := TF( "a" <= "" ) + TF( "a" <= " " ) + TF( "a" <= "a" ) + TF( "a" <= "A" ) + TF( "a" <= "ab" )
   c += TF( "a" <= "ab " ) + TF( "a" <= "abc" ) + TF( "a" <= "b" ) + TF( "a" <= " a" ) + TF( "a" <= ( "a" + Chr( 0 ) ) )
   c += TF( "a" <= Chr( 0 ) ) + TF( "a" <= Chr( 200 ) ) + TF( "a" <= "aB" )
   ? c
   c := TF( "A" <= "" ) + TF( "A" <= " " ) + TF( "A" <= "a" ) + TF( "A" <= "A" ) + TF( "A" <= "ab" )
   c += TF( "A" <= "ab " ) + TF( "A" <= "abc" ) + TF( "A" <= "b" ) + TF( "A" <= " a" ) + TF( "A" <= ( "a" + Chr( 0 ) ) )
   c += TF( "A" <= Chr( 0 ) ) + TF( "A" <= Chr( 200 ) ) + TF( "A" <= "aB" )
   ? c
   c := TF( "ab" <= "" ) + TF( "ab" <= " " ) + TF( "ab" <= "a" ) + TF( "ab" <= "A" ) + TF( "ab" <= "ab" )
   c += TF( "ab" <= "ab " ) + TF( "ab" <= "abc" ) + TF( "ab" <= "b" ) + TF( "ab" <= " a" )
   c += TF( "ab" <= ( "a" + Chr( 0 ) ) ) + TF( "ab" <= Chr( 0 ) ) + TF( "ab" <= Chr( 200 ) ) + TF( "ab" <= "aB" )
   ? c
   c := TF( "ab " <= "" ) + TF( "ab " <= " " ) + TF( "ab " <= "a" ) + TF( "ab " <= "A" ) + TF( "ab " <= "ab" )
   c += TF( "ab " <= "ab " ) + TF( "ab " <= "abc" ) + TF( "ab " <= "b" ) + TF( "ab " <= " a" )
   c += TF( "ab " <= ( "a" + Chr( 0 ) ) ) + TF( "ab " <= Chr( 0 ) ) + TF( "ab " <= Chr( 200 ) ) + TF( "ab " <= "aB" )
   ? c
   c := TF( "abc" <= "" ) + TF( "abc" <= " " ) + TF( "abc" <= "a" ) + TF( "abc" <= "A" ) + TF( "abc" <= "ab" )
   c += TF( "abc" <= "ab " ) + TF( "abc" <= "abc" ) + TF( "abc" <= "b" ) + TF( "abc" <= " a" )
   c += TF( "abc" <= ( "a" + Chr( 0 ) ) ) + TF( "abc" <= Chr( 0 ) ) + TF( "abc" <= Chr( 200 ) ) + TF( "abc" <= "aB" )
   ? c
   c := TF( "b" <= "" ) + TF( "b" <= " " ) + TF( "b" <= "a" ) + TF( "b" <= "A" ) + TF( "b" <= "ab" )
   c += TF( "b" <= "ab " ) + TF( "b" <= "abc" ) + TF( "b" <= "b" ) + TF( "b" <= " a" ) + TF( "b" <= ( "a" + Chr( 0 ) ) )
   c += TF( "b" <= Chr( 0 ) ) + TF( "b" <= Chr( 200 ) ) + TF( "b" <= "aB" )
   ? c
   c := TF( " a" <= "" ) + TF( " a" <= " " ) + TF( " a" <= "a" ) + TF( " a" <= "A" ) + TF( " a" <= "ab" )
   c += TF( " a" <= "ab " ) + TF( " a" <= "abc" ) + TF( " a" <= "b" ) + TF( " a" <= " a" )
   c += TF( " a" <= ( "a" + Chr( 0 ) ) ) + TF( " a" <= Chr( 0 ) ) + TF( " a" <= Chr( 200 ) ) + TF( " a" <= "aB" )
   ? c
   c := TF( ( "a" + Chr( 0 ) ) <= "" ) + TF( ( "a" + Chr( 0 ) ) <= " " ) + TF( ( "a" + Chr( 0 ) ) <= "a" )
   c += TF( ( "a" + Chr( 0 ) ) <= "A" ) + TF( ( "a" + Chr( 0 ) ) <= "ab" ) + TF( ( "a" + Chr( 0 ) ) <= "ab " )
   c += TF( ( "a" + Chr( 0 ) ) <= "abc" ) + TF( ( "a" + Chr( 0 ) ) <= "b" ) + TF( ( "a" + Chr( 0 ) ) <= " a" )
   c += TF( ( "a" + Chr( 0 ) ) <= ( "a" + Chr( 0 ) ) ) + TF( ( "a" + Chr( 0 ) ) <= Chr( 0 ) )
   c += TF( ( "a" + Chr( 0 ) ) <= Chr( 200 ) ) + TF( ( "a" + Chr( 0 ) ) <= "aB" )
   ? c
   c := TF( Chr( 0 ) <= "" ) + TF( Chr( 0 ) <= " " ) + TF( Chr( 0 ) <= "a" ) + TF( Chr( 0 ) <= "A" )
   c += TF( Chr( 0 ) <= "ab" ) + TF( Chr( 0 ) <= "ab " ) + TF( Chr( 0 ) <= "abc" ) + TF( Chr( 0 ) <= "b" )
   c += TF( Chr( 0 ) <= " a" ) + TF( Chr( 0 ) <= ( "a" + Chr( 0 ) ) ) + TF( Chr( 0 ) <= Chr( 0 ) )
   c += TF( Chr( 0 ) <= Chr( 200 ) ) + TF( Chr( 0 ) <= "aB" )
   ? c
   c := TF( Chr( 200 ) <= "" ) + TF( Chr( 200 ) <= " " ) + TF( Chr( 200 ) <= "a" ) + TF( Chr( 200 ) <= "A" )
   c += TF( Chr( 200 ) <= "ab" ) + TF( Chr( 200 ) <= "ab " ) + TF( Chr( 200 ) <= "abc" ) + TF( Chr( 200 ) <= "b" )
   c += TF( Chr( 200 ) <= " a" ) + TF( Chr( 200 ) <= ( "a" + Chr( 0 ) ) ) + TF( Chr( 200 ) <= Chr( 0 ) )
   c += TF( Chr( 200 ) <= Chr( 200 ) ) + TF( Chr( 200 ) <= "aB" )
   ? c
   c := TF( "aB" <= "" ) + TF( "aB" <= " " ) + TF( "aB" <= "a" ) + TF( "aB" <= "A" ) + TF( "aB" <= "ab" )
   c += TF( "aB" <= "ab " ) + TF( "aB" <= "abc" ) + TF( "aB" <= "b" ) + TF( "aB" <= " a" )
   c += TF( "aB" <= ( "a" + Chr( 0 ) ) ) + TF( "aB" <= Chr( 0 ) ) + TF( "aB" <= Chr( 200 ) ) + TF( "aB" <= "aB" )
   ? c
   ? "inlined <="
   c := TF( Le( "", "" ) ) + TF( Le( "", " " ) ) + TF( Le( "", "a" ) ) + TF( Le( "", "A" ) ) + TF( Le( "", "ab" ) )
   c += TF( Le( "", "ab " ) ) + TF( Le( "", "abc" ) ) + TF( Le( "", "b" ) ) + TF( Le( "", " a" ) )
   c += TF( Le( "", ( "a" + Chr( 0 ) ) ) ) + TF( Le( "", Chr( 0 ) ) ) + TF( Le( "", Chr( 200 ) ) )
   c += TF( Le( "", "aB" ) )
   ? c
   c := TF( Le( " ", "" ) ) + TF( Le( " ", " " ) ) + TF( Le( " ", "a" ) ) + TF( Le( " ", "A" ) ) + TF( Le( " ", "ab" ) )
   c += TF( Le( " ", "ab " ) ) + TF( Le( " ", "abc" ) ) + TF( Le( " ", "b" ) ) + TF( Le( " ", " a" ) )
   c += TF( Le( " ", ( "a" + Chr( 0 ) ) ) ) + TF( Le( " ", Chr( 0 ) ) ) + TF( Le( " ", Chr( 200 ) ) )
   c += TF( Le( " ", "aB" ) )
   ? c
   c := TF( Le( "a", "" ) ) + TF( Le( "a", " " ) ) + TF( Le( "a", "a" ) ) + TF( Le( "a", "A" ) ) + TF( Le( "a", "ab" ) )
   c += TF( Le( "a", "ab " ) ) + TF( Le( "a", "abc" ) ) + TF( Le( "a", "b" ) ) + TF( Le( "a", " a" ) )
   c += TF( Le( "a", ( "a" + Chr( 0 ) ) ) ) + TF( Le( "a", Chr( 0 ) ) ) + TF( Le( "a", Chr( 200 ) ) )
   c += TF( Le( "a", "aB" ) )
   ? c
   c := TF( Le( "A", "" ) ) + TF( Le( "A", " " ) ) + TF( Le( "A", "a" ) ) + TF( Le( "A", "A" ) ) + TF( Le( "A", "ab" ) )
   c += TF( Le( "A", "ab " ) ) + TF( Le( "A", "abc" ) ) + TF( Le( "A", "b" ) ) + TF( Le( "A", " a" ) )
   c += TF( Le( "A", ( "a" + Chr( 0 ) ) ) ) + TF( Le( "A", Chr( 0 ) ) ) + TF( Le( "A", Chr( 200 ) ) )
   c += TF( Le( "A", "aB" ) )
   ? c
   c := TF( Le( "ab", "" ) ) + TF( Le( "ab", " " ) ) + TF( Le( "ab", "a" ) ) + TF( Le( "ab", "A" ) )
   c += TF( Le( "ab", "ab" ) ) + TF( Le( "ab", "ab " ) ) + TF( Le( "ab", "abc" ) ) + TF( Le( "ab", "b" ) )
   c += TF( Le( "ab", " a" ) ) + TF( Le( "ab", ( "a" + Chr( 0 ) ) ) ) + TF( Le( "ab", Chr( 0 ) ) )
   c += TF( Le( "ab", Chr( 200 ) ) ) + TF( Le( "ab", "aB" ) )
   ? c
   c := TF( Le( "ab ", "" ) ) + TF( Le( "ab ", " " ) ) + TF( Le( "ab ", "a" ) ) + TF( Le( "ab ", "A" ) )
   c += TF( Le( "ab ", "ab" ) ) + TF( Le( "ab ", "ab " ) ) + TF( Le( "ab ", "abc" ) ) + TF( Le( "ab ", "b" ) )
   c += TF( Le( "ab ", " a" ) ) + TF( Le( "ab ", ( "a" + Chr( 0 ) ) ) ) + TF( Le( "ab ", Chr( 0 ) ) )
   c += TF( Le( "ab ", Chr( 200 ) ) ) + TF( Le( "ab ", "aB" ) )
   ? c
   c := TF( Le( "abc", "" ) ) + TF( Le( "abc", " " ) ) + TF( Le( "abc", "a" ) ) + TF( Le( "abc", "A" ) )
   c += TF( Le( "abc", "ab" ) ) + TF( Le( "abc", "ab " ) ) + TF( Le( "abc", "abc" ) ) + TF( Le( "abc", "b" ) )
   c += TF( Le( "abc", " a" ) ) + TF( Le( "abc", ( "a" + Chr( 0 ) ) ) ) + TF( Le( "abc", Chr( 0 ) ) )
   c += TF( Le( "abc", Chr( 200 ) ) ) + TF( Le( "abc", "aB" ) )
   ? c
   c := TF( Le( "b", "" ) ) + TF( Le( "b", " " ) ) + TF( Le( "b", "a" ) ) + TF( Le( "b", "A" ) ) + TF( Le( "b", "ab" ) )
   c += TF( Le( "b", "ab " ) ) + TF( Le( "b", "abc" ) ) + TF( Le( "b", "b" ) ) + TF( Le( "b", " a" ) )
   c += TF( Le( "b", ( "a" + Chr( 0 ) ) ) ) + TF( Le( "b", Chr( 0 ) ) ) + TF( Le( "b", Chr( 200 ) ) )
   c += TF( Le( "b", "aB" ) )
   ? c
   c := TF( Le( " a", "" ) ) + TF( Le( " a", " " ) ) + TF( Le( " a", "a" ) ) + TF( Le( " a", "A" ) )
   c += TF( Le( " a", "ab" ) ) + TF( Le( " a", "ab " ) ) + TF( Le( " a", "abc" ) ) + TF( Le( " a", "b" ) )
   c += TF( Le( " a", " a" ) ) + TF( Le( " a", ( "a" + Chr( 0 ) ) ) ) + TF( Le( " a", Chr( 0 ) ) )
   c += TF( Le( " a", Chr( 200 ) ) ) + TF( Le( " a", "aB" ) )
   ? c
   c := TF( Le( ( "a" + Chr( 0 ) ), "" ) ) + TF( Le( ( "a" + Chr( 0 ) ), " " ) ) + TF( Le( ( "a" + Chr( 0 ) ), "a" ) )
   c += TF( Le( ( "a" + Chr( 0 ) ), "A" ) ) + TF( Le( ( "a" + Chr( 0 ) ), "ab" ) )
   c += TF( Le( ( "a" + Chr( 0 ) ), "ab " ) ) + TF( Le( ( "a" + Chr( 0 ) ), "abc" ) )
   c += TF( Le( ( "a" + Chr( 0 ) ), "b" ) ) + TF( Le( ( "a" + Chr( 0 ) ), " a" ) )
   c += TF( Le( ( "a" + Chr( 0 ) ), ( "a" + Chr( 0 ) ) ) ) + TF( Le( ( "a" + Chr( 0 ) ), Chr( 0 ) ) )
   c += TF( Le( ( "a" + Chr( 0 ) ), Chr( 200 ) ) ) + TF( Le( ( "a" + Chr( 0 ) ), "aB" ) )
   ? c
   c := TF( Le( Chr( 0 ), "" ) ) + TF( Le( Chr( 0 ), " " ) ) + TF( Le( Chr( 0 ), "a" ) ) + TF( Le( Chr( 0 ), "A" ) )
   c += TF( Le( Chr( 0 ), "ab" ) ) + TF( Le( Chr( 0 ), "ab " ) ) + TF( Le( Chr( 0 ), "abc" ) )
   c += TF( Le( Chr( 0 ), "b" ) ) + TF( Le( Chr( 0 ), " a" ) ) + TF( Le( Chr( 0 ), ( "a" + Chr( 0 ) ) ) )
   c += TF( Le( Chr( 0 ), Chr( 0 ) ) ) + TF( Le( Chr( 0 ), Chr( 200 ) ) ) + TF( Le( Chr( 0 ), "aB" ) )
   ? c
   c := TF( Le( Chr( 200 ), "" ) ) + TF( Le( Chr( 200 ), " " ) ) + TF( Le( Chr( 200 ), "a" ) )
   c += TF( Le( Chr( 200 ), "A" ) ) + TF( Le( Chr( 200 ), "ab" ) ) + TF( Le( Chr( 200 ), "ab " ) )
   c += TF( Le( Chr( 200 ), "abc" ) ) + TF( Le( Chr( 200 ), "b" ) ) + TF( Le( Chr( 200 ), " a" ) )
   c += TF( Le( Chr( 200 ), ( "a" + Chr( 0 ) ) ) ) + TF( Le( Chr( 200 ), Chr( 0 ) ) )
   c += TF( Le( Chr( 200 ), Chr( 200 ) ) ) + TF( Le( Chr( 200 ), "aB" ) )
   ? c
   c := TF( Le( "aB", "" ) ) + TF( Le( "aB", " " ) ) + TF( Le( "aB", "a" ) ) + TF( Le( "aB", "A" ) )
   c += TF( Le( "aB", "ab" ) ) + TF( Le( "aB", "ab " ) ) + TF( Le( "aB", "abc" ) ) + TF( Le( "aB", "b" ) )
   c += TF( Le( "aB", " a" ) ) + TF( Le( "aB", ( "a" + Chr( 0 ) ) ) ) + TF( Le( "aB", Chr( 0 ) ) )
   c += TF( Le( "aB", Chr( 200 ) ) ) + TF( Le( "aB", "aB" ) )
   ? c
   ? "runtime <="
   Table( aVal, {| a, b | a <= b } )
   ? "macro <="
   Table( aText, {| a, b | &( a + " <= " + b ) } )
   ? "input <="
   Table( aIn, {| a, b | a <= b } )
RETURN

STATIC PROCEDURE ShowGt( aVal, aText, aIn )
   LOCAL c
   ? "literal >"
   c := TF( "" > "" ) + TF( "" > " " ) + TF( "" > "a" ) + TF( "" > "A" ) + TF( "" > "ab" ) + TF( "" > "ab " )
   c += TF( "" > "abc" ) + TF( "" > "b" ) + TF( "" > " a" ) + TF( "" > ( "a" + Chr( 0 ) ) ) + TF( "" > Chr( 0 ) )
   c += TF( "" > Chr( 200 ) ) + TF( "" > "aB" )
   ? c
   c := TF( " " > "" ) + TF( " " > " " ) + TF( " " > "a" ) + TF( " " > "A" ) + TF( " " > "ab" ) + TF( " " > "ab " )
   c += TF( " " > "abc" ) + TF( " " > "b" ) + TF( " " > " a" ) + TF( " " > ( "a" + Chr( 0 ) ) ) + TF( " " > Chr( 0 ) )
   c += TF( " " > Chr( 200 ) ) + TF( " " > "aB" )
   ? c
   c := TF( "a" > "" ) + TF( "a" > " " ) + TF( "a" > "a" ) + TF( "a" > "A" ) + TF( "a" > "ab" ) + TF( "a" > "ab " )
   c += TF( "a" > "abc" ) + TF( "a" > "b" ) + TF( "a" > " a" ) + TF( "a" > ( "a" + Chr( 0 ) ) ) + TF( "a" > Chr( 0 ) )
   c += TF( "a" > Chr( 200 ) ) + TF( "a" > "aB" )
   ? c
   c := TF( "A" > "" ) + TF( "A" > " " ) + TF( "A" > "a" ) + TF( "A" > "A" ) + TF( "A" > "ab" ) + TF( "A" > "ab " )
   c += TF( "A" > "abc" ) + TF( "A" > "b" ) + TF( "A" > " a" ) + TF( "A" > ( "a" + Chr( 0 ) ) ) + TF( "A" > Chr( 0 ) )
   c += TF( "A" > Chr( 200 ) ) + TF( "A" > "aB" )
   ? c
   c := TF( "ab" > "" ) + TF( "ab" > " " ) + TF( "ab" > "a" ) + TF( "ab" > "A" ) + TF( "ab" > "ab" )
   c += TF( "ab" > "ab " ) + TF( "ab" > "abc" ) + TF( "ab" > "b" ) + TF( "ab" > " a" ) + TF( "ab" > ( "a" + Chr( 0 ) ) )
   c += TF( "ab" > Chr( 0 ) ) + TF( "ab" > Chr( 200 ) ) + TF( "ab" > "aB" )
   ? c
   c := TF( "ab " > "" ) + TF( "ab " > " " ) + TF( "ab " > "a" ) + TF( "ab " > "A" ) + TF( "ab " > "ab" )
   c += TF( "ab " > "ab " ) + TF( "ab " > "abc" ) + TF( "ab " > "b" ) + TF( "ab " > " a" )
   c += TF( "ab " > ( "a" + Chr( 0 ) ) ) + TF( "ab " > Chr( 0 ) ) + TF( "ab " > Chr( 200 ) ) + TF( "ab " > "aB" )
   ? c
   c := TF( "abc" > "" ) + TF( "abc" > " " ) + TF( "abc" > "a" ) + TF( "abc" > "A" ) + TF( "abc" > "ab" )
   c += TF( "abc" > "ab " ) + TF( "abc" > "abc" ) + TF( "abc" > "b" ) + TF( "abc" > " a" )
   c += TF( "abc" > ( "a" + Chr( 0 ) ) ) + TF( "abc" > Chr( 0 ) ) + TF( "abc" > Chr( 200 ) ) + TF( "abc" > "aB" )
   ? c
   c := TF( "b" > "" ) + TF( "b" > " " ) + TF( "b" > "a" ) + TF( "b" > "A" ) + TF( "b" > "ab" ) + TF( "b" > "ab " )
   c += TF( "b" > "abc" ) + TF( "b" > "b" ) + TF( "b" > " a" ) + TF( "b" > ( "a" + Chr( 0 ) ) ) + TF( "b" > Chr( 0 ) )
   c += TF( "b" > Chr( 200 ) ) + TF( "b" > "aB" )
   ? c
   c := TF( " a" > "" ) + TF( " a" > " " ) + TF( " a" > "a" ) + TF( " a" > "A" ) + TF( " a" > "ab" )
   c += TF( " a" > "ab " ) + TF( " a" > "abc" ) + TF( " a" > "b" ) + TF( " a" > " a" ) + TF( " a" > ( "a" + Chr( 0 ) ) )
   c += TF( " a" > Chr( 0 ) ) + TF( " a" > Chr( 200 ) ) + TF( " a" > "aB" )
   ? c
   c := TF( ( "a" + Chr( 0 ) ) > "" ) + TF( ( "a" + Chr( 0 ) ) > " " ) + TF( ( "a" + Chr( 0 ) ) > "a" )
   c += TF( ( "a" + Chr( 0 ) ) > "A" ) + TF( ( "a" + Chr( 0 ) ) > "ab" ) + TF( ( "a" + Chr( 0 ) ) > "ab " )
   c += TF( ( "a" + Chr( 0 ) ) > "abc" ) + TF( ( "a" + Chr( 0 ) ) > "b" ) + TF( ( "a" + Chr( 0 ) ) > " a" )
   c += TF( ( "a" + Chr( 0 ) ) > ( "a" + Chr( 0 ) ) ) + TF( ( "a" + Chr( 0 ) ) > Chr( 0 ) )
   c += TF( ( "a" + Chr( 0 ) ) > Chr( 200 ) ) + TF( ( "a" + Chr( 0 ) ) > "aB" )
   ? c
   c := TF( Chr( 0 ) > "" ) + TF( Chr( 0 ) > " " ) + TF( Chr( 0 ) > "a" ) + TF( Chr( 0 ) > "A" ) + TF( Chr( 0 ) > "ab" )
   c += TF( Chr( 0 ) > "ab " ) + TF( Chr( 0 ) > "abc" ) + TF( Chr( 0 ) > "b" ) + TF( Chr( 0 ) > " a" )
   c += TF( Chr( 0 ) > ( "a" + Chr( 0 ) ) ) + TF( Chr( 0 ) > Chr( 0 ) ) + TF( Chr( 0 ) > Chr( 200 ) )
   c += TF( Chr( 0 ) > "aB" )
   ? c
   c := TF( Chr( 200 ) > "" ) + TF( Chr( 200 ) > " " ) + TF( Chr( 200 ) > "a" ) + TF( Chr( 200 ) > "A" )
   c += TF( Chr( 200 ) > "ab" ) + TF( Chr( 200 ) > "ab " ) + TF( Chr( 200 ) > "abc" ) + TF( Chr( 200 ) > "b" )
   c += TF( Chr( 200 ) > " a" ) + TF( Chr( 200 ) > ( "a" + Chr( 0 ) ) ) + TF( Chr( 200 ) > Chr( 0 ) )
   c += TF( Chr( 200 ) > Chr( 200 ) ) + TF( Chr( 200 ) > "aB" )
   ? c
   c := TF( "aB" > "" ) + TF( "aB" > " " ) + TF( "aB" > "a" ) + TF( "aB" > "A" ) + TF( "aB" > "ab" )
   c += TF( "aB" > "ab " ) + TF( "aB" > "abc" ) + TF( "aB" > "b" ) + TF( "aB" > " a" ) + TF( "aB" > ( "a" + Chr( 0 ) ) )
   c += TF( "aB" > Chr( 0 ) ) + TF( "aB" > Chr( 200 ) ) + TF( "aB" > "aB" )
   ? c
   ? "inlined >"
   c := TF( Gt( "", "" ) ) + TF( Gt( "", " " ) ) + TF( Gt( "", "a" ) ) + TF( Gt( "", "A" ) ) + TF( Gt( "", "ab" ) )
   c += TF( Gt( "", "ab " ) ) + TF( Gt( "", "abc" ) ) + TF( Gt( "", "b" ) ) + TF( Gt( "", " a" ) )
   c += TF( Gt( "", ( "a" + Chr( 0 ) ) ) ) + TF( Gt( "", Chr( 0 ) ) ) + TF( Gt( "", Chr( 200 ) ) )
   c += TF( Gt( "", "aB" ) )
   ? c
   c := TF( Gt( " ", "" ) ) + TF( Gt( " ", " " ) ) + TF( Gt( " ", "a" ) ) + TF( Gt( " ", "A" ) ) + TF( Gt( " ", "ab" ) )
   c += TF( Gt( " ", "ab " ) ) + TF( Gt( " ", "abc" ) ) + TF( Gt( " ", "b" ) ) + TF( Gt( " ", " a" ) )
   c += TF( Gt( " ", ( "a" + Chr( 0 ) ) ) ) + TF( Gt( " ", Chr( 0 ) ) ) + TF( Gt( " ", Chr( 200 ) ) )
   c += TF( Gt( " ", "aB" ) )
   ? c
   c := TF( Gt( "a", "" ) ) + TF( Gt( "a", " " ) ) + TF( Gt( "a", "a" ) ) + TF( Gt( "a", "A" ) ) + TF( Gt( "a", "ab" ) )
   c += TF( Gt( "a", "ab " ) ) + TF( Gt( "a", "abc" ) ) + TF( Gt( "a", "b" ) ) + TF( Gt( "a", " a" ) )
   c += TF( Gt( "a", ( "a" + Chr( 0 ) ) ) ) + TF( Gt( "a", Chr( 0 ) ) ) + TF( Gt( "a", Chr( 200 ) ) )
   c += TF( Gt( "a", "aB" ) )
   ? c
   c := TF( Gt( "A", "" ) ) + TF( Gt( "A", " " ) ) + TF( Gt( "A", "a" ) ) + TF( Gt( "A", "A" ) ) + TF( Gt( "A", "ab" ) )
   c += TF( Gt( "A", "ab " ) ) + TF( Gt( "A", "abc" ) ) + TF( Gt( "A", "b" ) ) + TF( Gt( "A", " a" ) )
   c += TF( Gt( "A", ( "a" + Chr( 0 ) ) ) ) + TF( Gt( "A", Chr( 0 ) ) ) + TF( Gt( "A", Chr( 200 ) ) )
   c += TF( Gt( "A", "aB" ) )
   ? c
   c := TF( Gt( "ab", "" ) ) + TF( Gt( "ab", " " ) ) + TF( Gt( "ab", "a" ) ) + TF( Gt( "ab", "A" ) )
   c += TF( Gt( "ab", "ab" ) ) + TF( Gt( "ab", "ab " ) ) + TF( Gt( "ab", "abc" ) ) + TF( Gt( "ab", "b" ) )
   c += TF( Gt( "ab", " a" ) ) + TF( Gt( "ab", ( "a" + Chr( 0 ) ) ) ) + TF( Gt( "ab", Chr( 0 ) ) )
   c += TF( Gt( "ab", Chr( 200 ) ) ) + TF( Gt( "ab", "aB" ) )
   ? c
   c := TF( Gt( "ab ", "" ) ) + TF( Gt( "ab ", " " ) ) + TF( Gt( "ab ", "a" ) ) + TF( Gt( "ab ", "A" ) )
   c += TF( Gt( "ab ", "ab" ) ) + TF( Gt( "ab ", "ab " ) ) + TF( Gt( "ab ", "abc" ) ) + TF( Gt( "ab ", "b" ) )
   c += TF( Gt( "ab ", " a" ) ) + TF( Gt( "ab ", ( "a" + Chr( 0 ) ) ) ) + TF( Gt( "ab ", Chr( 0 ) ) )
   c += TF( Gt( "ab ", Chr( 200 ) ) ) + TF( Gt( "ab ", "aB" ) )
   ? c
   c := TF( Gt( "abc", "" ) ) + TF( Gt( "abc", " " ) ) + TF( Gt( "abc", "a" ) ) + TF( Gt( "abc", "A" ) )
   c += TF( Gt( "abc", "ab" ) ) + TF( Gt( "abc", "ab " ) ) + TF( Gt( "abc", "abc" ) ) + TF( Gt( "abc", "b" ) )
   c += TF( Gt( "abc", " a" ) ) + TF( Gt( "abc", ( "a" + Chr( 0 ) ) ) ) + TF( Gt( "abc", Chr( 0 ) ) )
   c += TF( Gt( "abc", Chr( 200 ) ) ) + TF( Gt( "abc", "aB" ) )
   ? c
   c := TF( Gt( "b", "" ) ) + TF( Gt( "b", " " ) ) + TF( Gt( "b", "a" ) ) + TF( Gt( "b", "A" ) ) + TF( Gt( "b", "ab" ) )
   c += TF( Gt( "b", "ab " ) ) + TF( Gt( "b", "abc" ) ) + TF( Gt( "b", "b" ) ) + TF( Gt( "b", " a" ) )
   c += TF( Gt( "b", ( "a" + Chr( 0 ) ) ) ) + TF( Gt( "b", Chr( 0 ) ) ) + TF( Gt( "b", Chr( 200 ) ) )
   c += TF( Gt( "b", "aB" ) )
   ? c
   c := TF( Gt( " a", "" ) ) + TF( Gt( " a", " " ) ) + TF( Gt( " a", "a" ) ) + TF( Gt( " a", "A" ) )
   c += TF( Gt( " a", "ab" ) ) + TF( Gt( " a", "ab " ) ) + TF( Gt( " a", "abc" ) ) + TF( Gt( " a", "b" ) )
   c += TF( Gt( " a", " a" ) ) + TF( Gt( " a", ( "a" + Chr( 0 ) ) ) ) + TF( Gt( " a", Chr( 0 ) ) )
   c += TF( Gt( " a", Chr( 200 ) ) ) + TF( Gt( " a", "aB" ) )
   ? c
   c := TF( Gt( ( "a" + Chr( 0 ) ), "" ) ) + TF( Gt( ( "a" + Chr( 0 ) ), " " ) ) + TF( Gt( ( "a" + Chr( 0 ) ), "a" ) )
   c += TF( Gt( ( "a" + Chr( 0 ) ), "A" ) ) + TF( Gt( ( "a" + Chr( 0 ) ), "ab" ) )
   c += TF( Gt( ( "a" + Chr( 0 ) ), "ab " ) ) + TF( Gt( ( "a" + Chr( 0 ) ), "abc" ) )
   c += TF( Gt( ( "a" + Chr( 0 ) ), "b" ) ) + TF( Gt( ( "a" + Chr( 0 ) ), " a" ) )
   c += TF( Gt( ( "a" + Chr( 0 ) ), ( "a" + Chr( 0 ) ) ) ) + TF( Gt( ( "a" + Chr( 0 ) ), Chr( 0 ) ) )
   c += TF( Gt( ( "a" + Chr( 0 ) ), Chr( 200 ) ) ) + TF( Gt( ( "a" + Chr( 0 ) ), "aB" ) )
   ? c
   c := TF( Gt( Chr( 0 ), "" ) ) + TF( Gt( Chr( 0 ), " " ) ) + TF( Gt( Chr( 0 ), "a" ) ) + TF( Gt( Chr( 0 ), "A" ) )
   c += TF( Gt( Chr( 0 ), "ab" ) ) + TF( Gt( Chr( 0 ), "ab " ) ) + TF( Gt( Chr( 0 ), "abc" ) )
   c += TF( Gt( Chr( 0 ), "b" ) ) + TF( Gt( Chr( 0 ), " a" ) ) + TF( Gt( Chr( 0 ), ( "a" + Chr( 0 ) ) ) )
   c += TF( Gt( Chr( 0 ), Chr( 0 ) ) ) + TF( Gt( Chr( 0 ), Chr( 200 ) ) ) + TF( Gt( Chr( 0 ), "aB" ) )
   ? c
   c := TF( Gt( Chr( 200 ), "" ) ) + TF( Gt( Chr( 200 ), " " ) ) + TF( Gt( Chr( 200 ), "a" ) )
   c += TF( Gt( Chr( 200 ), "A" ) ) + TF( Gt( Chr( 200 ), "ab" ) ) + TF( Gt( Chr( 200 ), "ab " ) )
   c += TF( Gt( Chr( 200 ), "abc" ) ) + TF( Gt( Chr( 200 ), "b" ) ) + TF( Gt( Chr( 200 ), " a" ) )
   c += TF( Gt( Chr( 200 ), ( "a" + Chr( 0 ) ) ) ) + TF( Gt( Chr( 200 ), Chr( 0 ) ) )
   c += TF( Gt( Chr( 200 ), Chr( 200 ) ) ) + TF( Gt( Chr( 200 ), "aB" ) )
   ? c
   c := TF( Gt( "aB", "" ) ) + TF( Gt( "aB", " " ) ) + TF( Gt( "aB", "a" ) ) + TF( Gt( "aB", "A" ) )
   c += TF( Gt( "aB", "ab" ) ) + TF( Gt( "aB", "ab " ) ) + TF( Gt( "aB", "abc" ) ) + TF( Gt( "aB", "b" ) )
   c += TF( Gt( "aB", " a" ) ) + TF( Gt( "aB", ( "a" + Chr( 0 ) ) ) ) + TF( Gt( "aB", Chr( 0 ) ) )
   c += TF( Gt( "aB", Chr( 200 ) ) ) + TF( Gt( "aB", "aB" ) )
   ? c
   ? "runtime >"
   Table( aVal, {| a, b | a > b } )
   ? "macro >"
   Table( aText, {| a, b | &( a + " > " + b ) } )
   ? "input >"
   Table( aIn, {| a, b | a > b } )
RETURN

STATIC PROCEDURE ShowGe( aVal, aText, aIn )
   LOCAL c
   ? "literal >="
   c := TF( "" >= "" ) + TF( "" >= " " ) + TF( "" >= "a" ) + TF( "" >= "A" ) + TF( "" >= "ab" ) + TF( "" >= "ab " )
   c += TF( "" >= "abc" ) + TF( "" >= "b" ) + TF( "" >= " a" ) + TF( "" >= ( "a" + Chr( 0 ) ) ) + TF( "" >= Chr( 0 ) )
   c += TF( "" >= Chr( 200 ) ) + TF( "" >= "aB" )
   ? c
   c := TF( " " >= "" ) + TF( " " >= " " ) + TF( " " >= "a" ) + TF( " " >= "A" ) + TF( " " >= "ab" )
   c += TF( " " >= "ab " ) + TF( " " >= "abc" ) + TF( " " >= "b" ) + TF( " " >= " a" ) + TF( " " >= ( "a" + Chr( 0 ) ) )
   c += TF( " " >= Chr( 0 ) ) + TF( " " >= Chr( 200 ) ) + TF( " " >= "aB" )
   ? c
   c := TF( "a" >= "" ) + TF( "a" >= " " ) + TF( "a" >= "a" ) + TF( "a" >= "A" ) + TF( "a" >= "ab" )
   c += TF( "a" >= "ab " ) + TF( "a" >= "abc" ) + TF( "a" >= "b" ) + TF( "a" >= " a" ) + TF( "a" >= ( "a" + Chr( 0 ) ) )
   c += TF( "a" >= Chr( 0 ) ) + TF( "a" >= Chr( 200 ) ) + TF( "a" >= "aB" )
   ? c
   c := TF( "A" >= "" ) + TF( "A" >= " " ) + TF( "A" >= "a" ) + TF( "A" >= "A" ) + TF( "A" >= "ab" )
   c += TF( "A" >= "ab " ) + TF( "A" >= "abc" ) + TF( "A" >= "b" ) + TF( "A" >= " a" ) + TF( "A" >= ( "a" + Chr( 0 ) ) )
   c += TF( "A" >= Chr( 0 ) ) + TF( "A" >= Chr( 200 ) ) + TF( "A" >= "aB" )
   ? c
   c := TF( "ab" >= "" ) + TF( "ab" >= " " ) + TF( "ab" >= "a" ) + TF( "ab" >= "A" ) + TF( "ab" >= "ab" )
   c += TF( "ab" >= "ab " ) + TF( "ab" >= "abc" ) + TF( "ab" >= "b" ) + TF( "ab" >= " a" )
   c += TF( "ab" >= ( "a" + Chr( 0 ) ) ) + TF( "ab" >= Chr( 0 ) ) + TF( "ab" >= Chr( 200 ) ) + TF( "ab" >= "aB" )
   ? c
   c := TF( "ab " >= "" ) + TF( "ab " >= " " ) + TF( "ab " >= "a" ) + TF( "ab " >= "A" ) + TF( "ab " >= "ab" )
   c += TF( "ab " >= "ab " ) + TF( "ab " >= "abc" ) + TF( "ab " >= "b" ) + TF( "ab " >= " a" )
   c += TF( "ab " >= ( "a" + Chr( 0 ) ) ) + TF( "ab " >= Chr( 0 ) ) + TF( "ab " >= Chr( 200 ) ) + TF( "ab " >= "aB" )
   ? c
   c := TF( "abc" >= "" ) + TF( "abc" >= " " ) + TF( "abc" >= "a" ) + TF( "abc" >= "A" ) + TF( "abc" >= "ab" )
   c += TF( "abc" >= "ab " ) + TF( "abc" >= "abc" ) + TF( "abc" >= "b" ) + TF( "abc" >= " a" )
   c += TF( "abc" >= ( "a" + Chr( 0 ) ) ) + TF( "abc" >= Chr( 0 ) ) + TF( "abc" >= Chr( 200 ) ) + TF( "abc" >= "aB" )
   ? c
   c := TF( "b" >= "" ) + TF( "b" >= " " ) + TF( "b" >= "a" ) + TF( "b" >= "A" ) + TF( "b" >= "ab" )
   c += TF( "b" >= "ab " ) + TF( "b" >= "abc" ) + TF( "b" >= "b" ) + TF( "b" >= " a" ) + TF( "b" >= ( "a" + Chr( 0 ) ) )
   c += TF( "b" >= Chr( 0 ) ) + TF( "b" >= Chr( 200 ) ) + TF( "b" >= "aB" )
   ? c
   c := TF( " a" >= "" ) + TF( " a" >= " " ) + TF( " a" >= "a" ) + TF( " a" >= "A" ) + TF( " a" >= "ab" )
   c += TF( " a" >= "ab " ) + TF( " a" >= "abc" ) + TF( " a" >= "b" ) + TF( " a" >= " a" )
   c += TF( " a" >= ( "a" + Chr( 0 ) ) ) + TF( " a" >= Chr( 0 ) ) + TF( " a" >= Chr( 200 ) ) + TF( " a" >= "aB" )
   ? c
   c := TF( ( "a" + Chr( 0 ) ) >= "" ) + TF( ( "a" + Chr( 0 ) ) >= " " ) + TF( ( "a" + Chr( 0 ) ) >= "a" )
   c += TF( ( "a" + Chr( 0 ) ) >= "A" ) + TF( ( "a" + Chr( 0 ) ) >= "ab" ) + TF( ( "a" + Chr( 0 ) ) >= "ab " )
   c += TF( ( "a" + Chr( 0 ) ) >= "abc" ) + TF( ( "a" + Chr( 0 ) ) >= "b" ) + TF( ( "a" + Chr( 0 ) ) >= " a" )
   c += TF( ( "a" + Chr( 0 ) ) >= ( "a" + Chr( 0 ) ) ) + TF( ( "a" + Chr( 0 ) ) >= Chr( 0 ) )
   c += TF( ( "a" + Chr( 0 ) ) >= Chr( 200 ) ) + TF( ( "a" + Chr( 0 ) ) >= "aB" )
   ? c
   c := TF( Chr( 0 ) >= "" ) + TF( Chr( 0 ) >= " " ) + TF( Chr( 0 ) >= "a" ) + TF( Chr( 0 ) >= "A" )
   c += TF( Chr( 0 ) >= "ab" ) + TF( Chr( 0 ) >= "ab " ) + TF( Chr( 0 ) >= "abc" ) + TF( Chr( 0 ) >= "b" )
   c += TF( Chr( 0 ) >= " a" ) + TF( Chr( 0 ) >= ( "a" + Chr( 0 ) ) ) + TF( Chr( 0 ) >= Chr( 0 ) )
   c += TF( Chr( 0 ) >= Chr( 200 ) ) + TF( Chr( 0 ) >= "aB" )
   ? c
   c := TF( Chr( 200 ) >= "" ) + TF( Chr( 200 ) >= " " ) + TF( Chr( 200 ) >= "a" ) + TF( Chr( 200 ) >= "A" )
   c += TF( Chr( 200 ) >= "ab" ) + TF( Chr( 200 ) >= "ab " ) + TF( Chr( 200 ) >= "abc" ) + TF( Chr( 200 ) >= "b" )
   c += TF( Chr( 200 ) >= " a" ) + TF( Chr( 200 ) >= ( "a" + Chr( 0 ) ) ) + TF( Chr( 200 ) >= Chr( 0 ) )
   c += TF( Chr( 200 ) >= Chr( 200 ) ) + TF( Chr( 200 ) >= "aB" )
   ? c
   c := TF( "aB" >= "" ) + TF( "aB" >= " " ) + TF( "aB" >= "a" ) + TF( "aB" >= "A" ) + TF( "aB" >= "ab" )
   c += TF( "aB" >= "ab " ) + TF( "aB" >= "abc" ) + TF( "aB" >= "b" ) + TF( "aB" >= " a" )
   c += TF( "aB" >= ( "a" + Chr( 0 ) ) ) + TF( "aB" >= Chr( 0 ) ) + TF( "aB" >= Chr( 200 ) ) + TF( "aB" >= "aB" )
   ? c
   ? "inlined >="
   c := TF( Ge( "", "" ) ) + TF( Ge( "", " " ) ) + TF( Ge( "", "a" ) ) + TF( Ge( "", "A" ) ) + TF( Ge( "", "ab" ) )
   c += TF( Ge( "", "ab " ) ) + TF( Ge( "", "abc" ) ) + TF( Ge( "", "b" ) ) + TF( Ge( "", " a" ) )
   c += TF( Ge( "", ( "a" + Chr( 0 ) ) ) ) + TF( Ge( "", Chr( 0 ) ) ) + TF( Ge( "", Chr( 200 ) ) )
   c += TF( Ge( "", "aB" ) )
   ? c
   c := TF( Ge( " ", "" ) ) + TF( Ge( " ", " " ) ) + TF( Ge( " ", "a" ) ) + TF( Ge( " ", "A" ) ) + TF( Ge( " ", "ab" ) )
   c += TF( Ge( " ", "ab " ) ) + TF( Ge( " ", "abc" ) ) + TF( Ge( " ", "b" ) ) + TF( Ge( " ", " a" ) )
   c += TF( Ge( " ", ( "a" + Chr( 0 ) ) ) ) + TF( Ge( " ", Chr( 0 ) ) ) + TF( Ge( " ", Chr( 200 ) ) )
   c += TF( Ge( " ", "aB" ) )
   ? c
   c := TF( Ge( "a", "" ) ) + TF( Ge( "a", " " ) ) + TF( Ge( "a", "a" ) ) + TF( Ge( "a", "A" ) ) + TF( Ge( "a", "ab" ) )
   c += TF( Ge( "a", "ab " ) ) + TF( Ge( "a", "abc" ) ) + TF( Ge( "a", "b" ) ) + TF( Ge( "a", " a" ) )
   c += TF( Ge( "a", ( "a" + Chr( 0 ) ) ) ) + TF( Ge( "a", Chr( 0 ) ) ) + TF( Ge( "a", Chr( 200 ) ) )
   c += TF( Ge( "a", "aB" ) )
   ? c
   c := TF( Ge( "A", "" ) ) + TF( Ge( "A", " " ) ) + TF( Ge( "A", "a" ) ) + TF( Ge( "A", "A" ) ) + TF( Ge( "A", "ab" ) )
   c += TF( Ge( "A", "ab " ) ) + TF( Ge( "A", "abc" ) ) + TF( Ge( "A", "b" ) ) + TF( Ge( "A", " a" ) )
   c += TF( Ge( "A", ( "a" + Chr( 0 ) ) ) ) + TF( Ge( "A", Chr( 0 ) ) ) + TF( Ge( "A", Chr( 200 ) ) )
   c += TF( Ge( "A", "aB" ) )
   ? c
   c := TF( Ge( "ab", "" ) ) + TF( Ge( "ab", " " ) ) + TF( Ge( "ab", "a" ) ) + TF( Ge( "ab", "A" ) )
   c += TF( Ge( "ab", "ab" ) ) + TF( Ge( "ab", "ab " ) ) + TF( Ge( "ab", "abc" ) ) + TF( Ge( "ab", "b" ) )
   c += TF( Ge( "ab", " a" ) ) + TF( Ge( "ab", ( "a" + Chr( 0 ) ) ) ) + TF( Ge( "ab", Chr( 0 ) ) )
   c += TF( Ge( "ab", Chr( 200 ) ) ) + TF( Ge( "ab", "aB" ) )
   ? c
   c := TF( Ge( "ab ", "" ) ) + TF( Ge( "ab ", " " ) ) + TF( Ge( "ab ", "a" ) ) + TF( Ge( "ab ", "A" ) )
   c += TF( Ge( "ab ", "ab" ) ) + TF( Ge( "ab ", "ab " ) ) + TF( Ge( "ab ", "abc" ) ) + TF( Ge( "ab ", "b" ) )
   c += TF( Ge( "ab ", " a" ) ) + TF( Ge( "ab ", ( "a" + Chr( 0 ) ) ) ) + TF( Ge( "ab ", Chr( 0 ) ) )
   c += TF( Ge( "ab ", Chr( 200 ) ) ) + TF( Ge( "ab ", "aB" ) )
   ? c
   c := TF( Ge( "abc", "" ) ) + TF( Ge( "abc", " " ) ) + TF( Ge( "abc", "a" ) ) + TF( Ge( "abc", "A" ) )
   c += TF( Ge( "abc", "ab" ) ) + TF( Ge( "abc", "ab " ) ) + TF( Ge( "abc", "abc" ) ) + TF( Ge( "abc", "b" ) )
   c += TF( Ge( "abc", " a" ) ) + TF( Ge( "abc", ( "a" + Chr( 0 ) ) ) ) + TF( Ge( "abc", Chr( 0 ) ) )
   c += TF( Ge( "abc", Chr( 200 ) ) ) + TF( Ge( "abc", "aB" ) )
   ? c
   c := TF( Ge( "b", "" ) ) + TF( Ge( "b", " " ) ) + TF( Ge( "b", "a" ) ) + TF( Ge( "b", "A" ) ) + TF( Ge( "b", "ab" ) )
   c += TF( Ge( "b", "ab " ) ) + TF( Ge( "b", "abc" ) ) + TF( Ge( "b", "b" ) ) + TF( Ge( "b", " a" ) )
   c += TF( Ge( "b", ( "a" + Chr( 0 ) ) ) ) + TF( Ge( "b", Chr( 0 ) ) ) + TF( Ge( "b", Chr( 200 ) ) )
   c += TF( Ge( "b", "aB" ) )
   ? c
   c := TF( Ge( " a", "" ) ) + TF( Ge( " a", " " ) ) + TF( Ge( " a", "a" ) ) + TF( Ge( " a", "A" ) )
   c += TF( Ge( " a", "ab" ) ) + TF( Ge( " a", "ab " ) ) + TF( Ge( " a", "abc" ) ) + TF( Ge( " a", "b" ) )
   c += TF( Ge( " a", " a" ) ) + TF( Ge( " a", ( "a" + Chr( 0 ) ) ) ) + TF( Ge( " a", Chr( 0 ) ) )
   c += TF( Ge( " a", Chr( 200 ) ) ) + TF( Ge( " a", "aB" ) )
   ? c
   c := TF( Ge( ( "a" + Chr( 0 ) ), "" ) ) + TF( Ge( ( "a" + Chr( 0 ) ), " " ) ) + TF( Ge( ( "a" + Chr( 0 ) ), "a" ) )
   c += TF( Ge( ( "a" + Chr( 0 ) ), "A" ) ) + TF( Ge( ( "a" + Chr( 0 ) ), "ab" ) )
   c += TF( Ge( ( "a" + Chr( 0 ) ), "ab " ) ) + TF( Ge( ( "a" + Chr( 0 ) ), "abc" ) )
   c += TF( Ge( ( "a" + Chr( 0 ) ), "b" ) ) + TF( Ge( ( "a" + Chr( 0 ) ), " a" ) )
   c += TF( Ge( ( "a" + Chr( 0 ) ), ( "a" + Chr( 0 ) ) ) ) + TF( Ge( ( "a" + Chr( 0 ) ), Chr( 0 ) ) )
   c += TF( Ge( ( "a" + Chr( 0 ) ), Chr( 200 ) ) ) + TF( Ge( ( "a" + Chr( 0 ) ), "aB" ) )
   ? c
   c := TF( Ge( Chr( 0 ), "" ) ) + TF( Ge( Chr( 0 ), " " ) ) + TF( Ge( Chr( 0 ), "a" ) ) + TF( Ge( Chr( 0 ), "A" ) )
   c += TF( Ge( Chr( 0 ), "ab" ) ) + TF( Ge( Chr( 0 ), "ab " ) ) + TF( Ge( Chr( 0 ), "abc" ) )
   c += TF( Ge( Chr( 0 ), "b" ) ) + TF( Ge( Chr( 0 ), " a" ) ) + TF( Ge( Chr( 0 ), ( "a" + Chr( 0 ) ) ) )
   c += TF( Ge( Chr( 0 ), Chr( 0 ) ) ) + TF( Ge( Chr( 0 ), Chr( 200 ) ) ) + TF( Ge( Chr( 0 ), "aB" ) )
   ? c
   c := TF( Ge( Chr( 200 ), "" ) ) + TF( Ge( Chr( 200 ), " " ) ) + TF( Ge( Chr( 200 ), "a" ) )
   c += TF( Ge( Chr( 200 ), "A" ) ) + TF( Ge( Chr( 200 ), "ab" ) ) + TF( Ge( Chr( 200 ), "ab " ) )
   c += TF( Ge( Chr( 200 ), "abc" ) ) + TF( Ge( Chr( 200 ), "b" ) ) + TF( Ge( Chr( 200 ), " a" ) )
   c += TF( Ge( Chr( 200 ), ( "a" + Chr( 0 ) ) ) ) + TF( Ge( Chr( 200 ), Chr( 0 ) ) )
   c += TF( Ge( Chr( 200 ), Chr( 200 ) ) ) + TF( Ge( Chr( 200 ), "aB" ) )
   ? c
   c := TF( Ge( "aB", "" ) ) + TF( Ge( "aB", " " ) ) + TF( Ge( "aB", "a" ) ) + TF( Ge( "aB", "A" ) )
   c += TF( Ge( "aB", "ab" ) ) + TF( Ge( "aB", "ab " ) ) + TF( Ge( "aB", "abc" ) ) + TF( Ge( "aB", "b" ) )
   c += TF( Ge( "aB", " a" ) ) + TF( Ge( "aB", ( "a" + Chr( 0 ) ) ) ) + TF( Ge( "aB", Chr( 0 ) ) )
   c += TF( Ge( "aB", Chr( 200 ) ) ) + TF( Ge( "aB", "aB" ) )
   ? c
   ? "runtime >="
   Table( aVal, {| a, b | a >= b } )
   ? "macro >="
   Table( aText, {| a, b | &( a + " >= " + b ) } )
   ? "input >="
   Table( aIn, {| a, b | a >= b } )
RETURN

STATIC PROCEDURE ShowEqExact( aVal, aText, aIn )
   LOCAL c
   ? "literal = exact"
   c := TF( "" = "" ) + TF( "" = " " ) + TF( "" = "a" ) + TF( "" = "A" ) + TF( "" = "ab" ) + TF( "" = "ab " )
   c += TF( "" = "abc" ) + TF( "" = "b" ) + TF( "" = " a" ) + TF( "" = ( "a" + Chr( 0 ) ) ) + TF( "" = Chr( 0 ) )
   c += TF( "" = Chr( 200 ) ) + TF( "" = "aB" )
   ? c
   c := TF( " " = "" ) + TF( " " = " " ) + TF( " " = "a" ) + TF( " " = "A" ) + TF( " " = "ab" ) + TF( " " = "ab " )
   c += TF( " " = "abc" ) + TF( " " = "b" ) + TF( " " = " a" ) + TF( " " = ( "a" + Chr( 0 ) ) ) + TF( " " = Chr( 0 ) )
   c += TF( " " = Chr( 200 ) ) + TF( " " = "aB" )
   ? c
   c := TF( "a" = "" ) + TF( "a" = " " ) + TF( "a" = "a" ) + TF( "a" = "A" ) + TF( "a" = "ab" ) + TF( "a" = "ab " )
   c += TF( "a" = "abc" ) + TF( "a" = "b" ) + TF( "a" = " a" ) + TF( "a" = ( "a" + Chr( 0 ) ) ) + TF( "a" = Chr( 0 ) )
   c += TF( "a" = Chr( 200 ) ) + TF( "a" = "aB" )
   ? c
   c := TF( "A" = "" ) + TF( "A" = " " ) + TF( "A" = "a" ) + TF( "A" = "A" ) + TF( "A" = "ab" ) + TF( "A" = "ab " )
   c += TF( "A" = "abc" ) + TF( "A" = "b" ) + TF( "A" = " a" ) + TF( "A" = ( "a" + Chr( 0 ) ) ) + TF( "A" = Chr( 0 ) )
   c += TF( "A" = Chr( 200 ) ) + TF( "A" = "aB" )
   ? c
   c := TF( "ab" = "" ) + TF( "ab" = " " ) + TF( "ab" = "a" ) + TF( "ab" = "A" ) + TF( "ab" = "ab" )
   c += TF( "ab" = "ab " ) + TF( "ab" = "abc" ) + TF( "ab" = "b" ) + TF( "ab" = " a" ) + TF( "ab" = ( "a" + Chr( 0 ) ) )
   c += TF( "ab" = Chr( 0 ) ) + TF( "ab" = Chr( 200 ) ) + TF( "ab" = "aB" )
   ? c
   c := TF( "ab " = "" ) + TF( "ab " = " " ) + TF( "ab " = "a" ) + TF( "ab " = "A" ) + TF( "ab " = "ab" )
   c += TF( "ab " = "ab " ) + TF( "ab " = "abc" ) + TF( "ab " = "b" ) + TF( "ab " = " a" )
   c += TF( "ab " = ( "a" + Chr( 0 ) ) ) + TF( "ab " = Chr( 0 ) ) + TF( "ab " = Chr( 200 ) ) + TF( "ab " = "aB" )
   ? c
   c := TF( "abc" = "" ) + TF( "abc" = " " ) + TF( "abc" = "a" ) + TF( "abc" = "A" ) + TF( "abc" = "ab" )
   c += TF( "abc" = "ab " ) + TF( "abc" = "abc" ) + TF( "abc" = "b" ) + TF( "abc" = " a" )
   c += TF( "abc" = ( "a" + Chr( 0 ) ) ) + TF( "abc" = Chr( 0 ) ) + TF( "abc" = Chr( 200 ) ) + TF( "abc" = "aB" )
   ? c
   c := TF( "b" = "" ) + TF( "b" = " " ) + TF( "b" = "a" ) + TF( "b" = "A" ) + TF( "b" = "ab" ) + TF( "b" = "ab " )
   c += TF( "b" = "abc" ) + TF( "b" = "b" ) + TF( "b" = " a" ) + TF( "b" = ( "a" + Chr( 0 ) ) ) + TF( "b" = Chr( 0 ) )
   c += TF( "b" = Chr( 200 ) ) + TF( "b" = "aB" )
   ? c
   c := TF( " a" = "" ) + TF( " a" = " " ) + TF( " a" = "a" ) + TF( " a" = "A" ) + TF( " a" = "ab" )
   c += TF( " a" = "ab " ) + TF( " a" = "abc" ) + TF( " a" = "b" ) + TF( " a" = " a" ) + TF( " a" = ( "a" + Chr( 0 ) ) )
   c += TF( " a" = Chr( 0 ) ) + TF( " a" = Chr( 200 ) ) + TF( " a" = "aB" )
   ? c
   c := TF( ( "a" + Chr( 0 ) ) = "" ) + TF( ( "a" + Chr( 0 ) ) = " " ) + TF( ( "a" + Chr( 0 ) ) = "a" )
   c += TF( ( "a" + Chr( 0 ) ) = "A" ) + TF( ( "a" + Chr( 0 ) ) = "ab" ) + TF( ( "a" + Chr( 0 ) ) = "ab " )
   c += TF( ( "a" + Chr( 0 ) ) = "abc" ) + TF( ( "a" + Chr( 0 ) ) = "b" ) + TF( ( "a" + Chr( 0 ) ) = " a" )
   c += TF( ( "a" + Chr( 0 ) ) = ( "a" + Chr( 0 ) ) ) + TF( ( "a" + Chr( 0 ) ) = Chr( 0 ) )
   c += TF( ( "a" + Chr( 0 ) ) = Chr( 200 ) ) + TF( ( "a" + Chr( 0 ) ) = "aB" )
   ? c
   c := TF( Chr( 0 ) = "" ) + TF( Chr( 0 ) = " " ) + TF( Chr( 0 ) = "a" ) + TF( Chr( 0 ) = "A" ) + TF( Chr( 0 ) = "ab" )
   c += TF( Chr( 0 ) = "ab " ) + TF( Chr( 0 ) = "abc" ) + TF( Chr( 0 ) = "b" ) + TF( Chr( 0 ) = " a" )
   c += TF( Chr( 0 ) = ( "a" + Chr( 0 ) ) ) + TF( Chr( 0 ) = Chr( 0 ) ) + TF( Chr( 0 ) = Chr( 200 ) )
   c += TF( Chr( 0 ) = "aB" )
   ? c
   c := TF( Chr( 200 ) = "" ) + TF( Chr( 200 ) = " " ) + TF( Chr( 200 ) = "a" ) + TF( Chr( 200 ) = "A" )
   c += TF( Chr( 200 ) = "ab" ) + TF( Chr( 200 ) = "ab " ) + TF( Chr( 200 ) = "abc" ) + TF( Chr( 200 ) = "b" )
   c += TF( Chr( 200 ) = " a" ) + TF( Chr( 200 ) = ( "a" + Chr( 0 ) ) ) + TF( Chr( 200 ) = Chr( 0 ) )
   c += TF( Chr( 200 ) = Chr( 200 ) ) + TF( Chr( 200 ) = "aB" )
   ? c
   c := TF( "aB" = "" ) + TF( "aB" = " " ) + TF( "aB" = "a" ) + TF( "aB" = "A" ) + TF( "aB" = "ab" )
   c += TF( "aB" = "ab " ) + TF( "aB" = "abc" ) + TF( "aB" = "b" ) + TF( "aB" = " a" ) + TF( "aB" = ( "a" + Chr( 0 ) ) )
   c += TF( "aB" = Chr( 0 ) ) + TF( "aB" = Chr( 200 ) ) + TF( "aB" = "aB" )
   ? c
   ? "inlined = exact"
   c := TF( Eq( "", "" ) ) + TF( Eq( "", " " ) ) + TF( Eq( "", "a" ) ) + TF( Eq( "", "A" ) ) + TF( Eq( "", "ab" ) )
   c += TF( Eq( "", "ab " ) ) + TF( Eq( "", "abc" ) ) + TF( Eq( "", "b" ) ) + TF( Eq( "", " a" ) )
   c += TF( Eq( "", ( "a" + Chr( 0 ) ) ) ) + TF( Eq( "", Chr( 0 ) ) ) + TF( Eq( "", Chr( 200 ) ) )
   c += TF( Eq( "", "aB" ) )
   ? c
   c := TF( Eq( " ", "" ) ) + TF( Eq( " ", " " ) ) + TF( Eq( " ", "a" ) ) + TF( Eq( " ", "A" ) ) + TF( Eq( " ", "ab" ) )
   c += TF( Eq( " ", "ab " ) ) + TF( Eq( " ", "abc" ) ) + TF( Eq( " ", "b" ) ) + TF( Eq( " ", " a" ) )
   c += TF( Eq( " ", ( "a" + Chr( 0 ) ) ) ) + TF( Eq( " ", Chr( 0 ) ) ) + TF( Eq( " ", Chr( 200 ) ) )
   c += TF( Eq( " ", "aB" ) )
   ? c
   c := TF( Eq( "a", "" ) ) + TF( Eq( "a", " " ) ) + TF( Eq( "a", "a" ) ) + TF( Eq( "a", "A" ) ) + TF( Eq( "a", "ab" ) )
   c += TF( Eq( "a", "ab " ) ) + TF( Eq( "a", "abc" ) ) + TF( Eq( "a", "b" ) ) + TF( Eq( "a", " a" ) )
   c += TF( Eq( "a", ( "a" + Chr( 0 ) ) ) ) + TF( Eq( "a", Chr( 0 ) ) ) + TF( Eq( "a", Chr( 200 ) ) )
   c += TF( Eq( "a", "aB" ) )
   ? c
   c := TF( Eq( "A", "" ) ) + TF( Eq( "A", " " ) ) + TF( Eq( "A", "a" ) ) + TF( Eq( "A", "A" ) ) + TF( Eq( "A", "ab" ) )
   c += TF( Eq( "A", "ab " ) ) + TF( Eq( "A", "abc" ) ) + TF( Eq( "A", "b" ) ) + TF( Eq( "A", " a" ) )
   c += TF( Eq( "A", ( "a" + Chr( 0 ) ) ) ) + TF( Eq( "A", Chr( 0 ) ) ) + TF( Eq( "A", Chr( 200 ) ) )
   c += TF( Eq( "A", "aB" ) )
   ? c
   c := TF( Eq( "ab", "" ) ) + TF( Eq( "ab", " " ) ) + TF( Eq( "ab", "a" ) ) + TF( Eq( "ab", "A" ) )
   c += TF( Eq( "ab", "ab" ) ) + TF( Eq( "ab", "ab " ) ) + TF( Eq( "ab", "abc" ) ) + TF( Eq( "ab", "b" ) )
   c += TF( Eq( "ab", " a" ) ) + TF( Eq( "ab", ( "a" + Chr( 0 ) ) ) ) + TF( Eq( "ab", Chr( 0 ) ) )
   c += TF( Eq( "ab", Chr( 200 ) ) ) + TF( Eq( "ab", "aB" ) )
   ? c
   c := TF( Eq( "ab ", "" ) ) + TF( Eq( "ab ", " " ) ) + TF( Eq( "ab ", "a" ) ) + TF( Eq( "ab ", "A" ) )
   c += TF( Eq( "ab ", "ab" ) ) + TF( Eq( "ab ", "ab " ) ) + TF( Eq( "ab ", "abc" ) ) + TF( Eq( "ab ", "b" ) )
   c += TF( Eq( "ab ", " a" ) ) + TF( Eq( "ab ", ( "a" + Chr( 0 ) ) ) ) + TF( Eq( "ab ", Chr( 0 ) ) )
   c += TF( Eq( "ab ", Chr( 200 ) ) ) + TF( Eq( "ab ", "aB" ) )
   ? c
   c := TF( Eq( "abc", "" ) ) + TF( Eq( "abc", " " ) ) + TF( Eq( "abc", "a" ) ) + TF( Eq( "abc", "A" ) )
   c += TF( Eq( "abc", "ab" ) ) + TF( Eq( "abc", "ab " ) ) + TF( Eq( "abc", "abc" ) ) + TF( Eq( "abc", "b" ) )
   c += TF( Eq( "abc", " a" ) ) + TF( Eq( "abc", ( "a" + Chr( 0 ) ) ) ) + TF( Eq( "abc", Chr( 0 ) ) )
   c += TF( Eq( "abc", Chr( 200 ) ) ) + TF( Eq( "abc", "aB" ) )
   ? c
   c := TF( Eq( "b", "" ) ) + TF( Eq( "b", " " ) ) + TF( Eq( "b", "a" ) ) + TF( Eq( "b", "A" ) ) + TF( Eq( "b", "ab" ) )
   c += TF( Eq( "b", "ab " ) ) + TF( Eq( "b", "abc" ) ) + TF( Eq( "b", "b" ) ) + TF( Eq( "b", " a" ) )
   c += TF( Eq( "b", ( "a" + Chr( 0 ) ) ) ) + TF( Eq( "b", Chr( 0 ) ) ) + TF( Eq( "b", Chr( 200 ) ) )
   c += TF( Eq( "b", "aB" ) )
   ? c
   c := TF( Eq( " a", "" ) ) + TF( Eq( " a", " " ) ) + TF( Eq( " a", "a" ) ) + TF( Eq( " a", "A" ) )
   c += TF( Eq( " a", "ab" ) ) + TF( Eq( " a", "ab " ) ) + TF( Eq( " a", "abc" ) ) + TF( Eq( " a", "b" ) )
   c += TF( Eq( " a", " a" ) ) + TF( Eq( " a", ( "a" + Chr( 0 ) ) ) ) + TF( Eq( " a", Chr( 0 ) ) )
   c += TF( Eq( " a", Chr( 200 ) ) ) + TF( Eq( " a", "aB" ) )
   ? c
   c := TF( Eq( ( "a" + Chr( 0 ) ), "" ) ) + TF( Eq( ( "a" + Chr( 0 ) ), " " ) ) + TF( Eq( ( "a" + Chr( 0 ) ), "a" ) )
   c += TF( Eq( ( "a" + Chr( 0 ) ), "A" ) ) + TF( Eq( ( "a" + Chr( 0 ) ), "ab" ) )
   c += TF( Eq( ( "a" + Chr( 0 ) ), "ab " ) ) + TF( Eq( ( "a" + Chr( 0 ) ), "abc" ) )
   c += TF( Eq( ( "a" + Chr( 0 ) ), "b" ) ) + TF( Eq( ( "a" + Chr( 0 ) ), " a" ) )
   c += TF( Eq( ( "a" + Chr( 0 ) ), ( "a" + Chr( 0 ) ) ) ) + TF( Eq( ( "a" + Chr( 0 ) ), Chr( 0 ) ) )
   c += TF( Eq( ( "a" + Chr( 0 ) ), Chr( 200 ) ) ) + TF( Eq( ( "a" + Chr( 0 ) ), "aB" ) )
   ? c
   c := TF( Eq( Chr( 0 ), "" ) ) + TF( Eq( Chr( 0 ), " " ) ) + TF( Eq( Chr( 0 ), "a" ) ) + TF( Eq( Chr( 0 ), "A" ) )
   c += TF( Eq( Chr( 0 ), "ab" ) ) + TF( Eq( Chr( 0 ), "ab " ) ) + TF( Eq( Chr( 0 ), "abc" ) )
   c += TF( Eq( Chr( 0 ), "b" ) ) + TF( Eq( Chr( 0 ), " a" ) ) + TF( Eq( Chr( 0 ), ( "a" + Chr( 0 ) ) ) )
   c += TF( Eq( Chr( 0 ), Chr( 0 ) ) ) + TF( Eq( Chr( 0 ), Chr( 200 ) ) ) + TF( Eq( Chr( 0 ), "aB" ) )
   ? c
   c := TF( Eq( Chr( 200 ), "" ) ) + TF( Eq( Chr( 200 ), " " ) ) + TF( Eq( Chr( 200 ), "a" ) )
   c += TF( Eq( Chr( 200 ), "A" ) ) + TF( Eq( Chr( 200 ), "ab" ) ) + TF( Eq( Chr( 200 ), "ab " ) )
   c += TF( Eq( Chr( 200 ), "abc" ) ) + TF( Eq( Chr( 200 ), "b" ) ) + TF( Eq( Chr( 200 ), " a" ) )
   c += TF( Eq( Chr( 200 ), ( "a" + Chr( 0 ) ) ) ) + TF( Eq( Chr( 200 ), Chr( 0 ) ) )
   c += TF( Eq( Chr( 200 ), Chr( 200 ) ) ) + TF( Eq( Chr( 200 ), "aB" ) )
   ? c
   c := TF( Eq( "aB", "" ) ) + TF( Eq( "aB", " " ) ) + TF( Eq( "aB", "a" ) ) + TF( Eq( "aB", "A" ) )
   c += TF( Eq( "aB", "ab" ) ) + TF( Eq( "aB", "ab " ) ) + TF( Eq( "aB", "abc" ) ) + TF( Eq( "aB", "b" ) )
   c += TF( Eq( "aB", " a" ) ) + TF( Eq( "aB", ( "a" + Chr( 0 ) ) ) ) + TF( Eq( "aB", Chr( 0 ) ) )
   c += TF( Eq( "aB", Chr( 200 ) ) ) + TF( Eq( "aB", "aB" ) )
   ? c
   ? "runtime = exact"
   Table( aVal, {| a, b | a = b } )
   ? "macro = exact"
   Table( aText, {| a, b | &( a + " = " + b ) } )
   ? "input = exact"
   Table( aIn, {| a, b | a = b } )
RETURN

STATIC PROCEDURE ShowNeExact( aVal, aText, aIn )
   LOCAL c
   ? "literal != exact"
   c := TF( "" != "" ) + TF( "" != " " ) + TF( "" != "a" ) + TF( "" != "A" ) + TF( "" != "ab" ) + TF( "" != "ab " )
   c += TF( "" != "abc" ) + TF( "" != "b" ) + TF( "" != " a" ) + TF( "" != ( "a" + Chr( 0 ) ) ) + TF( "" != Chr( 0 ) )
   c += TF( "" != Chr( 200 ) ) + TF( "" != "aB" )
   ? c
   c := TF( " " != "" ) + TF( " " != " " ) + TF( " " != "a" ) + TF( " " != "A" ) + TF( " " != "ab" )
   c += TF( " " != "ab " ) + TF( " " != "abc" ) + TF( " " != "b" ) + TF( " " != " a" ) + TF( " " != ( "a" + Chr( 0 ) ) )
   c += TF( " " != Chr( 0 ) ) + TF( " " != Chr( 200 ) ) + TF( " " != "aB" )
   ? c
   c := TF( "a" != "" ) + TF( "a" != " " ) + TF( "a" != "a" ) + TF( "a" != "A" ) + TF( "a" != "ab" )
   c += TF( "a" != "ab " ) + TF( "a" != "abc" ) + TF( "a" != "b" ) + TF( "a" != " a" ) + TF( "a" != ( "a" + Chr( 0 ) ) )
   c += TF( "a" != Chr( 0 ) ) + TF( "a" != Chr( 200 ) ) + TF( "a" != "aB" )
   ? c
   c := TF( "A" != "" ) + TF( "A" != " " ) + TF( "A" != "a" ) + TF( "A" != "A" ) + TF( "A" != "ab" )
   c += TF( "A" != "ab " ) + TF( "A" != "abc" ) + TF( "A" != "b" ) + TF( "A" != " a" ) + TF( "A" != ( "a" + Chr( 0 ) ) )
   c += TF( "A" != Chr( 0 ) ) + TF( "A" != Chr( 200 ) ) + TF( "A" != "aB" )
   ? c
   c := TF( "ab" != "" ) + TF( "ab" != " " ) + TF( "ab" != "a" ) + TF( "ab" != "A" ) + TF( "ab" != "ab" )
   c += TF( "ab" != "ab " ) + TF( "ab" != "abc" ) + TF( "ab" != "b" ) + TF( "ab" != " a" )
   c += TF( "ab" != ( "a" + Chr( 0 ) ) ) + TF( "ab" != Chr( 0 ) ) + TF( "ab" != Chr( 200 ) ) + TF( "ab" != "aB" )
   ? c
   c := TF( "ab " != "" ) + TF( "ab " != " " ) + TF( "ab " != "a" ) + TF( "ab " != "A" ) + TF( "ab " != "ab" )
   c += TF( "ab " != "ab " ) + TF( "ab " != "abc" ) + TF( "ab " != "b" ) + TF( "ab " != " a" )
   c += TF( "ab " != ( "a" + Chr( 0 ) ) ) + TF( "ab " != Chr( 0 ) ) + TF( "ab " != Chr( 200 ) ) + TF( "ab " != "aB" )
   ? c
   c := TF( "abc" != "" ) + TF( "abc" != " " ) + TF( "abc" != "a" ) + TF( "abc" != "A" ) + TF( "abc" != "ab" )
   c += TF( "abc" != "ab " ) + TF( "abc" != "abc" ) + TF( "abc" != "b" ) + TF( "abc" != " a" )
   c += TF( "abc" != ( "a" + Chr( 0 ) ) ) + TF( "abc" != Chr( 0 ) ) + TF( "abc" != Chr( 200 ) ) + TF( "abc" != "aB" )
   ? c
   c := TF( "b" != "" ) + TF( "b" != " " ) + TF( "b" != "a" ) + TF( "b" != "A" ) + TF( "b" != "ab" )
   c += TF( "b" != "ab " ) + TF( "b" != "abc" ) + TF( "b" != "b" ) + TF( "b" != " a" ) + TF( "b" != ( "a" + Chr( 0 ) ) )
   c += TF( "b" != Chr( 0 ) ) + TF( "b" != Chr( 200 ) ) + TF( "b" != "aB" )
   ? c
   c := TF( " a" != "" ) + TF( " a" != " " ) + TF( " a" != "a" ) + TF( " a" != "A" ) + TF( " a" != "ab" )
   c += TF( " a" != "ab " ) + TF( " a" != "abc" ) + TF( " a" != "b" ) + TF( " a" != " a" )
   c += TF( " a" != ( "a" + Chr( 0 ) ) ) + TF( " a" != Chr( 0 ) ) + TF( " a" != Chr( 200 ) ) + TF( " a" != "aB" )
   ? c
   c := TF( ( "a" + Chr( 0 ) ) != "" ) + TF( ( "a" + Chr( 0 ) ) != " " ) + TF( ( "a" + Chr( 0 ) ) != "a" )
   c += TF( ( "a" + Chr( 0 ) ) != "A" ) + TF( ( "a" + Chr( 0 ) ) != "ab" ) + TF( ( "a" + Chr( 0 ) ) != "ab " )
   c += TF( ( "a" + Chr( 0 ) ) != "abc" ) + TF( ( "a" + Chr( 0 ) ) != "b" ) + TF( ( "a" + Chr( 0 ) ) != " a" )
   c += TF( ( "a" + Chr( 0 ) ) != ( "a" + Chr( 0 ) ) ) + TF( ( "a" + Chr( 0 ) ) != Chr( 0 ) )
   c += TF( ( "a" + Chr( 0 ) ) != Chr( 200 ) ) + TF( ( "a" + Chr( 0 ) ) != "aB" )
   ? c
   c := TF( Chr( 0 ) != "" ) + TF( Chr( 0 ) != " " ) + TF( Chr( 0 ) != "a" ) + TF( Chr( 0 ) != "A" )
   c += TF( Chr( 0 ) != "ab" ) + TF( Chr( 0 ) != "ab " ) + TF( Chr( 0 ) != "abc" ) + TF( Chr( 0 ) != "b" )
   c += TF( Chr( 0 ) != " a" ) + TF( Chr( 0 ) != ( "a" + Chr( 0 ) ) ) + TF( Chr( 0 ) != Chr( 0 ) )
   c += TF( Chr( 0 ) != Chr( 200 ) ) + TF( Chr( 0 ) != "aB" )
   ? c
   c := TF( Chr( 200 ) != "" ) + TF( Chr( 200 ) != " " ) + TF( Chr( 200 ) != "a" ) + TF( Chr( 200 ) != "A" )
   c += TF( Chr( 200 ) != "ab" ) + TF( Chr( 200 ) != "ab " ) + TF( Chr( 200 ) != "abc" ) + TF( Chr( 200 ) != "b" )
   c += TF( Chr( 200 ) != " a" ) + TF( Chr( 200 ) != ( "a" + Chr( 0 ) ) ) + TF( Chr( 200 ) != Chr( 0 ) )
   c += TF( Chr( 200 ) != Chr( 200 ) ) + TF( Chr( 200 ) != "aB" )
   ? c
   c := TF( "aB" != "" ) + TF( "aB" != " " ) + TF( "aB" != "a" ) + TF( "aB" != "A" ) + TF( "aB" != "ab" )
   c += TF( "aB" != "ab " ) + TF( "aB" != "abc" ) + TF( "aB" != "b" ) + TF( "aB" != " a" )
   c += TF( "aB" != ( "a" + Chr( 0 ) ) ) + TF( "aB" != Chr( 0 ) ) + TF( "aB" != Chr( 200 ) ) + TF( "aB" != "aB" )
   ? c
   ? "inlined != exact"
   c := TF( Ne( "", "" ) ) + TF( Ne( "", " " ) ) + TF( Ne( "", "a" ) ) + TF( Ne( "", "A" ) ) + TF( Ne( "", "ab" ) )
   c += TF( Ne( "", "ab " ) ) + TF( Ne( "", "abc" ) ) + TF( Ne( "", "b" ) ) + TF( Ne( "", " a" ) )
   c += TF( Ne( "", ( "a" + Chr( 0 ) ) ) ) + TF( Ne( "", Chr( 0 ) ) ) + TF( Ne( "", Chr( 200 ) ) )
   c += TF( Ne( "", "aB" ) )
   ? c
   c := TF( Ne( " ", "" ) ) + TF( Ne( " ", " " ) ) + TF( Ne( " ", "a" ) ) + TF( Ne( " ", "A" ) ) + TF( Ne( " ", "ab" ) )
   c += TF( Ne( " ", "ab " ) ) + TF( Ne( " ", "abc" ) ) + TF( Ne( " ", "b" ) ) + TF( Ne( " ", " a" ) )
   c += TF( Ne( " ", ( "a" + Chr( 0 ) ) ) ) + TF( Ne( " ", Chr( 0 ) ) ) + TF( Ne( " ", Chr( 200 ) ) )
   c += TF( Ne( " ", "aB" ) )
   ? c
   c := TF( Ne( "a", "" ) ) + TF( Ne( "a", " " ) ) + TF( Ne( "a", "a" ) ) + TF( Ne( "a", "A" ) ) + TF( Ne( "a", "ab" ) )
   c += TF( Ne( "a", "ab " ) ) + TF( Ne( "a", "abc" ) ) + TF( Ne( "a", "b" ) ) + TF( Ne( "a", " a" ) )
   c += TF( Ne( "a", ( "a" + Chr( 0 ) ) ) ) + TF( Ne( "a", Chr( 0 ) ) ) + TF( Ne( "a", Chr( 200 ) ) )
   c += TF( Ne( "a", "aB" ) )
   ? c
   c := TF( Ne( "A", "" ) ) + TF( Ne( "A", " " ) ) + TF( Ne( "A", "a" ) ) + TF( Ne( "A", "A" ) ) + TF( Ne( "A", "ab" ) )
   c += TF( Ne( "A", "ab " ) ) + TF( Ne( "A", "abc" ) ) + TF( Ne( "A", "b" ) ) + TF( Ne( "A", " a" ) )
   c += TF( Ne( "A", ( "a" + Chr( 0 ) ) ) ) + TF( Ne( "A", Chr( 0 ) ) ) + TF( Ne( "A", Chr( 200 ) ) )
   c += TF( Ne( "A", "aB" ) )
   ? c
   c := TF( Ne( "ab", "" ) ) + TF( Ne( "ab", " " ) ) + TF( Ne( "ab", "a" ) ) + TF( Ne( "ab", "A" ) )
   c += TF( Ne( "ab", "ab" ) ) + TF( Ne( "ab", "ab " ) ) + TF( Ne( "ab", "abc" ) ) + TF( Ne( "ab", "b" ) )
   c += TF( Ne( "ab", " a" ) ) + TF( Ne( "ab", ( "a" + Chr( 0 ) ) ) ) + TF( Ne( "ab", Chr( 0 ) ) )
   c += TF( Ne( "ab", Chr( 200 ) ) ) + TF( Ne( "ab", "aB" ) )
   ? c
   c := TF( Ne( "ab ", "" ) ) + TF( Ne( "ab ", " " ) ) + TF( Ne( "ab ", "a" ) ) + TF( Ne( "ab ", "A" ) )
   c += TF( Ne( "ab ", "ab" ) ) + TF( Ne( "ab ", "ab " ) ) + TF( Ne( "ab ", "abc" ) ) + TF( Ne( "ab ", "b" ) )
   c += TF( Ne( "ab ", " a" ) ) + TF( Ne( "ab ", ( "a" + Chr( 0 ) ) ) ) + TF( Ne( "ab ", Chr( 0 ) ) )
   c += TF( Ne( "ab ", Chr( 200 ) ) ) + TF( Ne( "ab ", "aB" ) )
   ? c
   c := TF( Ne( "abc", "" ) ) + TF( Ne( "abc", " " ) ) + TF( Ne( "abc", "a" ) ) + TF( Ne( "abc", "A" ) )
   c += TF( Ne( "abc", "ab" ) ) + TF( Ne( "abc", "ab " ) ) + TF( Ne( "abc", "abc" ) ) + TF( Ne( "abc", "b" ) )
   c += TF( Ne( "abc", " a" ) ) + TF( Ne( "abc", ( "a" + Chr( 0 ) ) ) ) + TF( Ne( "abc", Chr( 0 ) ) )
   c += TF( Ne( "abc", Chr( 200 ) ) ) + TF( Ne( "abc", "aB" ) )
   ? c
   c := TF( Ne( "b", "" ) ) + TF( Ne( "b", " " ) ) + TF( Ne( "b", "a" ) ) + TF( Ne( "b", "A" ) ) + TF( Ne( "b", "ab" ) )
   c += TF( Ne( "b", "ab " ) ) + TF( Ne( "b", "abc" ) ) + TF( Ne( "b", "b" ) ) + TF( Ne( "b", " a" ) )
   c += TF( Ne( "b", ( "a" + Chr( 0 ) ) ) ) + TF( Ne( "b", Chr( 0 ) ) ) + TF( Ne( "b", Chr( 200 ) ) )
   c += TF( Ne( "b", "aB" ) )
   ? c
   c := TF( Ne( " a", "" ) ) + TF( Ne( " a", " " ) ) + TF( Ne( " a", "a" ) ) + TF( Ne( " a", "A" ) )
   c += TF( Ne( " a", "ab" ) ) + TF( Ne( " a", "ab " ) ) + TF( Ne( " a", "abc" ) ) + TF( Ne( " a", "b" ) )
   c += TF( Ne( " a", " a" ) ) + TF( Ne( " a", ( "a" + Chr( 0 ) ) ) ) + TF( Ne( " a", Chr( 0 ) ) )
   c += TF( Ne( " a", Chr( 200 ) ) ) + TF( Ne( " a", "aB" ) )
   ? c
   c := TF( Ne( ( "a" + Chr( 0 ) ), "" ) ) + TF( Ne( ( "a" + Chr( 0 ) ), " " ) ) + TF( Ne( ( "a" + Chr( 0 ) ), "a" ) )
   c += TF( Ne( ( "a" + Chr( 0 ) ), "A" ) ) + TF( Ne( ( "a" + Chr( 0 ) ), "ab" ) )
   c += TF( Ne( ( "a" + Chr( 0 ) ), "ab " ) ) + TF( Ne( ( "a" + Chr( 0 ) ), "abc" ) )
   c += TF( Ne( ( "a" + Chr( 0 ) ), "b" ) ) + TF( Ne( ( "a" + Chr( 0 ) ), " a" ) )
   c += TF( Ne( ( "a" + Chr( 0 ) ), ( "a" + Chr( 0 ) ) ) ) + TF( Ne( ( "a" + Chr( 0 ) ), Chr( 0 ) ) )
   c += TF( Ne( ( "a" + Chr( 0 ) ), Chr( 200 ) ) ) + TF( Ne( ( "a" + Chr( 0 ) ), "aB" ) )
   ? c
   c := TF( Ne( Chr( 0 ), "" ) ) + TF( Ne( Chr( 0 ), " " ) ) + TF( Ne( Chr( 0 ), "a" ) ) + TF( Ne( Chr( 0 ), "A" ) )
   c += TF( Ne( Chr( 0 ), "ab" ) ) + TF( Ne( Chr( 0 ), "ab " ) ) + TF( Ne( Chr( 0 ), "abc" ) )
   c += TF( Ne( Chr( 0 ), "b" ) ) + TF( Ne( Chr( 0 ), " a" ) ) + TF( Ne( Chr( 0 ), ( "a" + Chr( 0 ) ) ) )
   c += TF( Ne( Chr( 0 ), Chr( 0 ) ) ) + TF( Ne( Chr( 0 ), Chr( 200 ) ) ) + TF( Ne( Chr( 0 ), "aB" ) )
   ? c
   c := TF( Ne( Chr( 200 ), "" ) ) + TF( Ne( Chr( 200 ), " " ) ) + TF( Ne( Chr( 200 ), "a" ) )
   c += TF( Ne( Chr( 200 ), "A" ) ) + TF( Ne( Chr( 200 ), "ab" ) ) + TF( Ne( Chr( 200 ), "ab " ) )
   c += TF( Ne( Chr( 200 ), "abc" ) ) + TF( Ne( Chr( 200 ), "b" ) ) + TF( Ne( Chr( 200 ), " a" ) )
   c += TF( Ne( Chr( 200 ), ( "a" + Chr( 0 ) ) ) ) + TF( Ne( Chr( 200 ), Chr( 0 ) ) )
   c += TF( Ne( Chr( 200 ), Chr( 200 ) ) ) + TF( Ne( Chr( 200 ), "aB" ) )
   ? c
   c := TF( Ne( "aB", "" ) ) + TF( Ne( "aB", " " ) ) + TF( Ne( "aB", "a" ) ) + TF( Ne( "aB", "A" ) )
   c += TF( Ne( "aB", "ab" ) ) + TF( Ne( "aB", "ab " ) ) + TF( Ne( "aB", "abc" ) ) + TF( Ne( "aB", "b" ) )
   c += TF( Ne( "aB", " a" ) ) + TF( Ne( "aB", ( "a" + Chr( 0 ) ) ) ) + TF( Ne( "aB", Chr( 0 ) ) )
   c += TF( Ne( "aB", Chr( 200 ) ) ) + TF( Ne( "aB", "aB" ) )
   ? c
   ? "runtime != exact"
   Table( aVal, {| a, b | a != b } )
   ? "macro != exact"
   Table( aText, {| a, b | &( a + " != " + b ) } )
   ? "input != exact"
   Table( aIn, {| a, b | a != b } )
RETURN

STATIC PROCEDURE ShowLtExact( aVal, aText, aIn )
   LOCAL c
   ? "literal < exact"
   c := TF( "" < "" ) + TF( "" < " " ) + TF( "" < "a" ) + TF( "" < "A" ) + TF( "" < "ab" ) + TF( "" < "ab " )
   c += TF( "" < "abc" ) + TF( "" < "b" ) + TF( "" < " a" ) + TF( "" < ( "a" + Chr( 0 ) ) ) + TF( "" < Chr( 0 ) )
   c += TF( "" < Chr( 200 ) ) + TF( "" < "aB" )
   ? c
   c := TF( " " < "" ) + TF( " " < " " ) + TF( " " < "a" ) + TF( " " < "A" ) + TF( " " < "ab" ) + TF( " " < "ab " )
   c += TF( " " < "abc" ) + TF( " " < "b" ) + TF( " " < " a" ) + TF( " " < ( "a" + Chr( 0 ) ) ) + TF( " " < Chr( 0 ) )
   c += TF( " " < Chr( 200 ) ) + TF( " " < "aB" )
   ? c
   c := TF( "a" < "" ) + TF( "a" < " " ) + TF( "a" < "a" ) + TF( "a" < "A" ) + TF( "a" < "ab" ) + TF( "a" < "ab " )
   c += TF( "a" < "abc" ) + TF( "a" < "b" ) + TF( "a" < " a" ) + TF( "a" < ( "a" + Chr( 0 ) ) ) + TF( "a" < Chr( 0 ) )
   c += TF( "a" < Chr( 200 ) ) + TF( "a" < "aB" )
   ? c
   c := TF( "A" < "" ) + TF( "A" < " " ) + TF( "A" < "a" ) + TF( "A" < "A" ) + TF( "A" < "ab" ) + TF( "A" < "ab " )
   c += TF( "A" < "abc" ) + TF( "A" < "b" ) + TF( "A" < " a" ) + TF( "A" < ( "a" + Chr( 0 ) ) ) + TF( "A" < Chr( 0 ) )
   c += TF( "A" < Chr( 200 ) ) + TF( "A" < "aB" )
   ? c
   c := TF( "ab" < "" ) + TF( "ab" < " " ) + TF( "ab" < "a" ) + TF( "ab" < "A" ) + TF( "ab" < "ab" )
   c += TF( "ab" < "ab " ) + TF( "ab" < "abc" ) + TF( "ab" < "b" ) + TF( "ab" < " a" ) + TF( "ab" < ( "a" + Chr( 0 ) ) )
   c += TF( "ab" < Chr( 0 ) ) + TF( "ab" < Chr( 200 ) ) + TF( "ab" < "aB" )
   ? c
   c := TF( "ab " < "" ) + TF( "ab " < " " ) + TF( "ab " < "a" ) + TF( "ab " < "A" ) + TF( "ab " < "ab" )
   c += TF( "ab " < "ab " ) + TF( "ab " < "abc" ) + TF( "ab " < "b" ) + TF( "ab " < " a" )
   c += TF( "ab " < ( "a" + Chr( 0 ) ) ) + TF( "ab " < Chr( 0 ) ) + TF( "ab " < Chr( 200 ) ) + TF( "ab " < "aB" )
   ? c
   c := TF( "abc" < "" ) + TF( "abc" < " " ) + TF( "abc" < "a" ) + TF( "abc" < "A" ) + TF( "abc" < "ab" )
   c += TF( "abc" < "ab " ) + TF( "abc" < "abc" ) + TF( "abc" < "b" ) + TF( "abc" < " a" )
   c += TF( "abc" < ( "a" + Chr( 0 ) ) ) + TF( "abc" < Chr( 0 ) ) + TF( "abc" < Chr( 200 ) ) + TF( "abc" < "aB" )
   ? c
   c := TF( "b" < "" ) + TF( "b" < " " ) + TF( "b" < "a" ) + TF( "b" < "A" ) + TF( "b" < "ab" ) + TF( "b" < "ab " )
   c += TF( "b" < "abc" ) + TF( "b" < "b" ) + TF( "b" < " a" ) + TF( "b" < ( "a" + Chr( 0 ) ) ) + TF( "b" < Chr( 0 ) )
   c += TF( "b" < Chr( 200 ) ) + TF( "b" < "aB" )
   ? c
   c := TF( " a" < "" ) + TF( " a" < " " ) + TF( " a" < "a" ) + TF( " a" < "A" ) + TF( " a" < "ab" )
   c += TF( " a" < "ab " ) + TF( " a" < "abc" ) + TF( " a" < "b" ) + TF( " a" < " a" ) + TF( " a" < ( "a" + Chr( 0 ) ) )
   c += TF( " a" < Chr( 0 ) ) + TF( " a" < Chr( 200 ) ) + TF( " a" < "aB" )
   ? c
   c := TF( ( "a" + Chr( 0 ) ) < "" ) + TF( ( "a" + Chr( 0 ) ) < " " ) + TF( ( "a" + Chr( 0 ) ) < "a" )
   c += TF( ( "a" + Chr( 0 ) ) < "A" ) + TF( ( "a" + Chr( 0 ) ) < "ab" ) + TF( ( "a" + Chr( 0 ) ) < "ab " )
   c += TF( ( "a" + Chr( 0 ) ) < "abc" ) + TF( ( "a" + Chr( 0 ) ) < "b" ) + TF( ( "a" + Chr( 0 ) ) < " a" )
   c += TF( ( "a" + Chr( 0 ) ) < ( "a" + Chr( 0 ) ) ) + TF( ( "a" + Chr( 0 ) ) < Chr( 0 ) )
   c += TF( ( "a" + Chr( 0 ) ) < Chr( 200 ) ) + TF( ( "a" + Chr( 0 ) ) < "aB" )
   ? c
   c := TF( Chr( 0 ) < "" ) + TF( Chr( 0 ) < " " ) + TF( Chr( 0 ) < "a" ) + TF( Chr( 0 ) < "A" ) + TF( Chr( 0 ) < "ab" )
   c += TF( Chr( 0 ) < "ab " ) + TF( Chr( 0 ) < "abc" ) + TF( Chr( 0 ) < "b" ) + TF( Chr( 0 ) < " a" )
   c += TF( Chr( 0 ) < ( "a" + Chr( 0 ) ) ) + TF( Chr( 0 ) < Chr( 0 ) ) + TF( Chr( 0 ) < Chr( 200 ) )
   c += TF( Chr( 0 ) < "aB" )
   ? c
   c := TF( Chr( 200 ) < "" ) + TF( Chr( 200 ) < " " ) + TF( Chr( 200 ) < "a" ) + TF( Chr( 200 ) < "A" )
   c += TF( Chr( 200 ) < "ab" ) + TF( Chr( 200 ) < "ab " ) + TF( Chr( 200 ) < "abc" ) + TF( Chr( 200 ) < "b" )
   c += TF( Chr( 200 ) < " a" ) + TF( Chr( 200 ) < ( "a" + Chr( 0 ) ) ) + TF( Chr( 200 ) < Chr( 0 ) )
   c += TF( Chr( 200 ) < Chr( 200 ) ) + TF( Chr( 200 ) < "aB" )
   ? c
   c := TF( "aB" < "" ) + TF( "aB" < " " ) + TF( "aB" < "a" ) + TF( "aB" < "A" ) + TF( "aB" < "ab" )
   c += TF( "aB" < "ab " ) + TF( "aB" < "abc" ) + TF( "aB" < "b" ) + TF( "aB" < " a" ) + TF( "aB" < ( "a" + Chr( 0 ) ) )
   c += TF( "aB" < Chr( 0 ) ) + TF( "aB" < Chr( 200 ) ) + TF( "aB" < "aB" )
   ? c
   ? "inlined < exact"
   c := TF( Lt( "", "" ) ) + TF( Lt( "", " " ) ) + TF( Lt( "", "a" ) ) + TF( Lt( "", "A" ) ) + TF( Lt( "", "ab" ) )
   c += TF( Lt( "", "ab " ) ) + TF( Lt( "", "abc" ) ) + TF( Lt( "", "b" ) ) + TF( Lt( "", " a" ) )
   c += TF( Lt( "", ( "a" + Chr( 0 ) ) ) ) + TF( Lt( "", Chr( 0 ) ) ) + TF( Lt( "", Chr( 200 ) ) )
   c += TF( Lt( "", "aB" ) )
   ? c
   c := TF( Lt( " ", "" ) ) + TF( Lt( " ", " " ) ) + TF( Lt( " ", "a" ) ) + TF( Lt( " ", "A" ) ) + TF( Lt( " ", "ab" ) )
   c += TF( Lt( " ", "ab " ) ) + TF( Lt( " ", "abc" ) ) + TF( Lt( " ", "b" ) ) + TF( Lt( " ", " a" ) )
   c += TF( Lt( " ", ( "a" + Chr( 0 ) ) ) ) + TF( Lt( " ", Chr( 0 ) ) ) + TF( Lt( " ", Chr( 200 ) ) )
   c += TF( Lt( " ", "aB" ) )
   ? c
   c := TF( Lt( "a", "" ) ) + TF( Lt( "a", " " ) ) + TF( Lt( "a", "a" ) ) + TF( Lt( "a", "A" ) ) + TF( Lt( "a", "ab" ) )
   c += TF( Lt( "a", "ab " ) ) + TF( Lt( "a", "abc" ) ) + TF( Lt( "a", "b" ) ) + TF( Lt( "a", " a" ) )
   c += TF( Lt( "a", ( "a" + Chr( 0 ) ) ) ) + TF( Lt( "a", Chr( 0 ) ) ) + TF( Lt( "a", Chr( 200 ) ) )
   c += TF( Lt( "a", "aB" ) )
   ? c
   c := TF( Lt( "A", "" ) ) + TF( Lt( "A", " " ) ) + TF( Lt( "A", "a" ) ) + TF( Lt( "A", "A" ) ) + TF( Lt( "A", "ab" ) )
   c += TF( Lt( "A", "ab " ) ) + TF( Lt( "A", "abc" ) ) + TF( Lt( "A", "b" ) ) + TF( Lt( "A", " a" ) )
   c += TF( Lt( "A", ( "a" + Chr( 0 ) ) ) ) + TF( Lt( "A", Chr( 0 ) ) ) + TF( Lt( "A", Chr( 200 ) ) )
   c += TF( Lt( "A", "aB" ) )
   ? c
   c := TF( Lt( "ab", "" ) ) + TF( Lt( "ab", " " ) ) + TF( Lt( "ab", "a" ) ) + TF( Lt( "ab", "A" ) )
   c += TF( Lt( "ab", "ab" ) ) + TF( Lt( "ab", "ab " ) ) + TF( Lt( "ab", "abc" ) ) + TF( Lt( "ab", "b" ) )
   c += TF( Lt( "ab", " a" ) ) + TF( Lt( "ab", ( "a" + Chr( 0 ) ) ) ) + TF( Lt( "ab", Chr( 0 ) ) )
   c += TF( Lt( "ab", Chr( 200 ) ) ) + TF( Lt( "ab", "aB" ) )
   ? c
   c := TF( Lt( "ab ", "" ) ) + TF( Lt( "ab ", " " ) ) + TF( Lt( "ab ", "a" ) ) + TF( Lt( "ab ", "A" ) )
   c += TF( Lt( "ab ", "ab" ) ) + TF( Lt( "ab ", "ab " ) ) + TF( Lt( "ab ", "abc" ) ) + TF( Lt( "ab ", "b" ) )
   c += TF( Lt( "ab ", " a" ) ) + TF( Lt( "ab ", ( "a" + Chr( 0 ) ) ) ) + TF( Lt( "ab ", Chr( 0 ) ) )
   c += TF( Lt( "ab ", Chr( 200 ) ) ) + TF( Lt( "ab ", "aB" ) )
   ? c
   c := TF( Lt( "abc", "" ) ) + TF( Lt( "abc", " " ) ) + TF( Lt( "abc", "a" ) ) + TF( Lt( "abc", "A" ) )
   c += TF( Lt( "abc", "ab" ) ) + TF( Lt( "abc", "ab " ) ) + TF( Lt( "abc", "abc" ) ) + TF( Lt( "abc", "b" ) )
   c += TF( Lt( "abc", " a" ) ) + TF( Lt( "abc", ( "a" + Chr( 0 ) ) ) ) + TF( Lt( "abc", Chr( 0 ) ) )
   c += TF( Lt( "abc", Chr( 200 ) ) ) + TF( Lt( "abc", "aB" ) )
   ? c
   c := TF( Lt( "b", "" ) ) + TF( Lt( "b", " " ) ) + TF( Lt( "b", "a" ) ) + TF( Lt( "b", "A" ) ) + TF( Lt( "b", "ab" ) )
   c += TF( Lt( "b", "ab " ) ) + TF( Lt( "b", "abc" ) ) + TF( Lt( "b", "b" ) ) + TF( Lt( "b", " a" ) )
   c += TF( Lt( "b", ( "a" + Chr( 0 ) ) ) ) + TF( Lt( "b", Chr( 0 ) ) ) + TF( Lt( "b", Chr( 200 ) ) )
   c += TF( Lt( "b", "aB" ) )
   ? c
   c := TF( Lt( " a", "" ) ) + TF( Lt( " a", " " ) ) + TF( Lt( " a", "a" ) ) + TF( Lt( " a", "A" ) )
   c += TF( Lt( " a", "ab" ) ) + TF( Lt( " a", "ab " ) ) + TF( Lt( " a", "abc" ) ) + TF( Lt( " a", "b" ) )
   c += TF( Lt( " a", " a" ) ) + TF( Lt( " a", ( "a" + Chr( 0 ) ) ) ) + TF( Lt( " a", Chr( 0 ) ) )
   c += TF( Lt( " a", Chr( 200 ) ) ) + TF( Lt( " a", "aB" ) )
   ? c
   c := TF( Lt( ( "a" + Chr( 0 ) ), "" ) ) + TF( Lt( ( "a" + Chr( 0 ) ), " " ) ) + TF( Lt( ( "a" + Chr( 0 ) ), "a" ) )
   c += TF( Lt( ( "a" + Chr( 0 ) ), "A" ) ) + TF( Lt( ( "a" + Chr( 0 ) ), "ab" ) )
   c += TF( Lt( ( "a" + Chr( 0 ) ), "ab " ) ) + TF( Lt( ( "a" + Chr( 0 ) ), "abc" ) )
   c += TF( Lt( ( "a" + Chr( 0 ) ), "b" ) ) + TF( Lt( ( "a" + Chr( 0 ) ), " a" ) )
   c += TF( Lt( ( "a" + Chr( 0 ) ), ( "a" + Chr( 0 ) ) ) ) + TF( Lt( ( "a" + Chr( 0 ) ), Chr( 0 ) ) )
   c += TF( Lt( ( "a" + Chr( 0 ) ), Chr( 200 ) ) ) + TF( Lt( ( "a" + Chr( 0 ) ), "aB" ) )
   ? c
   c := TF( Lt( Chr( 0 ), "" ) ) + TF( Lt( Chr( 0 ), " " ) ) + TF( Lt( Chr( 0 ), "a" ) ) + TF( Lt( Chr( 0 ), "A" ) )
   c += TF( Lt( Chr( 0 ), "ab" ) ) + TF( Lt( Chr( 0 ), "ab " ) ) + TF( Lt( Chr( 0 ), "abc" ) )
   c += TF( Lt( Chr( 0 ), "b" ) ) + TF( Lt( Chr( 0 ), " a" ) ) + TF( Lt( Chr( 0 ), ( "a" + Chr( 0 ) ) ) )
   c += TF( Lt( Chr( 0 ), Chr( 0 ) ) ) + TF( Lt( Chr( 0 ), Chr( 200 ) ) ) + TF( Lt( Chr( 0 ), "aB" ) )
   ? c
   c := TF( Lt( Chr( 200 ), "" ) ) + TF( Lt( Chr( 200 ), " " ) ) + TF( Lt( Chr( 200 ), "a" ) )
   c += TF( Lt( Chr( 200 ), "A" ) ) + TF( Lt( Chr( 200 ), "ab" ) ) + TF( Lt( Chr( 200 ), "ab " ) )
   c += TF( Lt( Chr( 200 ), "abc" ) ) + TF( Lt( Chr( 200 ), "b" ) ) + TF( Lt( Chr( 200 ), " a" ) )
   c += TF( Lt( Chr( 200 ), ( "a" + Chr( 0 ) ) ) ) + TF( Lt( Chr( 200 ), Chr( 0 ) ) )
   c += TF( Lt( Chr( 200 ), Chr( 200 ) ) ) + TF( Lt( Chr( 200 ), "aB" ) )
   ? c
   c := TF( Lt( "aB", "" ) ) + TF( Lt( "aB", " " ) ) + TF( Lt( "aB", "a" ) ) + TF( Lt( "aB", "A" ) )
   c += TF( Lt( "aB", "ab" ) ) + TF( Lt( "aB", "ab " ) ) + TF( Lt( "aB", "abc" ) ) + TF( Lt( "aB", "b" ) )
   c += TF( Lt( "aB", " a" ) ) + TF( Lt( "aB", ( "a" + Chr( 0 ) ) ) ) + TF( Lt( "aB", Chr( 0 ) ) )
   c += TF( Lt( "aB", Chr( 200 ) ) ) + TF( Lt( "aB", "aB" ) )
   ? c
   ? "runtime < exact"
   Table( aVal, {| a, b | a < b } )
   ? "macro < exact"
   Table( aText, {| a, b | &( a + " < " + b ) } )
   ? "input < exact"
   Table( aIn, {| a, b | a < b } )
RETURN

STATIC PROCEDURE ShowLeExact( aVal, aText, aIn )
   LOCAL c
   ? "literal <= exact"
   c := TF( "" <= "" ) + TF( "" <= " " ) + TF( "" <= "a" ) + TF( "" <= "A" ) + TF( "" <= "ab" ) + TF( "" <= "ab " )
   c += TF( "" <= "abc" ) + TF( "" <= "b" ) + TF( "" <= " a" ) + TF( "" <= ( "a" + Chr( 0 ) ) ) + TF( "" <= Chr( 0 ) )
   c += TF( "" <= Chr( 200 ) ) + TF( "" <= "aB" )
   ? c
   c := TF( " " <= "" ) + TF( " " <= " " ) + TF( " " <= "a" ) + TF( " " <= "A" ) + TF( " " <= "ab" )
   c += TF( " " <= "ab " ) + TF( " " <= "abc" ) + TF( " " <= "b" ) + TF( " " <= " a" ) + TF( " " <= ( "a" + Chr( 0 ) ) )
   c += TF( " " <= Chr( 0 ) ) + TF( " " <= Chr( 200 ) ) + TF( " " <= "aB" )
   ? c
   c := TF( "a" <= "" ) + TF( "a" <= " " ) + TF( "a" <= "a" ) + TF( "a" <= "A" ) + TF( "a" <= "ab" )
   c += TF( "a" <= "ab " ) + TF( "a" <= "abc" ) + TF( "a" <= "b" ) + TF( "a" <= " a" ) + TF( "a" <= ( "a" + Chr( 0 ) ) )
   c += TF( "a" <= Chr( 0 ) ) + TF( "a" <= Chr( 200 ) ) + TF( "a" <= "aB" )
   ? c
   c := TF( "A" <= "" ) + TF( "A" <= " " ) + TF( "A" <= "a" ) + TF( "A" <= "A" ) + TF( "A" <= "ab" )
   c += TF( "A" <= "ab " ) + TF( "A" <= "abc" ) + TF( "A" <= "b" ) + TF( "A" <= " a" ) + TF( "A" <= ( "a" + Chr( 0 ) ) )
   c += TF( "A" <= Chr( 0 ) ) + TF( "A" <= Chr( 200 ) ) + TF( "A" <= "aB" )
   ? c
   c := TF( "ab" <= "" ) + TF( "ab" <= " " ) + TF( "ab" <= "a" ) + TF( "ab" <= "A" ) + TF( "ab" <= "ab" )
   c += TF( "ab" <= "ab " ) + TF( "ab" <= "abc" ) + TF( "ab" <= "b" ) + TF( "ab" <= " a" )
   c += TF( "ab" <= ( "a" + Chr( 0 ) ) ) + TF( "ab" <= Chr( 0 ) ) + TF( "ab" <= Chr( 200 ) ) + TF( "ab" <= "aB" )
   ? c
   c := TF( "ab " <= "" ) + TF( "ab " <= " " ) + TF( "ab " <= "a" ) + TF( "ab " <= "A" ) + TF( "ab " <= "ab" )
   c += TF( "ab " <= "ab " ) + TF( "ab " <= "abc" ) + TF( "ab " <= "b" ) + TF( "ab " <= " a" )
   c += TF( "ab " <= ( "a" + Chr( 0 ) ) ) + TF( "ab " <= Chr( 0 ) ) + TF( "ab " <= Chr( 200 ) ) + TF( "ab " <= "aB" )
   ? c
   c := TF( "abc" <= "" ) + TF( "abc" <= " " ) + TF( "abc" <= "a" ) + TF( "abc" <= "A" ) + TF( "abc" <= "ab" )
   c += TF( "abc" <= "ab " ) + TF( "abc" <= "abc" ) + TF( "abc" <= "b" ) + TF( "abc" <= " a" )
   c += TF( "abc" <= ( "a" + Chr( 0 ) ) ) + TF( "abc" <= Chr( 0 ) ) + TF( "abc" <= Chr( 200 ) ) + TF( "abc" <= "aB" )
   ? c
   c := TF( "b" <= "" ) + TF( "b" <= " " ) + TF( "b" <= "a" ) + TF( "b" <= "A" ) + TF( "b" <= "ab" )
   c += TF( "b" <= "ab " ) + TF( "b" <= "abc" ) + TF( "b" <= "b" ) + TF( "b" <= " a" ) + TF( "b" <= ( "a" + Chr( 0 ) ) )
   c += TF( "b" <= Chr( 0 ) ) + TF( "b" <= Chr( 200 ) ) + TF( "b" <= "aB" )
   ? c
   c := TF( " a" <= "" ) + TF( " a" <= " " ) + TF( " a" <= "a" ) + TF( " a" <= "A" ) + TF( " a" <= "ab" )
   c += TF( " a" <= "ab " ) + TF( " a" <= "abc" ) + TF( " a" <= "b" ) + TF( " a" <= " a" )
   c += TF( " a" <= ( "a" + Chr( 0 ) ) ) + TF( " a" <= Chr( 0 ) ) + TF( " a" <= Chr( 200 ) ) + TF( " a" <= "aB" )
   ? c
   c := TF( ( "a" + Chr( 0 ) ) <= "" ) + TF( ( "a" + Chr( 0 ) ) <= " " ) + TF( ( "a" + Chr( 0 ) ) <= "a" )
   c += TF( ( "a" + Chr( 0 ) ) <= "A" ) + TF( ( "a" + Chr( 0 ) ) <= "ab" ) + TF( ( "a" + Chr( 0 ) ) <= "ab " )
   c += TF( ( "a" + Chr( 0 ) ) <= "abc" ) + TF( ( "a" + Chr( 0 ) ) <= "b" ) + TF( ( "a" + Chr( 0 ) ) <= " a" )
   c += TF( ( "a" + Chr( 0 ) ) <= ( "a" + Chr( 0 ) ) ) + TF( ( "a" + Chr( 0 ) ) <= Chr( 0 ) )
   c += TF( ( "a" + Chr( 0 ) ) <= Chr( 200 ) ) + TF( ( "a" + Chr( 0 ) ) <= "aB" )
   ? c
   c := TF( Chr( 0 ) <= "" ) + TF( Chr( 0 ) <= " " ) + TF( Chr( 0 ) <= "a" ) + TF( Chr( 0 ) <= "A" )
   c += TF( Chr( 0 ) <= "ab" ) + TF( Chr( 0 ) <= "ab " ) + TF( Chr( 0 ) <= "abc" ) + TF( Chr( 0 ) <= "b" )
   c += TF( Chr( 0 ) <= " a" ) + TF( Chr( 0 ) <= ( "a" + Chr( 0 ) ) ) + TF( Chr( 0 ) <= Chr( 0 ) )
   c += TF( Chr( 0 ) <= Chr( 200 ) ) + TF( Chr( 0 ) <= "aB" )
   ? c
   c := TF( Chr( 200 ) <= "" ) + TF( Chr( 200 ) <= " " ) + TF( Chr( 200 ) <= "a" ) + TF( Chr( 200 ) <= "A" )
   c += TF( Chr( 200 ) <= "ab" ) + TF( Chr( 200 ) <= "ab " ) + TF( Chr( 200 ) <= "abc" ) + TF( Chr( 200 ) <= "b" )
   c += TF( Chr( 200 ) <= " a" ) + TF( Chr( 200 ) <= ( "a" + Chr( 0 ) ) ) + TF( Chr( 200 ) <= Chr( 0 ) )
   c += TF( Chr( 200 ) <= Chr( 200 ) ) + TF( Chr( 200 ) <= "aB" )
   ? c
   c := TF( "aB" <= "" ) + TF( "aB" <= " " ) + TF( "aB" <= "a" ) + TF( "aB" <= "A" ) + TF( "aB" <= "ab" )
   c += TF( "aB" <= "ab " ) + TF( "aB" <= "abc" ) + TF( "aB" <= "b" ) + TF( "aB" <= " a" )
   c += TF( "aB" <= ( "a" + Chr( 0 ) ) ) + TF( "aB" <= Chr( 0 ) ) + TF( "aB" <= Chr( 200 ) ) + TF( "aB" <= "aB" )
   ? c
   ? "inlined <= exact"
   c := TF( Le( "", "" ) ) + TF( Le( "", " " ) ) + TF( Le( "", "a" ) ) + TF( Le( "", "A" ) ) + TF( Le( "", "ab" ) )
   c += TF( Le( "", "ab " ) ) + TF( Le( "", "abc" ) ) + TF( Le( "", "b" ) ) + TF( Le( "", " a" ) )
   c += TF( Le( "", ( "a" + Chr( 0 ) ) ) ) + TF( Le( "", Chr( 0 ) ) ) + TF( Le( "", Chr( 200 ) ) )
   c += TF( Le( "", "aB" ) )
   ? c
   c := TF( Le( " ", "" ) ) + TF( Le( " ", " " ) ) + TF( Le( " ", "a" ) ) + TF( Le( " ", "A" ) ) + TF( Le( " ", "ab" ) )
   c += TF( Le( " ", "ab " ) ) + TF( Le( " ", "abc" ) ) + TF( Le( " ", "b" ) ) + TF( Le( " ", " a" ) )
   c += TF( Le( " ", ( "a" + Chr( 0 ) ) ) ) + TF( Le( " ", Chr( 0 ) ) ) + TF( Le( " ", Chr( 200 ) ) )
   c += TF( Le( " ", "aB" ) )
   ? c
   c := TF( Le( "a", "" ) ) + TF( Le( "a", " " ) ) + TF( Le( "a", "a" ) ) + TF( Le( "a", "A" ) ) + TF( Le( "a", "ab" ) )
   c += TF( Le( "a", "ab " ) ) + TF( Le( "a", "abc" ) ) + TF( Le( "a", "b" ) ) + TF( Le( "a", " a" ) )
   c += TF( Le( "a", ( "a" + Chr( 0 ) ) ) ) + TF( Le( "a", Chr( 0 ) ) ) + TF( Le( "a", Chr( 200 ) ) )
   c += TF( Le( "a", "aB" ) )
   ? c
   c := TF( Le( "A", "" ) ) + TF( Le( "A", " " ) ) + TF( Le( "A", "a" ) ) + TF( Le( "A", "A" ) ) + TF( Le( "A", "ab" ) )
   c += TF( Le( "A", "ab " ) ) + TF( Le( "A", "abc" ) ) + TF( Le( "A", "b" ) ) + TF( Le( "A", " a" ) )
   c += TF( Le( "A", ( "a" + Chr( 0 ) ) ) ) + TF( Le( "A", Chr( 0 ) ) ) + TF( Le( "A", Chr( 200 ) ) )
   c += TF( Le( "A", "aB" ) )
   ? c
   c := TF( Le( "ab", "" ) ) + TF( Le( "ab", " " ) ) + TF( Le( "ab", "a" ) ) + TF( Le( "ab", "A" ) )
   c += TF( Le( "ab", "ab" ) ) + TF( Le( "ab", "ab " ) ) + TF( Le( "ab", "abc" ) ) + TF( Le( "ab", "b" ) )
   c += TF( Le( "ab", " a" ) ) + TF( Le( "ab", ( "a" + Chr( 0 ) ) ) ) + TF( Le( "ab", Chr( 0 ) ) )
   c += TF( Le( "ab", Chr( 200 ) ) ) + TF( Le( "ab", "aB" ) )
   ? c
   c := TF( Le( "ab ", "" ) ) + TF( Le( "ab ", " " ) ) + TF( Le( "ab ", "a" ) ) + TF( Le( "ab ", "A" ) )
   c += TF( Le( "ab ", "ab" ) ) + TF( Le( "ab ", "ab " ) ) + TF( Le( "ab ", "abc" ) ) + TF( Le( "ab ", "b" ) )
   c += TF( Le( "ab ", " a" ) ) + TF( Le( "ab ", ( "a" + Chr( 0 ) ) ) ) + TF( Le( "ab ", Chr( 0 ) ) )
   c += TF( Le( "ab ", Chr( 200 ) ) ) + TF( Le( "ab ", "aB" ) )
   ? c
   c := TF( Le( "abc", "" ) ) + TF( Le( "abc", " " ) ) + TF( Le( "abc", "a" ) ) + TF( Le( "abc", "A" ) )
   c += TF( Le( "abc", "ab" ) ) + TF( Le( "abc", "ab " ) ) + TF( Le( "abc", "abc" ) ) + TF( Le( "abc", "b" ) )
   c += TF( Le( "abc", " a" ) ) + TF( Le( "abc", ( "a" + Chr( 0 ) ) ) ) + TF( Le( "abc", Chr( 0 ) ) )
   c += TF( Le( "abc", Chr( 200 ) ) ) + TF( Le( "abc", "aB" ) )
   ? c
   c := TF( Le( "b", "" ) ) + TF( Le( "b", " " ) ) + TF( Le( "b", "a" ) ) + TF( Le( "b", "A" ) ) + TF( Le( "b", "ab" ) )
   c += TF( Le( "b", "ab " ) ) + TF( Le( "b", "abc" ) ) + TF( Le( "b", "b" ) ) + TF( Le( "b", " a" ) )
   c += TF( Le( "b", ( "a" + Chr( 0 ) ) ) ) + TF( Le( "b", Chr( 0 ) ) ) + TF( Le( "b", Chr( 200 ) ) )
   c += TF( Le( "b", "aB" ) )
   ? c
   c := TF( Le( " a", "" ) ) + TF( Le( " a", " " ) ) + TF( Le( " a", "a" ) ) + TF( Le( " a", "A" ) )
   c += TF( Le( " a", "ab" ) ) + TF( Le( " a", "ab " ) ) + TF( Le( " a", "abc" ) ) + TF( Le( " a", "b" ) )
   c += TF( Le( " a", " a" ) ) + TF( Le( " a", ( "a" + Chr( 0 ) ) ) ) + TF( Le( " a", Chr( 0 ) ) )
   c += TF( Le( " a", Chr( 200 ) ) ) + TF( Le( " a", "aB" ) )
   ? c
   c := TF( Le( ( "a" + Chr( 0 ) ), "" ) ) + TF( Le( ( "a" + Chr( 0 ) ), " " ) ) + TF( Le( ( "a" + Chr( 0 ) ), "a" ) )
   c += TF( Le( ( "a" + Chr( 0 ) ), "A" ) ) + TF( Le( ( "a" + Chr( 0 ) ), "ab" ) )
   c += TF( Le( ( "a" + Chr( 0 ) ), "ab " ) ) + TF( Le( ( "a" + Chr( 0 ) ), "abc" ) )
   c += TF( Le( ( "a" + Chr( 0 ) ), "b" ) ) + TF( Le( ( "a" + Chr( 0 ) ), " a" ) )
   c += TF( Le( ( "a" + Chr( 0 ) ), ( "a" + Chr( 0 ) ) ) ) + TF( Le( ( "a" + Chr( 0 ) ), Chr( 0 ) ) )
   c += TF( Le( ( "a" + Chr( 0 ) ), Chr( 200 ) ) ) + TF( Le( ( "a" + Chr( 0 ) ), "aB" ) )
   ? c
   c := TF( Le( Chr( 0 ), "" ) ) + TF( Le( Chr( 0 ), " " ) ) + TF( Le( Chr( 0 ), "a" ) ) + TF( Le( Chr( 0 ), "A" ) )
   c += TF( Le( Chr( 0 ), "ab" ) ) + TF( Le( Chr( 0 ), "ab " ) ) + TF( Le( Chr( 0 ), "abc" ) )
   c += TF( Le( Chr( 0 ), "b" ) ) + TF( Le( Chr( 0 ), " a" ) ) + TF( Le( Chr( 0 ), ( "a" + Chr( 0 ) ) ) )
   c += TF( Le( Chr( 0 ), Chr( 0 ) ) ) + TF( Le( Chr( 0 ), Chr( 200 ) ) ) + TF( Le( Chr( 0 ), "aB" ) )
   ? c
   c := TF( Le( Chr( 200 ), "" ) ) + TF( Le( Chr( 200 ), " " ) ) + TF( Le( Chr( 200 ), "a" ) )
   c += TF( Le( Chr( 200 ), "A" ) ) + TF( Le( Chr( 200 ), "ab" ) ) + TF( Le( Chr( 200 ), "ab " ) )
   c += TF( Le( Chr( 200 ), "abc" ) ) + TF( Le( Chr( 200 ), "b" ) ) + TF( Le( Chr( 200 ), " a" ) )
   c += TF( Le( Chr( 200 ), ( "a" + Chr( 0 ) ) ) ) + TF( Le( Chr( 200 ), Chr( 0 ) ) )
   c += TF( Le( Chr( 200 ), Chr( 200 ) ) ) + TF( Le( Chr( 200 ), "aB" ) )
   ? c
   c := TF( Le( "aB", "" ) ) + TF( Le( "aB", " " ) ) + TF( Le( "aB", "a" ) ) + TF( Le( "aB", "A" ) )
   c += TF( Le( "aB", "ab" ) ) + TF( Le( "aB", "ab " ) ) + TF( Le( "aB", "abc" ) ) + TF( Le( "aB", "b" ) )
   c += TF( Le( "aB", " a" ) ) + TF( Le( "aB", ( "a" + Chr( 0 ) ) ) ) + TF( Le( "aB", Chr( 0 ) ) )
   c += TF( Le( "aB", Chr( 200 ) ) ) + TF( Le( "aB", "aB" ) )
   ? c
   ? "runtime <= exact"
   Table( aVal, {| a, b | a <= b } )
   ? "macro <= exact"
   Table( aText, {| a, b | &( a + " <= " + b ) } )
   ? "input <= exact"
   Table( aIn, {| a, b | a <= b } )
RETURN

STATIC PROCEDURE ShowGtExact( aVal, aText, aIn )
   LOCAL c
   ? "literal > exact"
   c := TF( "" > "" ) + TF( "" > " " ) + TF( "" > "a" ) + TF( "" > "A" ) + TF( "" > "ab" ) + TF( "" > "ab " )
   c += TF( "" > "abc" ) + TF( "" > "b" ) + TF( "" > " a" ) + TF( "" > ( "a" + Chr( 0 ) ) ) + TF( "" > Chr( 0 ) )
   c += TF( "" > Chr( 200 ) ) + TF( "" > "aB" )
   ? c
   c := TF( " " > "" ) + TF( " " > " " ) + TF( " " > "a" ) + TF( " " > "A" ) + TF( " " > "ab" ) + TF( " " > "ab " )
   c += TF( " " > "abc" ) + TF( " " > "b" ) + TF( " " > " a" ) + TF( " " > ( "a" + Chr( 0 ) ) ) + TF( " " > Chr( 0 ) )
   c += TF( " " > Chr( 200 ) ) + TF( " " > "aB" )
   ? c
   c := TF( "a" > "" ) + TF( "a" > " " ) + TF( "a" > "a" ) + TF( "a" > "A" ) + TF( "a" > "ab" ) + TF( "a" > "ab " )
   c += TF( "a" > "abc" ) + TF( "a" > "b" ) + TF( "a" > " a" ) + TF( "a" > ( "a" + Chr( 0 ) ) ) + TF( "a" > Chr( 0 ) )
   c += TF( "a" > Chr( 200 ) ) + TF( "a" > "aB" )
   ? c
   c := TF( "A" > "" ) + TF( "A" > " " ) + TF( "A" > "a" ) + TF( "A" > "A" ) + TF( "A" > "ab" ) + TF( "A" > "ab " )
   c += TF( "A" > "abc" ) + TF( "A" > "b" ) + TF( "A" > " a" ) + TF( "A" > ( "a" + Chr( 0 ) ) ) + TF( "A" > Chr( 0 ) )
   c += TF( "A" > Chr( 200 ) ) + TF( "A" > "aB" )
   ? c
   c := TF( "ab" > "" ) + TF( "ab" > " " ) + TF( "ab" > "a" ) + TF( "ab" > "A" ) + TF( "ab" > "ab" )
   c += TF( "ab" > "ab " ) + TF( "ab" > "abc" ) + TF( "ab" > "b" ) + TF( "ab" > " a" ) + TF( "ab" > ( "a" + Chr( 0 ) ) )
   c += TF( "ab" > Chr( 0 ) ) + TF( "ab" > Chr( 200 ) ) + TF( "ab" > "aB" )
   ? c
   c := TF( "ab " > "" ) + TF( "ab " > " " ) + TF( "ab " > "a" ) + TF( "ab " > "A" ) + TF( "ab " > "ab" )
   c += TF( "ab " > "ab " ) + TF( "ab " > "abc" ) + TF( "ab " > "b" ) + TF( "ab " > " a" )
   c += TF( "ab " > ( "a" + Chr( 0 ) ) ) + TF( "ab " > Chr( 0 ) ) + TF( "ab " > Chr( 200 ) ) + TF( "ab " > "aB" )
   ? c
   c := TF( "abc" > "" ) + TF( "abc" > " " ) + TF( "abc" > "a" ) + TF( "abc" > "A" ) + TF( "abc" > "ab" )
   c += TF( "abc" > "ab " ) + TF( "abc" > "abc" ) + TF( "abc" > "b" ) + TF( "abc" > " a" )
   c += TF( "abc" > ( "a" + Chr( 0 ) ) ) + TF( "abc" > Chr( 0 ) ) + TF( "abc" > Chr( 200 ) ) + TF( "abc" > "aB" )
   ? c
   c := TF( "b" > "" ) + TF( "b" > " " ) + TF( "b" > "a" ) + TF( "b" > "A" ) + TF( "b" > "ab" ) + TF( "b" > "ab " )
   c += TF( "b" > "abc" ) + TF( "b" > "b" ) + TF( "b" > " a" ) + TF( "b" > ( "a" + Chr( 0 ) ) ) + TF( "b" > Chr( 0 ) )
   c += TF( "b" > Chr( 200 ) ) + TF( "b" > "aB" )
   ? c
   c := TF( " a" > "" ) + TF( " a" > " " ) + TF( " a" > "a" ) + TF( " a" > "A" ) + TF( " a" > "ab" )
   c += TF( " a" > "ab " ) + TF( " a" > "abc" ) + TF( " a" > "b" ) + TF( " a" > " a" ) + TF( " a" > ( "a" + Chr( 0 ) ) )
   c += TF( " a" > Chr( 0 ) ) + TF( " a" > Chr( 200 ) ) + TF( " a" > "aB" )
   ? c
   c := TF( ( "a" + Chr( 0 ) ) > "" ) + TF( ( "a" + Chr( 0 ) ) > " " ) + TF( ( "a" + Chr( 0 ) ) > "a" )
   c += TF( ( "a" + Chr( 0 ) ) > "A" ) + TF( ( "a" + Chr( 0 ) ) > "ab" ) + TF( ( "a" + Chr( 0 ) ) > "ab " )
   c += TF( ( "a" + Chr( 0 ) ) > "abc" ) + TF( ( "a" + Chr( 0 ) ) > "b" ) + TF( ( "a" + Chr( 0 ) ) > " a" )
   c += TF( ( "a" + Chr( 0 ) ) > ( "a" + Chr( 0 ) ) ) + TF( ( "a" + Chr( 0 ) ) > Chr( 0 ) )
   c += TF( ( "a" + Chr( 0 ) ) > Chr( 200 ) ) + TF( ( "a" + Chr( 0 ) ) > "aB" )
   ? c
   c := TF( Chr( 0 ) > "" ) + TF( Chr( 0 ) > " " ) + TF( Chr( 0 ) > "a" ) + TF( Chr( 0 ) > "A" ) + TF( Chr( 0 ) > "ab" )
   c += TF( Chr( 0 ) > "ab " ) + TF( Chr( 0 ) > "abc" ) + TF( Chr( 0 ) > "b" ) + TF( Chr( 0 ) > " a" )
   c += TF( Chr( 0 ) > ( "a" + Chr( 0 ) ) ) + TF( Chr( 0 ) > Chr( 0 ) ) + TF( Chr( 0 ) > Chr( 200 ) )
   c += TF( Chr( 0 ) > "aB" )
   ? c
   c := TF( Chr( 200 ) > "" ) + TF( Chr( 200 ) > " " ) + TF( Chr( 200 ) > "a" ) + TF( Chr( 200 ) > "A" )
   c += TF( Chr( 200 ) > "ab" ) + TF( Chr( 200 ) > "ab " ) + TF( Chr( 200 ) > "abc" ) + TF( Chr( 200 ) > "b" )
   c += TF( Chr( 200 ) > " a" ) + TF( Chr( 200 ) > ( "a" + Chr( 0 ) ) ) + TF( Chr( 200 ) > Chr( 0 ) )
   c += TF( Chr( 200 ) > Chr( 200 ) ) + TF( Chr( 200 ) > "aB" )
   ? c
   c := TF( "aB" > "" ) + TF( "aB" > " " ) + TF( "aB" > "a" ) + TF( "aB" > "A" ) + TF( "aB" > "ab" )
   c += TF( "aB" > "ab " ) + TF( "aB" > "abc" ) + TF( "aB" > "b" ) + TF( "aB" > " a" ) + TF( "aB" > ( "a" + Chr( 0 ) ) )
   c += TF( "aB" > Chr( 0 ) ) + TF( "aB" > Chr( 200 ) ) + TF( "aB" > "aB" )
   ? c
   ? "inlined > exact"
   c := TF( Gt( "", "" ) ) + TF( Gt( "", " " ) ) + TF( Gt( "", "a" ) ) + TF( Gt( "", "A" ) ) + TF( Gt( "", "ab" ) )
   c += TF( Gt( "", "ab " ) ) + TF( Gt( "", "abc" ) ) + TF( Gt( "", "b" ) ) + TF( Gt( "", " a" ) )
   c += TF( Gt( "", ( "a" + Chr( 0 ) ) ) ) + TF( Gt( "", Chr( 0 ) ) ) + TF( Gt( "", Chr( 200 ) ) )
   c += TF( Gt( "", "aB" ) )
   ? c
   c := TF( Gt( " ", "" ) ) + TF( Gt( " ", " " ) ) + TF( Gt( " ", "a" ) ) + TF( Gt( " ", "A" ) ) + TF( Gt( " ", "ab" ) )
   c += TF( Gt( " ", "ab " ) ) + TF( Gt( " ", "abc" ) ) + TF( Gt( " ", "b" ) ) + TF( Gt( " ", " a" ) )
   c += TF( Gt( " ", ( "a" + Chr( 0 ) ) ) ) + TF( Gt( " ", Chr( 0 ) ) ) + TF( Gt( " ", Chr( 200 ) ) )
   c += TF( Gt( " ", "aB" ) )
   ? c
   c := TF( Gt( "a", "" ) ) + TF( Gt( "a", " " ) ) + TF( Gt( "a", "a" ) ) + TF( Gt( "a", "A" ) ) + TF( Gt( "a", "ab" ) )
   c += TF( Gt( "a", "ab " ) ) + TF( Gt( "a", "abc" ) ) + TF( Gt( "a", "b" ) ) + TF( Gt( "a", " a" ) )
   c += TF( Gt( "a", ( "a" + Chr( 0 ) ) ) ) + TF( Gt( "a", Chr( 0 ) ) ) + TF( Gt( "a", Chr( 200 ) ) )
   c += TF( Gt( "a", "aB" ) )
   ? c
   c := TF( Gt( "A", "" ) ) + TF( Gt( "A", " " ) ) + TF( Gt( "A", "a" ) ) + TF( Gt( "A", "A" ) ) + TF( Gt( "A", "ab" ) )
   c += TF( Gt( "A", "ab " ) ) + TF( Gt( "A", "abc" ) ) + TF( Gt( "A", "b" ) ) + TF( Gt( "A", " a" ) )
   c += TF( Gt( "A", ( "a" + Chr( 0 ) ) ) ) + TF( Gt( "A", Chr( 0 ) ) ) + TF( Gt( "A", Chr( 200 ) ) )
   c += TF( Gt( "A", "aB" ) )
   ? c
   c := TF( Gt( "ab", "" ) ) + TF( Gt( "ab", " " ) ) + TF( Gt( "ab", "a" ) ) + TF( Gt( "ab", "A" ) )
   c += TF( Gt( "ab", "ab" ) ) + TF( Gt( "ab", "ab " ) ) + TF( Gt( "ab", "abc" ) ) + TF( Gt( "ab", "b" ) )
   c += TF( Gt( "ab", " a" ) ) + TF( Gt( "ab", ( "a" + Chr( 0 ) ) ) ) + TF( Gt( "ab", Chr( 0 ) ) )
   c += TF( Gt( "ab", Chr( 200 ) ) ) + TF( Gt( "ab", "aB" ) )
   ? c
   c := TF( Gt( "ab ", "" ) ) + TF( Gt( "ab ", " " ) ) + TF( Gt( "ab ", "a" ) ) + TF( Gt( "ab ", "A" ) )
   c += TF( Gt( "ab ", "ab" ) ) + TF( Gt( "ab ", "ab " ) ) + TF( Gt( "ab ", "abc" ) ) + TF( Gt( "ab ", "b" ) )
   c += TF( Gt( "ab ", " a" ) ) + TF( Gt( "ab ", ( "a" + Chr( 0 ) ) ) ) + TF( Gt( "ab ", Chr( 0 ) ) )
   c += TF( Gt( "ab ", Chr( 200 ) ) ) + TF( Gt( "ab ", "aB" ) )
   ? c
   c := TF( Gt( "abc", "" ) ) + TF( Gt( "abc", " " ) ) + TF( Gt( "abc", "a" ) ) + TF( Gt( "abc", "A" ) )
   c += TF( Gt( "abc", "ab" ) ) + TF( Gt( "abc", "ab " ) ) + TF( Gt( "abc", "abc" ) ) + TF( Gt( "abc", "b" ) )
   c += TF( Gt( "abc", " a" ) ) + TF( Gt( "abc", ( "a" + Chr( 0 ) ) ) ) + TF( Gt( "abc", Chr( 0 ) ) )
   c += TF( Gt( "abc", Chr( 200 ) ) ) + TF( Gt( "abc", "aB" ) )
   ? c
   c := TF( Gt( "b", "" ) ) + TF( Gt( "b", " " ) ) + TF( Gt( "b", "a" ) ) + TF( Gt( "b", "A" ) ) + TF( Gt( "b", "ab" ) )
   c += TF( Gt( "b", "ab " ) ) + TF( Gt( "b", "abc" ) ) + TF( Gt( "b", "b" ) ) + TF( Gt( "b", " a" ) )
   c += TF( Gt( "b", ( "a" + Chr( 0 ) ) ) ) + TF( Gt( "b", Chr( 0 ) ) ) + TF( Gt( "b", Chr( 200 ) ) )
   c += TF( Gt( "b", "aB" ) )
   ? c
   c := TF( Gt( " a", "" ) ) + TF( Gt( " a", " " ) ) + TF( Gt( " a", "a" ) ) + TF( Gt( " a", "A" ) )
   c += TF( Gt( " a", "ab" ) ) + TF( Gt( " a", "ab " ) ) + TF( Gt( " a", "abc" ) ) + TF( Gt( " a", "b" ) )
   c += TF( Gt( " a", " a" ) ) + TF( Gt( " a", ( "a" + Chr( 0 ) ) ) ) + TF( Gt( " a", Chr( 0 ) ) )
   c += TF( Gt( " a", Chr( 200 ) ) ) + TF( Gt( " a", "aB" ) )
   ? c
   c := TF( Gt( ( "a" + Chr( 0 ) ), "" ) ) + TF( Gt( ( "a" + Chr( 0 ) ), " " ) ) + TF( Gt( ( "a" + Chr( 0 ) ), "a" ) )
   c += TF( Gt( ( "a" + Chr( 0 ) ), "A" ) ) + TF( Gt( ( "a" + Chr( 0 ) ), "ab" ) )
   c += TF( Gt( ( "a" + Chr( 0 ) ), "ab " ) ) + TF( Gt( ( "a" + Chr( 0 ) ), "abc" ) )
   c += TF( Gt( ( "a" + Chr( 0 ) ), "b" ) ) + TF( Gt( ( "a" + Chr( 0 ) ), " a" ) )
   c += TF( Gt( ( "a" + Chr( 0 ) ), ( "a" + Chr( 0 ) ) ) ) + TF( Gt( ( "a" + Chr( 0 ) ), Chr( 0 ) ) )
   c += TF( Gt( ( "a" + Chr( 0 ) ), Chr( 200 ) ) ) + TF( Gt( ( "a" + Chr( 0 ) ), "aB" ) )
   ? c
   c := TF( Gt( Chr( 0 ), "" ) ) + TF( Gt( Chr( 0 ), " " ) ) + TF( Gt( Chr( 0 ), "a" ) ) + TF( Gt( Chr( 0 ), "A" ) )
   c += TF( Gt( Chr( 0 ), "ab" ) ) + TF( Gt( Chr( 0 ), "ab " ) ) + TF( Gt( Chr( 0 ), "abc" ) )
   c += TF( Gt( Chr( 0 ), "b" ) ) + TF( Gt( Chr( 0 ), " a" ) ) + TF( Gt( Chr( 0 ), ( "a" + Chr( 0 ) ) ) )
   c += TF( Gt( Chr( 0 ), Chr( 0 ) ) ) + TF( Gt( Chr( 0 ), Chr( 200 ) ) ) + TF( Gt( Chr( 0 ), "aB" ) )
   ? c
   c := TF( Gt( Chr( 200 ), "" ) ) + TF( Gt( Chr( 200 ), " " ) ) + TF( Gt( Chr( 200 ), "a" ) )
   c += TF( Gt( Chr( 200 ), "A" ) ) + TF( Gt( Chr( 200 ), "ab" ) ) + TF( Gt( Chr( 200 ), "ab " ) )
   c += TF( Gt( Chr( 200 ), "abc" ) ) + TF( Gt( Chr( 200 ), "b" ) ) + TF( Gt( Chr( 200 ), " a" ) )
   c += TF( Gt( Chr( 200 ), ( "a" + Chr( 0 ) ) ) ) + TF( Gt( Chr( 200 ), Chr( 0 ) ) )
   c += TF( Gt( Chr( 200 ), Chr( 200 ) ) ) + TF( Gt( Chr( 200 ), "aB" ) )
   ? c
   c := TF( Gt( "aB", "" ) ) + TF( Gt( "aB", " " ) ) + TF( Gt( "aB", "a" ) ) + TF( Gt( "aB", "A" ) )
   c += TF( Gt( "aB", "ab" ) ) + TF( Gt( "aB", "ab " ) ) + TF( Gt( "aB", "abc" ) ) + TF( Gt( "aB", "b" ) )
   c += TF( Gt( "aB", " a" ) ) + TF( Gt( "aB", ( "a" + Chr( 0 ) ) ) ) + TF( Gt( "aB", Chr( 0 ) ) )
   c += TF( Gt( "aB", Chr( 200 ) ) ) + TF( Gt( "aB", "aB" ) )
   ? c
   ? "runtime > exact"
   Table( aVal, {| a, b | a > b } )
   ? "macro > exact"
   Table( aText, {| a, b | &( a + " > " + b ) } )
   ? "input > exact"
   Table( aIn, {| a, b | a > b } )
RETURN

STATIC PROCEDURE ShowGeExact( aVal, aText, aIn )
   LOCAL c
   ? "literal >= exact"
   c := TF( "" >= "" ) + TF( "" >= " " ) + TF( "" >= "a" ) + TF( "" >= "A" ) + TF( "" >= "ab" ) + TF( "" >= "ab " )
   c += TF( "" >= "abc" ) + TF( "" >= "b" ) + TF( "" >= " a" ) + TF( "" >= ( "a" + Chr( 0 ) ) ) + TF( "" >= Chr( 0 ) )
   c += TF( "" >= Chr( 200 ) ) + TF( "" >= "aB" )
   ? c
   c := TF( " " >= "" ) + TF( " " >= " " ) + TF( " " >= "a" ) + TF( " " >= "A" ) + TF( " " >= "ab" )
   c += TF( " " >= "ab " ) + TF( " " >= "abc" ) + TF( " " >= "b" ) + TF( " " >= " a" ) + TF( " " >= ( "a" + Chr( 0 ) ) )
   c += TF( " " >= Chr( 0 ) ) + TF( " " >= Chr( 200 ) ) + TF( " " >= "aB" )
   ? c
   c := TF( "a" >= "" ) + TF( "a" >= " " ) + TF( "a" >= "a" ) + TF( "a" >= "A" ) + TF( "a" >= "ab" )
   c += TF( "a" >= "ab " ) + TF( "a" >= "abc" ) + TF( "a" >= "b" ) + TF( "a" >= " a" ) + TF( "a" >= ( "a" + Chr( 0 ) ) )
   c += TF( "a" >= Chr( 0 ) ) + TF( "a" >= Chr( 200 ) ) + TF( "a" >= "aB" )
   ? c
   c := TF( "A" >= "" ) + TF( "A" >= " " ) + TF( "A" >= "a" ) + TF( "A" >= "A" ) + TF( "A" >= "ab" )
   c += TF( "A" >= "ab " ) + TF( "A" >= "abc" ) + TF( "A" >= "b" ) + TF( "A" >= " a" ) + TF( "A" >= ( "a" + Chr( 0 ) ) )
   c += TF( "A" >= Chr( 0 ) ) + TF( "A" >= Chr( 200 ) ) + TF( "A" >= "aB" )
   ? c
   c := TF( "ab" >= "" ) + TF( "ab" >= " " ) + TF( "ab" >= "a" ) + TF( "ab" >= "A" ) + TF( "ab" >= "ab" )
   c += TF( "ab" >= "ab " ) + TF( "ab" >= "abc" ) + TF( "ab" >= "b" ) + TF( "ab" >= " a" )
   c += TF( "ab" >= ( "a" + Chr( 0 ) ) ) + TF( "ab" >= Chr( 0 ) ) + TF( "ab" >= Chr( 200 ) ) + TF( "ab" >= "aB" )
   ? c
   c := TF( "ab " >= "" ) + TF( "ab " >= " " ) + TF( "ab " >= "a" ) + TF( "ab " >= "A" ) + TF( "ab " >= "ab" )
   c += TF( "ab " >= "ab " ) + TF( "ab " >= "abc" ) + TF( "ab " >= "b" ) + TF( "ab " >= " a" )
   c += TF( "ab " >= ( "a" + Chr( 0 ) ) ) + TF( "ab " >= Chr( 0 ) ) + TF( "ab " >= Chr( 200 ) ) + TF( "ab " >= "aB" )
   ? c
   c := TF( "abc" >= "" ) + TF( "abc" >= " " ) + TF( "abc" >= "a" ) + TF( "abc" >= "A" ) + TF( "abc" >= "ab" )
   c += TF( "abc" >= "ab " ) + TF( "abc" >= "abc" ) + TF( "abc" >= "b" ) + TF( "abc" >= " a" )
   c += TF( "abc" >= ( "a" + Chr( 0 ) ) ) + TF( "abc" >= Chr( 0 ) ) + TF( "abc" >= Chr( 200 ) ) + TF( "abc" >= "aB" )
   ? c
   c := TF( "b" >= "" ) + TF( "b" >= " " ) + TF( "b" >= "a" ) + TF( "b" >= "A" ) + TF( "b" >= "ab" )
   c += TF( "b" >= "ab " ) + TF( "b" >= "abc" ) + TF( "b" >= "b" ) + TF( "b" >= " a" ) + TF( "b" >= ( "a" + Chr( 0 ) ) )
   c += TF( "b" >= Chr( 0 ) ) + TF( "b" >= Chr( 200 ) ) + TF( "b" >= "aB" )
   ? c
   c := TF( " a" >= "" ) + TF( " a" >= " " ) + TF( " a" >= "a" ) + TF( " a" >= "A" ) + TF( " a" >= "ab" )
   c += TF( " a" >= "ab " ) + TF( " a" >= "abc" ) + TF( " a" >= "b" ) + TF( " a" >= " a" )
   c += TF( " a" >= ( "a" + Chr( 0 ) ) ) + TF( " a" >= Chr( 0 ) ) + TF( " a" >= Chr( 200 ) ) + TF( " a" >= "aB" )
   ? c
   c := TF( ( "a" + Chr( 0 ) ) >= "" ) + TF( ( "a" + Chr( 0 ) ) >= " " ) + TF( ( "a" + Chr( 0 ) ) >= "a" )
   c += TF( ( "a" + Chr( 0 ) ) >= "A" ) + TF( ( "a" + Chr( 0 ) ) >= "ab" ) + TF( ( "a" + Chr( 0 ) ) >= "ab " )
   c += TF( ( "a" + Chr( 0 ) ) >= "abc" ) + TF( ( "a" + Chr( 0 ) ) >= "b" ) + TF( ( "a" + Chr( 0 ) ) >= " a" )
   c += TF( ( "a" + Chr( 0 ) ) >= ( "a" + Chr( 0 ) ) ) + TF( ( "a" + Chr( 0 ) ) >= Chr( 0 ) )
   c += TF( ( "a" + Chr( 0 ) ) >= Chr( 200 ) ) + TF( ( "a" + Chr( 0 ) ) >= "aB" )
   ? c
   c := TF( Chr( 0 ) >= "" ) + TF( Chr( 0 ) >= " " ) + TF( Chr( 0 ) >= "a" ) + TF( Chr( 0 ) >= "A" )
   c += TF( Chr( 0 ) >= "ab" ) + TF( Chr( 0 ) >= "ab " ) + TF( Chr( 0 ) >= "abc" ) + TF( Chr( 0 ) >= "b" )
   c += TF( Chr( 0 ) >= " a" ) + TF( Chr( 0 ) >= ( "a" + Chr( 0 ) ) ) + TF( Chr( 0 ) >= Chr( 0 ) )
   c += TF( Chr( 0 ) >= Chr( 200 ) ) + TF( Chr( 0 ) >= "aB" )
   ? c
   c := TF( Chr( 200 ) >= "" ) + TF( Chr( 200 ) >= " " ) + TF( Chr( 200 ) >= "a" ) + TF( Chr( 200 ) >= "A" )
   c += TF( Chr( 200 ) >= "ab" ) + TF( Chr( 200 ) >= "ab " ) + TF( Chr( 200 ) >= "abc" ) + TF( Chr( 200 ) >= "b" )
   c += TF( Chr( 200 ) >= " a" ) + TF( Chr( 200 ) >= ( "a" + Chr( 0 ) ) ) + TF( Chr( 200 ) >= Chr( 0 ) )
   c += TF( Chr( 200 ) >= Chr( 200 ) ) + TF( Chr( 200 ) >= "aB" )
   ? c
   c := TF( "aB" >= "" ) + TF( "aB" >= " " ) + TF( "aB" >= "a" ) + TF( "aB" >= "A" ) + TF( "aB" >= "ab" )
   c += TF( "aB" >= "ab " ) + TF( "aB" >= "abc" ) + TF( "aB" >= "b" ) + TF( "aB" >= " a" )
   c += TF( "aB" >= ( "a" + Chr( 0 ) ) ) + TF( "aB" >= Chr( 0 ) ) + TF( "aB" >= Chr( 200 ) ) + TF( "aB" >= "aB" )
   ? c
   ? "inlined >= exact"
   c := TF( Ge( "", "" ) ) + TF( Ge( "", " " ) ) + TF( Ge( "", "a" ) ) + TF( Ge( "", "A" ) ) + TF( Ge( "", "ab" ) )
   c += TF( Ge( "", "ab " ) ) + TF( Ge( "", "abc" ) ) + TF( Ge( "", "b" ) ) + TF( Ge( "", " a" ) )
   c += TF( Ge( "", ( "a" + Chr( 0 ) ) ) ) + TF( Ge( "", Chr( 0 ) ) ) + TF( Ge( "", Chr( 200 ) ) )
   c += TF( Ge( "", "aB" ) )
   ? c
   c := TF( Ge( " ", "" ) ) + TF( Ge( " ", " " ) ) + TF( Ge( " ", "a" ) ) + TF( Ge( " ", "A" ) ) + TF( Ge( " ", "ab" ) )
   c += TF( Ge( " ", "ab " ) ) + TF( Ge( " ", "abc" ) ) + TF( Ge( " ", "b" ) ) + TF( Ge( " ", " a" ) )
   c += TF( Ge( " ", ( "a" + Chr( 0 ) ) ) ) + TF( Ge( " ", Chr( 0 ) ) ) + TF( Ge( " ", Chr( 200 ) ) )
   c += TF( Ge( " ", "aB" ) )
   ? c
   c := TF( Ge( "a", "" ) ) + TF( Ge( "a", " " ) ) + TF( Ge( "a", "a" ) ) + TF( Ge( "a", "A" ) ) + TF( Ge( "a", "ab" ) )
   c += TF( Ge( "a", "ab " ) ) + TF( Ge( "a", "abc" ) ) + TF( Ge( "a", "b" ) ) + TF( Ge( "a", " a" ) )
   c += TF( Ge( "a", ( "a" + Chr( 0 ) ) ) ) + TF( Ge( "a", Chr( 0 ) ) ) + TF( Ge( "a", Chr( 200 ) ) )
   c += TF( Ge( "a", "aB" ) )
   ? c
   c := TF( Ge( "A", "" ) ) + TF( Ge( "A", " " ) ) + TF( Ge( "A", "a" ) ) + TF( Ge( "A", "A" ) ) + TF( Ge( "A", "ab" ) )
   c += TF( Ge( "A", "ab " ) ) + TF( Ge( "A", "abc" ) ) + TF( Ge( "A", "b" ) ) + TF( Ge( "A", " a" ) )
   c += TF( Ge( "A", ( "a" + Chr( 0 ) ) ) ) + TF( Ge( "A", Chr( 0 ) ) ) + TF( Ge( "A", Chr( 200 ) ) )
   c += TF( Ge( "A", "aB" ) )
   ? c
   c := TF( Ge( "ab", "" ) ) + TF( Ge( "ab", " " ) ) + TF( Ge( "ab", "a" ) ) + TF( Ge( "ab", "A" ) )
   c += TF( Ge( "ab", "ab" ) ) + TF( Ge( "ab", "ab " ) ) + TF( Ge( "ab", "abc" ) ) + TF( Ge( "ab", "b" ) )
   c += TF( Ge( "ab", " a" ) ) + TF( Ge( "ab", ( "a" + Chr( 0 ) ) ) ) + TF( Ge( "ab", Chr( 0 ) ) )
   c += TF( Ge( "ab", Chr( 200 ) ) ) + TF( Ge( "ab", "aB" ) )
   ? c
   c := TF( Ge( "ab ", "" ) ) + TF( Ge( "ab ", " " ) ) + TF( Ge( "ab ", "a" ) ) + TF( Ge( "ab ", "A" ) )
   c += TF( Ge( "ab ", "ab" ) ) + TF( Ge( "ab ", "ab " ) ) + TF( Ge( "ab ", "abc" ) ) + TF( Ge( "ab ", "b" ) )
   c += TF( Ge( "ab ", " a" ) ) + TF( Ge( "ab ", ( "a" + Chr( 0 ) ) ) ) + TF( Ge( "ab ", Chr( 0 ) ) )
   c += TF( Ge( "ab ", Chr( 200 ) ) ) + TF( Ge( "ab ", "aB" ) )
   ? c
   c := TF( Ge( "abc", "" ) ) + TF( Ge( "abc", " " ) ) + TF( Ge( "abc", "a" ) ) + TF( Ge( "abc", "A" ) )
   c += TF( Ge( "abc", "ab" ) ) + TF( Ge( "abc", "ab " ) ) + TF( Ge( "abc", "abc" ) ) + TF( Ge( "abc", "b" ) )
   c += TF( Ge( "abc", " a" ) ) + TF( Ge( "abc", ( "a" + Chr( 0 ) ) ) ) + TF( Ge( "abc", Chr( 0 ) ) )
   c += TF( Ge( "abc", Chr( 200 ) ) ) + TF( Ge( "abc", "aB" ) )
   ? c
   c := TF( Ge( "b", "" ) ) + TF( Ge( "b", " " ) ) + TF( Ge( "b", "a" ) ) + TF( Ge( "b", "A" ) ) + TF( Ge( "b", "ab" ) )
   c += TF( Ge( "b", "ab " ) ) + TF( Ge( "b", "abc" ) ) + TF( Ge( "b", "b" ) ) + TF( Ge( "b", " a" ) )
   c += TF( Ge( "b", ( "a" + Chr( 0 ) ) ) ) + TF( Ge( "b", Chr( 0 ) ) ) + TF( Ge( "b", Chr( 200 ) ) )
   c += TF( Ge( "b", "aB" ) )
   ? c
   c := TF( Ge( " a", "" ) ) + TF( Ge( " a", " " ) ) + TF( Ge( " a", "a" ) ) + TF( Ge( " a", "A" ) )
   c += TF( Ge( " a", "ab" ) ) + TF( Ge( " a", "ab " ) ) + TF( Ge( " a", "abc" ) ) + TF( Ge( " a", "b" ) )
   c += TF( Ge( " a", " a" ) ) + TF( Ge( " a", ( "a" + Chr( 0 ) ) ) ) + TF( Ge( " a", Chr( 0 ) ) )
   c += TF( Ge( " a", Chr( 200 ) ) ) + TF( Ge( " a", "aB" ) )
   ? c
   c := TF( Ge( ( "a" + Chr( 0 ) ), "" ) ) + TF( Ge( ( "a" + Chr( 0 ) ), " " ) ) + TF( Ge( ( "a" + Chr( 0 ) ), "a" ) )
   c += TF( Ge( ( "a" + Chr( 0 ) ), "A" ) ) + TF( Ge( ( "a" + Chr( 0 ) ), "ab" ) )
   c += TF( Ge( ( "a" + Chr( 0 ) ), "ab " ) ) + TF( Ge( ( "a" + Chr( 0 ) ), "abc" ) )
   c += TF( Ge( ( "a" + Chr( 0 ) ), "b" ) ) + TF( Ge( ( "a" + Chr( 0 ) ), " a" ) )
   c += TF( Ge( ( "a" + Chr( 0 ) ), ( "a" + Chr( 0 ) ) ) ) + TF( Ge( ( "a" + Chr( 0 ) ), Chr( 0 ) ) )
   c += TF( Ge( ( "a" + Chr( 0 ) ), Chr( 200 ) ) ) + TF( Ge( ( "a" + Chr( 0 ) ), "aB" ) )
   ? c
   c := TF( Ge( Chr( 0 ), "" ) ) + TF( Ge( Chr( 0 ), " " ) ) + TF( Ge( Chr( 0 ), "a" ) ) + TF( Ge( Chr( 0 ), "A" ) )
   c += TF( Ge( Chr( 0 ), "ab" ) ) + TF( Ge( Chr( 0 ), "ab " ) ) + TF( Ge( Chr( 0 ), "abc" ) )
   c += TF( Ge( Chr( 0 ), "b" ) ) + TF( Ge( Chr( 0 ), " a" ) ) + TF( Ge( Chr( 0 ), ( "a" + Chr( 0 ) ) ) )
   c += TF( Ge( Chr( 0 ), Chr( 0 ) ) ) + TF( Ge( Chr( 0 ), Chr( 200 ) ) ) + TF( Ge( Chr( 0 ), "aB" ) )
   ? c
   c := TF( Ge( Chr( 200 ), "" ) ) + TF( Ge( Chr( 200 ), " " ) ) + TF( Ge( Chr( 200 ), "a" ) )
   c += TF( Ge( Chr( 200 ), "A" ) ) + TF( Ge( Chr( 200 ), "ab" ) ) + TF( Ge( Chr( 200 ), "ab " ) )
   c += TF( Ge( Chr( 200 ), "abc" ) ) + TF( Ge( Chr( 200 ), "b" ) ) + TF( Ge( Chr( 200 ), " a" ) )
   c += TF( Ge( Chr( 200 ), ( "a" + Chr( 0 ) ) ) ) + TF( Ge( Chr( 200 ), Chr( 0 ) ) )
   c += TF( Ge( Chr( 200 ), Chr( 200 ) ) ) + TF( Ge( Chr( 200 ), "aB" ) )
   ? c
   c := TF( Ge( "aB", "" ) ) + TF( Ge( "aB", " " ) ) + TF( Ge( "aB", "a" ) ) + TF( Ge( "aB", "A" ) )
   c += TF( Ge( "aB", "ab" ) ) + TF( Ge( "aB", "ab " ) ) + TF( Ge( "aB", "abc" ) ) + TF( Ge( "aB", "b" ) )
   c += TF( Ge( "aB", " a" ) ) + TF( Ge( "aB", ( "a" + Chr( 0 ) ) ) ) + TF( Ge( "aB", Chr( 0 ) ) )
   c += TF( Ge( "aB", Chr( 200 ) ) ) + TF( Ge( "aB", "aB" ) )
   ? c
   ? "runtime >= exact"
   Table( aVal, {| a, b | a >= b } )
   ? "macro >= exact"
   Table( aText, {| a, b | &( a + " >= " + b ) } )
   ? "input >= exact"
   Table( aIn, {| a, b | a >= b } )
RETURN

STATIC PROCEDURE Table( aVal, bOp )
   LOCAL a, b
   FOR EACH a IN aVal
      ? ""
      FOR EACH b IN aVal
         ?? TF( Eval( bOp, a, b ) )
      NEXT
   NEXT
RETURN

// The text of a command-line argument with each <n> replaced by Chr( n ).
STATIC FUNCTION Decoded( cArg )
   LOCAL cOut := "", n
   DO WHILE ( n := At( "<", cArg ) ) > 0
      cOut += Left( cArg, n - 1 )
      cArg := SubStr( cArg, n + 1 )
      n := At( ">", cArg )
      IF n > 1
         cOut += Chr( Val( Left( cArg, n - 1 ) ) )
      ENDIF
      cArg := SubStr( cArg, n + 1 )
   ENDDO
RETURN cOut + cArg

STATIC FUNCTION TF( l )
RETURN iif( l, "T", "F" )

STATIC FUNCTION Eeq( a, b )
RETURN a == b

STATIC FUNCTION Has( a, b )
RETURN a $ b

STATIC FUNCTION Eq( a, b )
RETURN a = b

STATIC FUNCTION Ne( a, b )
RETURN a != b

STATIC FUNCTION Lt( a, b )
RETURN a < b

STATIC FUNCTION Le( a, b )
RETURN a <= b

STATIC FUNCTION Gt( a, b )
RETURN a > b

STATIC FUNCTION Ge( a, b )
RETURN a >= b
