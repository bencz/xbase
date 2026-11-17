// Calls of runtime functions on constants, which the compiler evaluates: on the syntax tree only the calls the
// language evaluates when compiling (Len, Chr, Asc, At, Upper of ASCII letters, digits and spaces, Empty), whose
// results are literals that fold further (an exact quotient of two integers is an integer); on xIR constants the
// others too, giving what the function returns at run time. Each call is made compiled (with literal arguments), as a
// macro of the same text, and with the same arguments as parameters of a block (nothing to fold): the three give the
// same value, type and width (a number too large for an integer counts as the largest one: a start past the end).
// The expressions of the second part show what the language folds: compiled and as a macro, under two SET DECIMALS
// (Asc() and At() give literals, with a program constant's width; Empty() of a codeblock or of literals that
// evaluate nothing else). Calls on arguments of the wrong type keep their run-time error.
#xtranslate T( <f>, <a,...> ) => { <"f">, { <a> }, {|| <f>( <a> ) }, { <"a"> } }
#xtranslate E( <x> ) => { <"x">, {|| <x> } }

PROCEDURE Main()
   LOCAL aCall, aExpr, x, n, cText, cArgs
   aCall := { ;
      T( Chr, 65 ), T( Chr, 0 ), T( Chr, 255 ), T( Chr, 256 ), T( Chr, -1 ), T( Chr, 65.9 ), T( Chr, -0.5 ), ;
      T( Chr, 10000000066.5 ), T( Asc, "A" ), T( Asc, "" ), T( Asc, "AB" ), T( Asc, Chr( 200 ) + "x" ), ;
      T( Asc, Chr( 0 ) ), T( Upper, "ab" ), T( Upper, "aZ 09" ), T( Upper, "a_b.c!" ), T( Upper, "" ), ;
      T( Upper, Chr( 200 ) + "a" ), T( Lower, "AB" ), T( Lower, "Ab_C 9" ), T( Lower, Chr( 200 ) + "B" ), ;
      T( SubStr, "abcdef", 2 ), T( SubStr, "abcdef", 2, 3 ), T( SubStr, "abcdef", -2 ), ;
      T( SubStr, "abcdef", -2, 1 ), T( SubStr, "abcdef", 0 ), T( SubStr, "abcdef", 0, 2 ), ;
      T( SubStr, "abcdef", 7 ), T( SubStr, "abcdef", 6 ), T( SubStr, "abcdef", 2, 0 ), ;
      T( SubStr, "abcdef", 2, -1 ), T( SubStr, "abcdef", 2, 100 ), T( SubStr, "abcdef", -100, 2 ), ;
      T( SubStr, "abcdef", 2.9, 1.9 ), T( SubStr, "", 1 ), T( SubStr, "abcdef", 100000000000000000000.0 ), ;
      T( SubStr, "abcdef", -6 ), T( SubStr, "abcdef", -7, 3 ), ;
      T( Left, "abc", 2 ), T( Left, "abc", 0 ), T( Left, "abc", -1 ), T( Left, "abc", 5 ), T( Left, "", 1 ), ;
      T( Left, "abc", 2.9 ), T( Right, "abc", 2 ), T( Right, "abc", 0 ), T( Right, "abc", -1 ), ;
      T( Right, "abc", 5 ), T( Right, "", 1 ), T( Right, "abc", 1.9 ), ;
      T( Trim, "  a b  " ), T( RTrim, Chr( 9 ) + " a" + Chr( 9 ) + " " ), ;
      T( LTrim, Chr( 9 ) + " a" + Chr( 9 ) + " " ), ;
      T( AllTrim, Chr( 10 ) + "x" + Chr( 13 ) + " " ), T( AllTrim, "" ), T( AllTrim, "   " ), T( LTrim, "   " ), ;
      T( Trim, "a" + Chr( 0 ) + " " ), ;
      T( Replicate, "ab", 3 ), T( Replicate, "ab", 0 ), T( Replicate, "ab", -1 ), T( Replicate, "", 5 ), ;
      T( Replicate, "ab", 2048 ), T( Replicate, "ab", 2049 ), T( Replicate, "abc", 2.9 ), ;
      T( Space, 3 ), T( Space, 0 ), T( Space, -2 ), T( Space, 4096 ), T( Space, 4097 ), ;
      T( At, "b", "abcb" ), T( At, "", "abc" ), T( At, "abcd", "abc" ), T( At, "c", "c" ), T( At, "x", "abc" ), ;
      T( At, "bc", "abcbc" ), T( At, "", "" ), T( RAt, "b", "abcb" ), T( RAt, "", "abc" ), ;
      T( RAt, "abcd", "abc" ), T( RAt, "bc", "abcbc" ), T( RAt, "a", "a" ), ;
      T( Empty, NIL ), T( Empty, .T. ), T( Empty, .F. ), T( Empty, 0 ), T( Empty, 0.0 ), T( Empty, -0.0 ), ;
      T( Empty, 5 ), T( Empty, 0.001 ), T( Empty, "" ), T( Empty, "  " ), ;
      T( Empty, Chr( 9 ) + Chr( 10 ) + Chr( 13 ) + " " ), T( Empty, Chr( 0 ) ), T( Empty, "x" ), T( Empty, {} ), ;
      T( Empty, { NIL } ), T( Empty, {=>} ), T( Empty, { 1 => 2 } ), ;
      T( Len, "abc" ), T( Len, "" ), T( Len, { 1, 2 } ), T( Len, {} ) }
   aExpr := { ;
      E( Asc( "A" ) / 5 ), E( Asc( Chr( 66 ) ) / 2 ), E( At( "c", "abcd" ) / 3 ), E( RAt( "c", "abcd" ) / 3 ), ;
      E( Len( Chr( 65 ) ) / 1 ), E( Len( Upper( "ab" ) ) / 2 ), E( Len( Upper( "a_" ) ) / 2 ), ;
      E( Len( Upper( "a1 Z" ) ) / 4 ), E( Len( Lower( "AB" ) ) / 2 ), E( Len( SubStr( "abcd", 2 ) ) / 3 ), ;
      E( Len( Left( "abc", 2 ) ) / 2 ), E( Len( Right( "abc", 2 ) ) / 2 ), E( Len( Trim( "a " ) ) / 1 ), ;
      E( Len( AllTrim( " a " ) ) / 1 ), E( Len( Replicate( "ab", 2 ) ) / 2 ), E( Len( Space( 4 ) ) / 2 ), ;
      E( Len( "a" + "b" ) / 2 ), E( Len( "a " - "b" ) / 3 ), E( Len( Chr( 65 ) + "b" ) / 2 ), ;
      E( Asc( Upper( Chr( 97 ) ) ) / 5 ), E( iif( Empty( "" ), 1, 2 ) / 1 ), E( iif( Empty( NIL ), 1, 2 ) / 1 ), ;
      E( iif( Empty( {} ), 1, 2 ) / 1 ), E( iif( Empty( {=>} ), 1, 2 ) / 1 ), E( iif( Empty( 0 ), 1, 2 ) / 1 ), ;
      E( iif( Empty( .F. ), 1, 2 ) / 1 ), E( iif( Empty( "x" ), 1, 2 ) / 1 ), E( iif( Empty( Chr( 9 ) ), 1, 2 ) / 1 ), ;
      E( iif( Chr( 65 ) == "A", 6, 0 ) / 3 ), E( iif( "AB" $ Upper( "xaby" ), 6, 0 ) / 3 ), ;
      E( At( Upper( "b" ), Upper( "abc" ) ) / 2 ), E( Len( iif( .T., "ab", "c" ) ) / 2 ), E( Len( Chr( 300 ) ) / 1 ), ;
      E( Asc( Chr( -1 ) ) / 5 ), E( Asc( "" ) / 1 ), E( At( "", "" ) / 1 ), E( Len( Upper( "é" ) ) / 2 ), ;
      E( Len( Upper( "" ) ) / 1 ), E( Asc( "AB" ) / 5 ), E( iif( Empty( { 1 } ), 1, 2 ) / 1 ), ;
      E( Str( Asc( "A" ) * 1000000000 ) ), E( Str( At( "b", "ab" ) + 1000000000 ) ), ;
      E( Str( Asc( Chr( 255 ) ) * 100000000 ) ), E( Str( Len( "ab" ) * 1000000000 ) ), ;
      E( iif( Empty( {|| 1 } ), 1, 2 ) / 1 ), E( iif( Empty( { {=>} } ), 1, 2 ) / 1 ), ;
      E( iif( Empty( { {} } ), 1, 2 ) / 1 ), E( iif( Empty( { 1 => {|| 2 } } ), 1, 2 ) / 1 ), ;
      E( Len( Upper( "ab" + Chr( 0 ) ) ) / 3 ), E( Len( Upper( "a" + "b" ) ) / 2 ) }
   ? "calls"
   FOR EACH x IN aCall
      cArgs := ""
      FOR EACH cText IN x[ 4 ]
         cArgs += iif( Empty( cArgs ), "", ", " ) + cText
      NEXT
      cText := x[ 1 ] + "( " + cArgs + " )"
      ? PadR( cText, 50 ), Show( Eval( x[ 3 ] ) ), Same( Eval( x[ 3 ] ), &( cText ), Run( x[ 1 ], x[ 2 ] ) )
   NEXT
   ? "errors kept"
   ErrorBlock( {| e | Break( e ) } )
   FOR EACH x IN { {|| Upper( 1 ) }, {|| SubStr( "abc", 1, NIL ) }, {|| Chr( "a" ) }, {|| Asc( 65 ) }, ;
         {|| At( 1, "a" ) }, {|| Replicate( "a", "b" ) }, {|| Space( "1" ) }, {|| Left( 1, 1 ) } }
      ? Failed( x )
   NEXT
   // A macro of lists nested 40 deep, every one constant: compiled at once (each level is looked at once).
   cText := "( 1, 1 )"
   FOR n := 1 TO 40
      cText := "( " + cText + ", 1 )"
   NEXT
   ? "nested", &( "Len( { " + cText + " } )" )
   FOR EACH n IN { 2, 5 }
      SET DECIMALS TO n
      ? "expressions, decimals", n
      FOR EACH x IN aExpr
         ? PadR( x[ 1 ], 50 ), Show( Eval( x[ 2 ] ) ), Show( &( x[ 1 ] ) )
      NEXT
   NEXT
RETURN

// The error a call raises: its code and the function.
STATIC FUNCTION Failed( b )
   LOCAL e
   BEGIN SEQUENCE
      Eval( b )
   RECOVER USING e
      RETURN "error " + LTrim( Str( e:SubCode ) ) + " " + e:Operation
   END SEQUENCE
RETURN "no error"

// The function called with the arguments as a block's parameters.
STATIC FUNCTION Run( cFn, aArgs )
   LOCAL cParams := SubStr( "a, b, c", 1, 3 * Len( aArgs ) - 2 )
   LOCAL bCall := &( "{| " + cParams + " | " + cFn + "( " + cParams + " ) }" )
   IF Len( aArgs ) == 1
      RETURN Eval( bCall, aArgs[ 1 ] )
   ELSEIF Len( aArgs ) == 2
      RETURN Eval( bCall, aArgs[ 1 ], aArgs[ 2 ] )
   ENDIF
RETURN Eval( bCall, aArgs[ 1 ], aArgs[ 2 ], aArgs[ 3 ] )

STATIC FUNCTION Same( xCompiled, xMacro, xRun )
   LOCAL c := Show( xCompiled )
   IF c == Show( xMacro ) .AND. c == Show( xRun )
      RETURN "same"
   ENDIF
RETURN "DIFFERENT: macro " + Show( xMacro ) + ", run " + Show( xRun )

// The value with its type; a number as Str() shows it (its width and decimals); a string's bytes outside ASCII
// letters, digits and punctuation as \code; a long string by its length and ends.
STATIC FUNCTION Show( x )
   LOCAL c, cOut := ""
   SWITCH ValType( x )
   CASE "N"
      RETURN "N[" + Str( x ) + "]"
   CASE "L"
      RETURN iif( x, ".T.", ".F." )
   CASE "C"
      IF Len( x ) > 40
         RETURN "C" + LTrim( Str( Len( x ) ) ) + "[" + Show( Left( x, 6 ) ) + ".." + Show( Right( x, 6 ) ) + "]"
      ENDIF
      FOR EACH c IN x
         cOut += iif( Asc( c ) < 32 .OR. Asc( c ) > 126, "\" + LTrim( Str( Asc( c ) ) ), c )
      NEXT
      RETURN '"' + cOut + '"'
   ENDSWITCH
RETURN ValType( x )
