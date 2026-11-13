// Val() of every text of a list (blanks around, signs, a point first or last, exponents and commas that are not
// numbers, letters after digits, hexadecimal, more digits than an integer holds, tabs), shown with the width and
// decimals it gives (Str() of it, Len( Str() ), the decimals of Str( x, 25, 9 )); then StrZero() and Str() with
// widths and decimals at their limits. Texts as literals, from an array made at run time, from macros and from
// the command line (<n> stands for Chr( n )).
PROCEDURE Main( ... )
   LOCAL aText := {}, aIn := {}, c, n, d, x
   AAdd( aText, "" )
   AAdd( aText, " " )
   AAdd( aText, "12" )
   AAdd( aText, " 12" )
   AAdd( aText, "12 " )
   AAdd( aText, "-12" )
   AAdd( aText, "+12" )
   AAdd( aText, "- 12" )
   AAdd( aText, ".5" )
   AAdd( aText, "5." )
   AAdd( aText, "-.5" )
   AAdd( aText, "-0" )
   AAdd( aText, "-0.0" )
   AAdd( aText, "0.10" )
   AAdd( aText, "1.234567890123" )
   AAdd( aText, "1e3" )
   AAdd( aText, "1E-2" )
   AAdd( aText, "1,5" )
   AAdd( aText, "1 2" )
   AAdd( aText, "abc" )
   AAdd( aText, "12abc" )
   AAdd( aText, "0x1F" )
   AAdd( aText, "1.2.3" )
   AAdd( aText, "." )
   AAdd( aText, "-" )
   AAdd( aText, "+" )
   AAdd( aText, "  -3.25  " )
   AAdd( aText, "00012.500" )
   AAdd( aText, "99999999999999999999" )
   AAdd( aText, "123456789012345678" )
   AAdd( aText, "9223372036854775807" )
   AAdd( aText, "9223372036854775808" )
   AAdd( aText, "-9223372036854775808" )
   AAdd( aText, "0.0000001" )
   AAdd( aText, "1000000000000000000000000000000" )
   AAdd( aText, "12" + Chr( 9 ) + "3" )
   AAdd( aText, "" + Chr( 9 ) + "7" )
   AAdd( aText, "1-2" )
   AAdd( aText, "--5" )
   AAdd( aText, "5-" )
   FOR EACH c IN { ... }
      AAdd( aIn, Decoded( c ) )
   NEXT
   ? "literal"
   ? Shown( Val( "" ) )
   ? Shown( Val( " " ) )
   ? Shown( Val( "12" ) )
   ? Shown( Val( " 12" ) )
   ? Shown( Val( "12 " ) )
   ? Shown( Val( "-12" ) )
   ? Shown( Val( "+12" ) )
   ? Shown( Val( "- 12" ) )
   ? Shown( Val( ".5" ) )
   ? Shown( Val( "5." ) )
   ? Shown( Val( "-.5" ) )
   ? Shown( Val( "-0" ) )
   ? Shown( Val( "-0.0" ) )
   ? Shown( Val( "0.10" ) )
   ? Shown( Val( "1.234567890123" ) )
   ? Shown( Val( "1e3" ) )
   ? Shown( Val( "1E-2" ) )
   ? Shown( Val( "1,5" ) )
   ? Shown( Val( "1 2" ) )
   ? Shown( Val( "abc" ) )
   ? Shown( Val( "12abc" ) )
   ? Shown( Val( "0x1F" ) )
   ? Shown( Val( "1.2.3" ) )
   ? Shown( Val( "." ) )
   ? Shown( Val( "-" ) )
   ? Shown( Val( "+" ) )
   ? Shown( Val( "  -3.25  " ) )
   ? Shown( Val( "00012.500" ) )
   ? Shown( Val( "99999999999999999999" ) )
   ? Shown( Val( "123456789012345678" ) )
   ? Shown( Val( "9223372036854775807" ) )
   ? Shown( Val( "9223372036854775808" ) )
   ? Shown( Val( "-9223372036854775808" ) )
   ? Shown( Val( "0.0000001" ) )
   ? Shown( Val( "1000000000000000000000000000000" ) )
   ? Shown( Val( "12" + Chr( 9 ) + "3" ) )
   ? Shown( Val( "" + Chr( 9 ) + "7" ) )
   ? Shown( Val( "1-2" ) )
   ? Shown( Val( "--5" ) )
   ? Shown( Val( "5-" ) )
   ? "runtime"
   FOR EACH c IN aText
      ? Shown( Val( c ) )
   NEXT
   ? "macro"
   FOR EACH c IN aText
      M->cText := c
      ? Shown( &( "Val( M->cText )" ) )
   NEXT
   ? "input"
   FOR EACH c IN aIn
      ? Shown( Val( c ) )
   NEXT
   ? "strzero"
   FOR EACH x IN { 0, 5, -5, 12.345, -12.345, 99999, 1234567890123, 0.5 }
      ? ""
      FOR EACH n IN { 1, 2, 5, 10, 20 }
         FOR EACH d IN { 0, 1, 3 }
            ?? " [" + StrZero( x, n, d ) + "]"
         NEXT
      NEXT
   NEXT
   ? "str"
   FOR EACH x IN { 0, -0.5, 0.5, 1.005, 2.675, -2.675, 123456789012345 * 1.0, 0.1 + 0.2 }
      ? ""
      FOR EACH n IN { 0, 1, 3, 4, 25, 60 }
         FOR EACH d IN { 0, 2, 9, 20 }
            ?? " [" + Str( x, n, d ) + "]"
         NEXT
      NEXT
   NEXT
RETURN

STATIC FUNCTION Shown( x )
   LOCAL c := Str( x ), cDec := Str( x, 25, 9 )
RETURN "[" + c + "] " + LTrim( Str( Len( c ) ) ) + " " + LTrim( cDec )

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
