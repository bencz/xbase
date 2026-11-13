// The string functions over every combination of a list of strings (empty, blank, padded, NUL, bytes above 127,
// control characters) and of positions and lengths (negative, zero, one, past the end, fractional), as a program
// meets them: literal arguments (which the compiler may fold or run in place), arguments computed at run time,
// macros compiled from text, and strings read from the command line (<n> stands for Chr( n )). Each result shows
// as its type and its text, bytes outside 32-126 as <n>.
PROCEDURE Main( ... )
   LOCAL aStr := {}, aText := {}, aIn := {}, cArg
   AAdd( aStr, "" )
   AAdd( aText, "''" )
   AAdd( aStr, " " )
   AAdd( aText, "' '" )
   AAdd( aStr, "a" )
   AAdd( aText, "'a'" )
   AAdd( aStr, "abc" )
   AAdd( aText, "'abc'" )
   AAdd( aStr, "  ab  " )
   AAdd( aText, "'  ab  '" )
   AAdd( aStr, "Hello, World" )
   AAdd( aText, "'Hello, World'" )
   AAdd( aStr, "a" + Chr( 0 ) + "b" )
   AAdd( aText, "'a' + Chr( 0 ) + 'b'" )
   AAdd( aStr, Chr( 200 ) + "x" )
   AAdd( aText, "Chr( 200 ) + 'x'" )
   AAdd( aStr, Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ) )
   AAdd( aText, "Chr( 9 ) + 't' + Chr( 13 ) + Chr( 10 )" )
   FOR EACH cArg IN { ... }
      AAdd( aIn, Decoded( cArg ) )
   NEXT
   F00( aStr, aText, aIn )
   F01( aStr, aText, aIn )
   F02( aStr, aText, aIn )
   F03( aStr, aText, aIn )
   F04( aStr, aText, aIn )
   F05( aStr, aText, aIn )
   F06( aStr, aText, aIn )
   F07( aStr, aText, aIn )
   F08( aStr, aText, aIn )
   F09( aStr, aText, aIn )
   F10( aStr, aText, aIn )
   F11( aStr, aText, aIn )
   F12( aStr, aText, aIn )
   F13( aStr, aText, aIn )
   F14( aStr, aText, aIn )
   F15( aStr, aText, aIn )
   F16( aStr, aText, aIn )
   F17( aStr, aText, aIn )
   F18( aStr, aText, aIn )
   F19( aStr, aText, aIn )
   F20( aStr, aText, aIn )
   F21( aStr, aText, aIn )
   F22( aStr, aText, aIn )
   F23( aStr, aText, aIn )
   F24( aStr, aText, aIn )
RETURN

STATIC PROCEDURE F00( aStr, aText, aIn )
   LOCAL v0, v1
   ? "SubStr(sn) literal"
   ? ""
   ?? Shown( SubStr( "", -10 ) )
   ?? Shown( SubStr( "", -3 ) )
   ?? Shown( SubStr( "", -1 ) )
   ?? Shown( SubStr( "", 0 ) )
   ?? Shown( SubStr( "", 1 ) )
   ?? Shown( SubStr( "", 2 ) )
   ?? Shown( SubStr( "", 3 ) )
   ?? Shown( SubStr( "", 5 ) )
   ?? Shown( SubStr( "", 12 ) )
   ?? Shown( SubStr( "", 13 ) )
   ?? Shown( SubStr( "", 99 ) )
   ?? Shown( SubStr( "", 2.7 ) )
   ? ""
   ?? Shown( SubStr( " ", -10 ) )
   ?? Shown( SubStr( " ", -3 ) )
   ?? Shown( SubStr( " ", -1 ) )
   ?? Shown( SubStr( " ", 0 ) )
   ?? Shown( SubStr( " ", 1 ) )
   ?? Shown( SubStr( " ", 2 ) )
   ?? Shown( SubStr( " ", 3 ) )
   ?? Shown( SubStr( " ", 5 ) )
   ?? Shown( SubStr( " ", 12 ) )
   ?? Shown( SubStr( " ", 13 ) )
   ?? Shown( SubStr( " ", 99 ) )
   ?? Shown( SubStr( " ", 2.7 ) )
   ? ""
   ?? Shown( SubStr( "a", -10 ) )
   ?? Shown( SubStr( "a", -3 ) )
   ?? Shown( SubStr( "a", -1 ) )
   ?? Shown( SubStr( "a", 0 ) )
   ?? Shown( SubStr( "a", 1 ) )
   ?? Shown( SubStr( "a", 2 ) )
   ?? Shown( SubStr( "a", 3 ) )
   ?? Shown( SubStr( "a", 5 ) )
   ?? Shown( SubStr( "a", 12 ) )
   ?? Shown( SubStr( "a", 13 ) )
   ?? Shown( SubStr( "a", 99 ) )
   ?? Shown( SubStr( "a", 2.7 ) )
   ? ""
   ?? Shown( SubStr( "abc", -10 ) )
   ?? Shown( SubStr( "abc", -3 ) )
   ?? Shown( SubStr( "abc", -1 ) )
   ?? Shown( SubStr( "abc", 0 ) )
   ?? Shown( SubStr( "abc", 1 ) )
   ?? Shown( SubStr( "abc", 2 ) )
   ?? Shown( SubStr( "abc", 3 ) )
   ?? Shown( SubStr( "abc", 5 ) )
   ?? Shown( SubStr( "abc", 12 ) )
   ?? Shown( SubStr( "abc", 13 ) )
   ?? Shown( SubStr( "abc", 99 ) )
   ?? Shown( SubStr( "abc", 2.7 ) )
   ? ""
   ?? Shown( SubStr( "  ab  ", -10 ) )
   ?? Shown( SubStr( "  ab  ", -3 ) )
   ?? Shown( SubStr( "  ab  ", -1 ) )
   ?? Shown( SubStr( "  ab  ", 0 ) )
   ?? Shown( SubStr( "  ab  ", 1 ) )
   ?? Shown( SubStr( "  ab  ", 2 ) )
   ?? Shown( SubStr( "  ab  ", 3 ) )
   ?? Shown( SubStr( "  ab  ", 5 ) )
   ?? Shown( SubStr( "  ab  ", 12 ) )
   ?? Shown( SubStr( "  ab  ", 13 ) )
   ?? Shown( SubStr( "  ab  ", 99 ) )
   ?? Shown( SubStr( "  ab  ", 2.7 ) )
   ? ""
   ?? Shown( SubStr( "Hello, World", -10 ) )
   ?? Shown( SubStr( "Hello, World", -3 ) )
   ?? Shown( SubStr( "Hello, World", -1 ) )
   ?? Shown( SubStr( "Hello, World", 0 ) )
   ?? Shown( SubStr( "Hello, World", 1 ) )
   ?? Shown( SubStr( "Hello, World", 2 ) )
   ?? Shown( SubStr( "Hello, World", 3 ) )
   ?? Shown( SubStr( "Hello, World", 5 ) )
   ?? Shown( SubStr( "Hello, World", 12 ) )
   ?? Shown( SubStr( "Hello, World", 13 ) )
   ?? Shown( SubStr( "Hello, World", 99 ) )
   ?? Shown( SubStr( "Hello, World", 2.7 ) )
   ? ""
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", -10 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", -3 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", -1 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 0 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 1 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 2 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 3 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 5 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 12 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 13 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 99 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 2.7 ) )
   ? ""
   ?? Shown( SubStr( Chr( 200 ) + "x", -10 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", -3 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", -1 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 0 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 1 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 2 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 3 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 5 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 12 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 13 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 99 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 2.7 ) )
   ? ""
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -10 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -3 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -1 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 0 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 1 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 2 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 3 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 5 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 12 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 13 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 99 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 2.7 ) )
   ? "SubStr(sn) runtime"
   FOR EACH v0 IN aStr
      ? ""
      FOR EACH v1 IN { -10, -3, -1, 0, 1, 2, 3, 5, 12, 13, 99, 2.7 }
         ?? Shown( SubStr( v0, v1 ) )
      NEXT
   NEXT
   ? "SubStr(sn) macro"
   FOR EACH v0 IN aText
      ? ""
      FOR EACH v1 IN { "-10", "-3", "-1", "0", "1", "2", "3", "5", "12", "13", "99", "2.7" }
         ?? Shown( &( "SubStr( " + v0 + ", " + v1 + " )" ) )
      NEXT
   NEXT
   ? "SubStr(sn) input"
   FOR EACH v0 IN aIn
      ? ""
      FOR EACH v1 IN { -10, -3, -1, 0, 1, 2, 3, 5, 12, 13, 99, 2.7 }
         ?? Shown( SubStr( v0, v1 ) )
      NEXT
   NEXT
RETURN

STATIC PROCEDURE F01( aStr, aText, aIn )
   LOCAL v0, v1, v2
   ? "SubStr(snm) literal"
   ? ""
   ?? Shown( SubStr( "", -10, -1 ) )
   ?? Shown( SubStr( "", -10, 0 ) )
   ?? Shown( SubStr( "", -10, 1 ) )
   ?? Shown( SubStr( "", -10, 2 ) )
   ?? Shown( SubStr( "", -10, 5 ) )
   ?? Shown( SubStr( "", -10, 99 ) )
   ?? Shown( SubStr( "", -3, -1 ) )
   ?? Shown( SubStr( "", -3, 0 ) )
   ?? Shown( SubStr( "", -3, 1 ) )
   ?? Shown( SubStr( "", -3, 2 ) )
   ?? Shown( SubStr( "", -3, 5 ) )
   ?? Shown( SubStr( "", -3, 99 ) )
   ?? Shown( SubStr( "", -1, -1 ) )
   ?? Shown( SubStr( "", -1, 0 ) )
   ?? Shown( SubStr( "", -1, 1 ) )
   ?? Shown( SubStr( "", -1, 2 ) )
   ?? Shown( SubStr( "", -1, 5 ) )
   ?? Shown( SubStr( "", -1, 99 ) )
   ?? Shown( SubStr( "", 0, -1 ) )
   ?? Shown( SubStr( "", 0, 0 ) )
   ?? Shown( SubStr( "", 0, 1 ) )
   ?? Shown( SubStr( "", 0, 2 ) )
   ?? Shown( SubStr( "", 0, 5 ) )
   ?? Shown( SubStr( "", 0, 99 ) )
   ?? Shown( SubStr( "", 1, -1 ) )
   ?? Shown( SubStr( "", 1, 0 ) )
   ?? Shown( SubStr( "", 1, 1 ) )
   ?? Shown( SubStr( "", 1, 2 ) )
   ?? Shown( SubStr( "", 1, 5 ) )
   ?? Shown( SubStr( "", 1, 99 ) )
   ?? Shown( SubStr( "", 2, -1 ) )
   ?? Shown( SubStr( "", 2, 0 ) )
   ?? Shown( SubStr( "", 2, 1 ) )
   ?? Shown( SubStr( "", 2, 2 ) )
   ?? Shown( SubStr( "", 2, 5 ) )
   ?? Shown( SubStr( "", 2, 99 ) )
   ?? Shown( SubStr( "", 3, -1 ) )
   ?? Shown( SubStr( "", 3, 0 ) )
   ?? Shown( SubStr( "", 3, 1 ) )
   ?? Shown( SubStr( "", 3, 2 ) )
   ?? Shown( SubStr( "", 3, 5 ) )
   ?? Shown( SubStr( "", 3, 99 ) )
   ?? Shown( SubStr( "", 5, -1 ) )
   ?? Shown( SubStr( "", 5, 0 ) )
   ?? Shown( SubStr( "", 5, 1 ) )
   ?? Shown( SubStr( "", 5, 2 ) )
   ?? Shown( SubStr( "", 5, 5 ) )
   ?? Shown( SubStr( "", 5, 99 ) )
   ?? Shown( SubStr( "", 12, -1 ) )
   ?? Shown( SubStr( "", 12, 0 ) )
   ?? Shown( SubStr( "", 12, 1 ) )
   ?? Shown( SubStr( "", 12, 2 ) )
   ?? Shown( SubStr( "", 12, 5 ) )
   ?? Shown( SubStr( "", 12, 99 ) )
   ?? Shown( SubStr( "", 13, -1 ) )
   ?? Shown( SubStr( "", 13, 0 ) )
   ?? Shown( SubStr( "", 13, 1 ) )
   ?? Shown( SubStr( "", 13, 2 ) )
   ?? Shown( SubStr( "", 13, 5 ) )
   ?? Shown( SubStr( "", 13, 99 ) )
   ?? Shown( SubStr( "", 99, -1 ) )
   ?? Shown( SubStr( "", 99, 0 ) )
   ?? Shown( SubStr( "", 99, 1 ) )
   ?? Shown( SubStr( "", 99, 2 ) )
   ?? Shown( SubStr( "", 99, 5 ) )
   ?? Shown( SubStr( "", 99, 99 ) )
   ?? Shown( SubStr( "", 2.7, -1 ) )
   ?? Shown( SubStr( "", 2.7, 0 ) )
   ?? Shown( SubStr( "", 2.7, 1 ) )
   ?? Shown( SubStr( "", 2.7, 2 ) )
   ?? Shown( SubStr( "", 2.7, 5 ) )
   ?? Shown( SubStr( "", 2.7, 99 ) )
   ? ""
   ?? Shown( SubStr( " ", -10, -1 ) )
   ?? Shown( SubStr( " ", -10, 0 ) )
   ?? Shown( SubStr( " ", -10, 1 ) )
   ?? Shown( SubStr( " ", -10, 2 ) )
   ?? Shown( SubStr( " ", -10, 5 ) )
   ?? Shown( SubStr( " ", -10, 99 ) )
   ?? Shown( SubStr( " ", -3, -1 ) )
   ?? Shown( SubStr( " ", -3, 0 ) )
   ?? Shown( SubStr( " ", -3, 1 ) )
   ?? Shown( SubStr( " ", -3, 2 ) )
   ?? Shown( SubStr( " ", -3, 5 ) )
   ?? Shown( SubStr( " ", -3, 99 ) )
   ?? Shown( SubStr( " ", -1, -1 ) )
   ?? Shown( SubStr( " ", -1, 0 ) )
   ?? Shown( SubStr( " ", -1, 1 ) )
   ?? Shown( SubStr( " ", -1, 2 ) )
   ?? Shown( SubStr( " ", -1, 5 ) )
   ?? Shown( SubStr( " ", -1, 99 ) )
   ?? Shown( SubStr( " ", 0, -1 ) )
   ?? Shown( SubStr( " ", 0, 0 ) )
   ?? Shown( SubStr( " ", 0, 1 ) )
   ?? Shown( SubStr( " ", 0, 2 ) )
   ?? Shown( SubStr( " ", 0, 5 ) )
   ?? Shown( SubStr( " ", 0, 99 ) )
   ?? Shown( SubStr( " ", 1, -1 ) )
   ?? Shown( SubStr( " ", 1, 0 ) )
   ?? Shown( SubStr( " ", 1, 1 ) )
   ?? Shown( SubStr( " ", 1, 2 ) )
   ?? Shown( SubStr( " ", 1, 5 ) )
   ?? Shown( SubStr( " ", 1, 99 ) )
   ?? Shown( SubStr( " ", 2, -1 ) )
   ?? Shown( SubStr( " ", 2, 0 ) )
   ?? Shown( SubStr( " ", 2, 1 ) )
   ?? Shown( SubStr( " ", 2, 2 ) )
   ?? Shown( SubStr( " ", 2, 5 ) )
   ?? Shown( SubStr( " ", 2, 99 ) )
   ?? Shown( SubStr( " ", 3, -1 ) )
   ?? Shown( SubStr( " ", 3, 0 ) )
   ?? Shown( SubStr( " ", 3, 1 ) )
   ?? Shown( SubStr( " ", 3, 2 ) )
   ?? Shown( SubStr( " ", 3, 5 ) )
   ?? Shown( SubStr( " ", 3, 99 ) )
   ?? Shown( SubStr( " ", 5, -1 ) )
   ?? Shown( SubStr( " ", 5, 0 ) )
   ?? Shown( SubStr( " ", 5, 1 ) )
   ?? Shown( SubStr( " ", 5, 2 ) )
   ?? Shown( SubStr( " ", 5, 5 ) )
   ?? Shown( SubStr( " ", 5, 99 ) )
   ?? Shown( SubStr( " ", 12, -1 ) )
   ?? Shown( SubStr( " ", 12, 0 ) )
   ?? Shown( SubStr( " ", 12, 1 ) )
   ?? Shown( SubStr( " ", 12, 2 ) )
   ?? Shown( SubStr( " ", 12, 5 ) )
   ?? Shown( SubStr( " ", 12, 99 ) )
   ?? Shown( SubStr( " ", 13, -1 ) )
   ?? Shown( SubStr( " ", 13, 0 ) )
   ?? Shown( SubStr( " ", 13, 1 ) )
   ?? Shown( SubStr( " ", 13, 2 ) )
   ?? Shown( SubStr( " ", 13, 5 ) )
   ?? Shown( SubStr( " ", 13, 99 ) )
   ?? Shown( SubStr( " ", 99, -1 ) )
   ?? Shown( SubStr( " ", 99, 0 ) )
   ?? Shown( SubStr( " ", 99, 1 ) )
   ?? Shown( SubStr( " ", 99, 2 ) )
   ?? Shown( SubStr( " ", 99, 5 ) )
   ?? Shown( SubStr( " ", 99, 99 ) )
   ?? Shown( SubStr( " ", 2.7, -1 ) )
   ?? Shown( SubStr( " ", 2.7, 0 ) )
   ?? Shown( SubStr( " ", 2.7, 1 ) )
   ?? Shown( SubStr( " ", 2.7, 2 ) )
   ?? Shown( SubStr( " ", 2.7, 5 ) )
   ?? Shown( SubStr( " ", 2.7, 99 ) )
   ? ""
   ?? Shown( SubStr( "a", -10, -1 ) )
   ?? Shown( SubStr( "a", -10, 0 ) )
   ?? Shown( SubStr( "a", -10, 1 ) )
   ?? Shown( SubStr( "a", -10, 2 ) )
   ?? Shown( SubStr( "a", -10, 5 ) )
   ?? Shown( SubStr( "a", -10, 99 ) )
   ?? Shown( SubStr( "a", -3, -1 ) )
   ?? Shown( SubStr( "a", -3, 0 ) )
   ?? Shown( SubStr( "a", -3, 1 ) )
   ?? Shown( SubStr( "a", -3, 2 ) )
   ?? Shown( SubStr( "a", -3, 5 ) )
   ?? Shown( SubStr( "a", -3, 99 ) )
   ?? Shown( SubStr( "a", -1, -1 ) )
   ?? Shown( SubStr( "a", -1, 0 ) )
   ?? Shown( SubStr( "a", -1, 1 ) )
   ?? Shown( SubStr( "a", -1, 2 ) )
   ?? Shown( SubStr( "a", -1, 5 ) )
   ?? Shown( SubStr( "a", -1, 99 ) )
   ?? Shown( SubStr( "a", 0, -1 ) )
   ?? Shown( SubStr( "a", 0, 0 ) )
   ?? Shown( SubStr( "a", 0, 1 ) )
   ?? Shown( SubStr( "a", 0, 2 ) )
   ?? Shown( SubStr( "a", 0, 5 ) )
   ?? Shown( SubStr( "a", 0, 99 ) )
   ?? Shown( SubStr( "a", 1, -1 ) )
   ?? Shown( SubStr( "a", 1, 0 ) )
   ?? Shown( SubStr( "a", 1, 1 ) )
   ?? Shown( SubStr( "a", 1, 2 ) )
   ?? Shown( SubStr( "a", 1, 5 ) )
   ?? Shown( SubStr( "a", 1, 99 ) )
   ?? Shown( SubStr( "a", 2, -1 ) )
   ?? Shown( SubStr( "a", 2, 0 ) )
   ?? Shown( SubStr( "a", 2, 1 ) )
   ?? Shown( SubStr( "a", 2, 2 ) )
   ?? Shown( SubStr( "a", 2, 5 ) )
   ?? Shown( SubStr( "a", 2, 99 ) )
   ?? Shown( SubStr( "a", 3, -1 ) )
   ?? Shown( SubStr( "a", 3, 0 ) )
   ?? Shown( SubStr( "a", 3, 1 ) )
   ?? Shown( SubStr( "a", 3, 2 ) )
   ?? Shown( SubStr( "a", 3, 5 ) )
   ?? Shown( SubStr( "a", 3, 99 ) )
   ?? Shown( SubStr( "a", 5, -1 ) )
   ?? Shown( SubStr( "a", 5, 0 ) )
   ?? Shown( SubStr( "a", 5, 1 ) )
   ?? Shown( SubStr( "a", 5, 2 ) )
   ?? Shown( SubStr( "a", 5, 5 ) )
   ?? Shown( SubStr( "a", 5, 99 ) )
   ?? Shown( SubStr( "a", 12, -1 ) )
   ?? Shown( SubStr( "a", 12, 0 ) )
   ?? Shown( SubStr( "a", 12, 1 ) )
   ?? Shown( SubStr( "a", 12, 2 ) )
   ?? Shown( SubStr( "a", 12, 5 ) )
   ?? Shown( SubStr( "a", 12, 99 ) )
   ?? Shown( SubStr( "a", 13, -1 ) )
   ?? Shown( SubStr( "a", 13, 0 ) )
   ?? Shown( SubStr( "a", 13, 1 ) )
   ?? Shown( SubStr( "a", 13, 2 ) )
   ?? Shown( SubStr( "a", 13, 5 ) )
   ?? Shown( SubStr( "a", 13, 99 ) )
   ?? Shown( SubStr( "a", 99, -1 ) )
   ?? Shown( SubStr( "a", 99, 0 ) )
   ?? Shown( SubStr( "a", 99, 1 ) )
   ?? Shown( SubStr( "a", 99, 2 ) )
   ?? Shown( SubStr( "a", 99, 5 ) )
   ?? Shown( SubStr( "a", 99, 99 ) )
   ?? Shown( SubStr( "a", 2.7, -1 ) )
   ?? Shown( SubStr( "a", 2.7, 0 ) )
   ?? Shown( SubStr( "a", 2.7, 1 ) )
   ?? Shown( SubStr( "a", 2.7, 2 ) )
   ?? Shown( SubStr( "a", 2.7, 5 ) )
   ?? Shown( SubStr( "a", 2.7, 99 ) )
   ? ""
   ?? Shown( SubStr( "abc", -10, -1 ) )
   ?? Shown( SubStr( "abc", -10, 0 ) )
   ?? Shown( SubStr( "abc", -10, 1 ) )
   ?? Shown( SubStr( "abc", -10, 2 ) )
   ?? Shown( SubStr( "abc", -10, 5 ) )
   ?? Shown( SubStr( "abc", -10, 99 ) )
   ?? Shown( SubStr( "abc", -3, -1 ) )
   ?? Shown( SubStr( "abc", -3, 0 ) )
   ?? Shown( SubStr( "abc", -3, 1 ) )
   ?? Shown( SubStr( "abc", -3, 2 ) )
   ?? Shown( SubStr( "abc", -3, 5 ) )
   ?? Shown( SubStr( "abc", -3, 99 ) )
   ?? Shown( SubStr( "abc", -1, -1 ) )
   ?? Shown( SubStr( "abc", -1, 0 ) )
   ?? Shown( SubStr( "abc", -1, 1 ) )
   ?? Shown( SubStr( "abc", -1, 2 ) )
   ?? Shown( SubStr( "abc", -1, 5 ) )
   ?? Shown( SubStr( "abc", -1, 99 ) )
   ?? Shown( SubStr( "abc", 0, -1 ) )
   ?? Shown( SubStr( "abc", 0, 0 ) )
   ?? Shown( SubStr( "abc", 0, 1 ) )
   ?? Shown( SubStr( "abc", 0, 2 ) )
   ?? Shown( SubStr( "abc", 0, 5 ) )
   ?? Shown( SubStr( "abc", 0, 99 ) )
   ?? Shown( SubStr( "abc", 1, -1 ) )
   ?? Shown( SubStr( "abc", 1, 0 ) )
   ?? Shown( SubStr( "abc", 1, 1 ) )
   ?? Shown( SubStr( "abc", 1, 2 ) )
   ?? Shown( SubStr( "abc", 1, 5 ) )
   ?? Shown( SubStr( "abc", 1, 99 ) )
   ?? Shown( SubStr( "abc", 2, -1 ) )
   ?? Shown( SubStr( "abc", 2, 0 ) )
   ?? Shown( SubStr( "abc", 2, 1 ) )
   ?? Shown( SubStr( "abc", 2, 2 ) )
   ?? Shown( SubStr( "abc", 2, 5 ) )
   ?? Shown( SubStr( "abc", 2, 99 ) )
   ?? Shown( SubStr( "abc", 3, -1 ) )
   ?? Shown( SubStr( "abc", 3, 0 ) )
   ?? Shown( SubStr( "abc", 3, 1 ) )
   ?? Shown( SubStr( "abc", 3, 2 ) )
   ?? Shown( SubStr( "abc", 3, 5 ) )
   ?? Shown( SubStr( "abc", 3, 99 ) )
   ?? Shown( SubStr( "abc", 5, -1 ) )
   ?? Shown( SubStr( "abc", 5, 0 ) )
   ?? Shown( SubStr( "abc", 5, 1 ) )
   ?? Shown( SubStr( "abc", 5, 2 ) )
   ?? Shown( SubStr( "abc", 5, 5 ) )
   ?? Shown( SubStr( "abc", 5, 99 ) )
   ?? Shown( SubStr( "abc", 12, -1 ) )
   ?? Shown( SubStr( "abc", 12, 0 ) )
   ?? Shown( SubStr( "abc", 12, 1 ) )
   ?? Shown( SubStr( "abc", 12, 2 ) )
   ?? Shown( SubStr( "abc", 12, 5 ) )
   ?? Shown( SubStr( "abc", 12, 99 ) )
   ?? Shown( SubStr( "abc", 13, -1 ) )
   ?? Shown( SubStr( "abc", 13, 0 ) )
   ?? Shown( SubStr( "abc", 13, 1 ) )
   ?? Shown( SubStr( "abc", 13, 2 ) )
   ?? Shown( SubStr( "abc", 13, 5 ) )
   ?? Shown( SubStr( "abc", 13, 99 ) )
   ?? Shown( SubStr( "abc", 99, -1 ) )
   ?? Shown( SubStr( "abc", 99, 0 ) )
   ?? Shown( SubStr( "abc", 99, 1 ) )
   ?? Shown( SubStr( "abc", 99, 2 ) )
   ?? Shown( SubStr( "abc", 99, 5 ) )
   ?? Shown( SubStr( "abc", 99, 99 ) )
   ?? Shown( SubStr( "abc", 2.7, -1 ) )
   ?? Shown( SubStr( "abc", 2.7, 0 ) )
   ?? Shown( SubStr( "abc", 2.7, 1 ) )
   ?? Shown( SubStr( "abc", 2.7, 2 ) )
   ?? Shown( SubStr( "abc", 2.7, 5 ) )
   ?? Shown( SubStr( "abc", 2.7, 99 ) )
   ? ""
   ?? Shown( SubStr( "  ab  ", -10, -1 ) )
   ?? Shown( SubStr( "  ab  ", -10, 0 ) )
   ?? Shown( SubStr( "  ab  ", -10, 1 ) )
   ?? Shown( SubStr( "  ab  ", -10, 2 ) )
   ?? Shown( SubStr( "  ab  ", -10, 5 ) )
   ?? Shown( SubStr( "  ab  ", -10, 99 ) )
   ?? Shown( SubStr( "  ab  ", -3, -1 ) )
   ?? Shown( SubStr( "  ab  ", -3, 0 ) )
   ?? Shown( SubStr( "  ab  ", -3, 1 ) )
   ?? Shown( SubStr( "  ab  ", -3, 2 ) )
   ?? Shown( SubStr( "  ab  ", -3, 5 ) )
   ?? Shown( SubStr( "  ab  ", -3, 99 ) )
   ?? Shown( SubStr( "  ab  ", -1, -1 ) )
   ?? Shown( SubStr( "  ab  ", -1, 0 ) )
   ?? Shown( SubStr( "  ab  ", -1, 1 ) )
   ?? Shown( SubStr( "  ab  ", -1, 2 ) )
   ?? Shown( SubStr( "  ab  ", -1, 5 ) )
   ?? Shown( SubStr( "  ab  ", -1, 99 ) )
   ?? Shown( SubStr( "  ab  ", 0, -1 ) )
   ?? Shown( SubStr( "  ab  ", 0, 0 ) )
   ?? Shown( SubStr( "  ab  ", 0, 1 ) )
   ?? Shown( SubStr( "  ab  ", 0, 2 ) )
   ?? Shown( SubStr( "  ab  ", 0, 5 ) )
   ?? Shown( SubStr( "  ab  ", 0, 99 ) )
   ?? Shown( SubStr( "  ab  ", 1, -1 ) )
   ?? Shown( SubStr( "  ab  ", 1, 0 ) )
   ?? Shown( SubStr( "  ab  ", 1, 1 ) )
   ?? Shown( SubStr( "  ab  ", 1, 2 ) )
   ?? Shown( SubStr( "  ab  ", 1, 5 ) )
   ?? Shown( SubStr( "  ab  ", 1, 99 ) )
   ?? Shown( SubStr( "  ab  ", 2, -1 ) )
   ?? Shown( SubStr( "  ab  ", 2, 0 ) )
   ?? Shown( SubStr( "  ab  ", 2, 1 ) )
   ?? Shown( SubStr( "  ab  ", 2, 2 ) )
   ?? Shown( SubStr( "  ab  ", 2, 5 ) )
   ?? Shown( SubStr( "  ab  ", 2, 99 ) )
   ?? Shown( SubStr( "  ab  ", 3, -1 ) )
   ?? Shown( SubStr( "  ab  ", 3, 0 ) )
   ?? Shown( SubStr( "  ab  ", 3, 1 ) )
   ?? Shown( SubStr( "  ab  ", 3, 2 ) )
   ?? Shown( SubStr( "  ab  ", 3, 5 ) )
   ?? Shown( SubStr( "  ab  ", 3, 99 ) )
   ?? Shown( SubStr( "  ab  ", 5, -1 ) )
   ?? Shown( SubStr( "  ab  ", 5, 0 ) )
   ?? Shown( SubStr( "  ab  ", 5, 1 ) )
   ?? Shown( SubStr( "  ab  ", 5, 2 ) )
   ?? Shown( SubStr( "  ab  ", 5, 5 ) )
   ?? Shown( SubStr( "  ab  ", 5, 99 ) )
   ?? Shown( SubStr( "  ab  ", 12, -1 ) )
   ?? Shown( SubStr( "  ab  ", 12, 0 ) )
   ?? Shown( SubStr( "  ab  ", 12, 1 ) )
   ?? Shown( SubStr( "  ab  ", 12, 2 ) )
   ?? Shown( SubStr( "  ab  ", 12, 5 ) )
   ?? Shown( SubStr( "  ab  ", 12, 99 ) )
   ?? Shown( SubStr( "  ab  ", 13, -1 ) )
   ?? Shown( SubStr( "  ab  ", 13, 0 ) )
   ?? Shown( SubStr( "  ab  ", 13, 1 ) )
   ?? Shown( SubStr( "  ab  ", 13, 2 ) )
   ?? Shown( SubStr( "  ab  ", 13, 5 ) )
   ?? Shown( SubStr( "  ab  ", 13, 99 ) )
   ?? Shown( SubStr( "  ab  ", 99, -1 ) )
   ?? Shown( SubStr( "  ab  ", 99, 0 ) )
   ?? Shown( SubStr( "  ab  ", 99, 1 ) )
   ?? Shown( SubStr( "  ab  ", 99, 2 ) )
   ?? Shown( SubStr( "  ab  ", 99, 5 ) )
   ?? Shown( SubStr( "  ab  ", 99, 99 ) )
   ?? Shown( SubStr( "  ab  ", 2.7, -1 ) )
   ?? Shown( SubStr( "  ab  ", 2.7, 0 ) )
   ?? Shown( SubStr( "  ab  ", 2.7, 1 ) )
   ?? Shown( SubStr( "  ab  ", 2.7, 2 ) )
   ?? Shown( SubStr( "  ab  ", 2.7, 5 ) )
   ?? Shown( SubStr( "  ab  ", 2.7, 99 ) )
   ? ""
   ?? Shown( SubStr( "Hello, World", -10, -1 ) )
   ?? Shown( SubStr( "Hello, World", -10, 0 ) )
   ?? Shown( SubStr( "Hello, World", -10, 1 ) )
   ?? Shown( SubStr( "Hello, World", -10, 2 ) )
   ?? Shown( SubStr( "Hello, World", -10, 5 ) )
   ?? Shown( SubStr( "Hello, World", -10, 99 ) )
   ?? Shown( SubStr( "Hello, World", -3, -1 ) )
   ?? Shown( SubStr( "Hello, World", -3, 0 ) )
   ?? Shown( SubStr( "Hello, World", -3, 1 ) )
   ?? Shown( SubStr( "Hello, World", -3, 2 ) )
   ?? Shown( SubStr( "Hello, World", -3, 5 ) )
   ?? Shown( SubStr( "Hello, World", -3, 99 ) )
   ?? Shown( SubStr( "Hello, World", -1, -1 ) )
   ?? Shown( SubStr( "Hello, World", -1, 0 ) )
   ?? Shown( SubStr( "Hello, World", -1, 1 ) )
   ?? Shown( SubStr( "Hello, World", -1, 2 ) )
   ?? Shown( SubStr( "Hello, World", -1, 5 ) )
   ?? Shown( SubStr( "Hello, World", -1, 99 ) )
   ?? Shown( SubStr( "Hello, World", 0, -1 ) )
   ?? Shown( SubStr( "Hello, World", 0, 0 ) )
   ?? Shown( SubStr( "Hello, World", 0, 1 ) )
   ?? Shown( SubStr( "Hello, World", 0, 2 ) )
   ?? Shown( SubStr( "Hello, World", 0, 5 ) )
   ?? Shown( SubStr( "Hello, World", 0, 99 ) )
   ?? Shown( SubStr( "Hello, World", 1, -1 ) )
   ?? Shown( SubStr( "Hello, World", 1, 0 ) )
   ?? Shown( SubStr( "Hello, World", 1, 1 ) )
   ?? Shown( SubStr( "Hello, World", 1, 2 ) )
   ?? Shown( SubStr( "Hello, World", 1, 5 ) )
   ?? Shown( SubStr( "Hello, World", 1, 99 ) )
   ?? Shown( SubStr( "Hello, World", 2, -1 ) )
   ?? Shown( SubStr( "Hello, World", 2, 0 ) )
   ?? Shown( SubStr( "Hello, World", 2, 1 ) )
   ?? Shown( SubStr( "Hello, World", 2, 2 ) )
   ?? Shown( SubStr( "Hello, World", 2, 5 ) )
   ?? Shown( SubStr( "Hello, World", 2, 99 ) )
   ?? Shown( SubStr( "Hello, World", 3, -1 ) )
   ?? Shown( SubStr( "Hello, World", 3, 0 ) )
   ?? Shown( SubStr( "Hello, World", 3, 1 ) )
   ?? Shown( SubStr( "Hello, World", 3, 2 ) )
   ?? Shown( SubStr( "Hello, World", 3, 5 ) )
   ?? Shown( SubStr( "Hello, World", 3, 99 ) )
   ?? Shown( SubStr( "Hello, World", 5, -1 ) )
   ?? Shown( SubStr( "Hello, World", 5, 0 ) )
   ?? Shown( SubStr( "Hello, World", 5, 1 ) )
   ?? Shown( SubStr( "Hello, World", 5, 2 ) )
   ?? Shown( SubStr( "Hello, World", 5, 5 ) )
   ?? Shown( SubStr( "Hello, World", 5, 99 ) )
   ?? Shown( SubStr( "Hello, World", 12, -1 ) )
   ?? Shown( SubStr( "Hello, World", 12, 0 ) )
   ?? Shown( SubStr( "Hello, World", 12, 1 ) )
   ?? Shown( SubStr( "Hello, World", 12, 2 ) )
   ?? Shown( SubStr( "Hello, World", 12, 5 ) )
   ?? Shown( SubStr( "Hello, World", 12, 99 ) )
   ?? Shown( SubStr( "Hello, World", 13, -1 ) )
   ?? Shown( SubStr( "Hello, World", 13, 0 ) )
   ?? Shown( SubStr( "Hello, World", 13, 1 ) )
   ?? Shown( SubStr( "Hello, World", 13, 2 ) )
   ?? Shown( SubStr( "Hello, World", 13, 5 ) )
   ?? Shown( SubStr( "Hello, World", 13, 99 ) )
   ?? Shown( SubStr( "Hello, World", 99, -1 ) )
   ?? Shown( SubStr( "Hello, World", 99, 0 ) )
   ?? Shown( SubStr( "Hello, World", 99, 1 ) )
   ?? Shown( SubStr( "Hello, World", 99, 2 ) )
   ?? Shown( SubStr( "Hello, World", 99, 5 ) )
   ?? Shown( SubStr( "Hello, World", 99, 99 ) )
   ?? Shown( SubStr( "Hello, World", 2.7, -1 ) )
   ?? Shown( SubStr( "Hello, World", 2.7, 0 ) )
   ?? Shown( SubStr( "Hello, World", 2.7, 1 ) )
   ?? Shown( SubStr( "Hello, World", 2.7, 2 ) )
   ?? Shown( SubStr( "Hello, World", 2.7, 5 ) )
   ?? Shown( SubStr( "Hello, World", 2.7, 99 ) )
   ? ""
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", -10, -1 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", -10, 0 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", -10, 1 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", -10, 2 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", -10, 5 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", -10, 99 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", -3, -1 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", -3, 0 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", -3, 1 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", -3, 2 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", -3, 5 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", -3, 99 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", -1, -1 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", -1, 0 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", -1, 1 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", -1, 2 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", -1, 5 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", -1, 99 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 0, -1 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 0, 0 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 0, 1 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 0, 2 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 0, 5 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 0, 99 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 1, -1 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 1, 0 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 1, 1 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 1, 2 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 1, 5 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 1, 99 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 2, -1 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 2, 0 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 2, 1 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 2, 2 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 2, 5 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 2, 99 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 3, -1 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 3, 0 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 3, 1 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 3, 2 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 3, 5 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 3, 99 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 5, -1 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 5, 0 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 5, 1 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 5, 2 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 5, 5 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 5, 99 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 12, -1 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 12, 0 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 12, 1 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 12, 2 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 12, 5 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 12, 99 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 13, -1 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 13, 0 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 13, 1 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 13, 2 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 13, 5 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 13, 99 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 99, -1 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 99, 0 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 99, 1 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 99, 2 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 99, 5 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 99, 99 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 2.7, -1 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 2.7, 0 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 2.7, 1 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 2.7, 2 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 2.7, 5 ) )
   ?? Shown( SubStr( "a" + Chr( 0 ) + "b", 2.7, 99 ) )
   ? ""
   ?? Shown( SubStr( Chr( 200 ) + "x", -10, -1 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", -10, 0 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", -10, 1 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", -10, 2 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", -10, 5 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", -10, 99 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", -3, -1 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", -3, 0 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", -3, 1 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", -3, 2 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", -3, 5 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", -3, 99 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", -1, -1 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", -1, 0 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", -1, 1 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", -1, 2 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", -1, 5 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", -1, 99 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 0, -1 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 0, 0 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 0, 1 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 0, 2 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 0, 5 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 0, 99 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 1, -1 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 1, 0 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 1, 1 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 1, 2 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 1, 5 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 1, 99 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 2, -1 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 2, 0 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 2, 1 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 2, 2 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 2, 5 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 2, 99 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 3, -1 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 3, 0 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 3, 1 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 3, 2 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 3, 5 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 3, 99 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 5, -1 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 5, 0 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 5, 1 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 5, 2 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 5, 5 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 5, 99 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 12, -1 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 12, 0 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 12, 1 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 12, 2 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 12, 5 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 12, 99 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 13, -1 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 13, 0 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 13, 1 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 13, 2 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 13, 5 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 13, 99 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 99, -1 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 99, 0 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 99, 1 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 99, 2 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 99, 5 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 99, 99 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 2.7, -1 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 2.7, 0 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 2.7, 1 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 2.7, 2 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 2.7, 5 ) )
   ?? Shown( SubStr( Chr( 200 ) + "x", 2.7, 99 ) )
   ? ""
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -10, -1 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -10, 0 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -10, 1 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -10, 2 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -10, 5 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -10, 99 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -3, -1 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -3, 0 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -3, 1 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -3, 2 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -3, 5 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -3, 99 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -1, -1 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -1, 0 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -1, 1 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -1, 2 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -1, 5 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -1, 99 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 0, -1 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 0, 0 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 0, 1 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 0, 2 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 0, 5 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 0, 99 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 1, -1 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 1, 0 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 1, 1 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 1, 2 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 1, 5 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 1, 99 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 2, -1 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 2, 0 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 2, 1 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 2, 2 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 2, 5 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 2, 99 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 3, -1 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 3, 0 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 3, 1 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 3, 2 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 3, 5 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 3, 99 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 5, -1 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 5, 0 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 5, 1 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 5, 2 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 5, 5 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 5, 99 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 12, -1 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 12, 0 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 12, 1 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 12, 2 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 12, 5 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 12, 99 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 13, -1 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 13, 0 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 13, 1 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 13, 2 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 13, 5 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 13, 99 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 99, -1 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 99, 0 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 99, 1 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 99, 2 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 99, 5 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 99, 99 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 2.7, -1 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 2.7, 0 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 2.7, 1 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 2.7, 2 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 2.7, 5 ) )
   ?? Shown( SubStr( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 2.7, 99 ) )
   ? "SubStr(snm) runtime"
   FOR EACH v0 IN aStr
      ? ""
      FOR EACH v1 IN { -10, -3, -1, 0, 1, 2, 3, 5, 12, 13, 99, 2.7 }
         FOR EACH v2 IN { -1, 0, 1, 2, 5, 99 }
            ?? Shown( SubStr( v0, v1, v2 ) )
         NEXT
      NEXT
   NEXT
   ? "SubStr(snm) macro"
   FOR EACH v0 IN aText
      ? ""
      FOR EACH v1 IN { "-10", "-3", "-1", "0", "1", "2", "3", "5", "12", "13", "99", "2.7" }
         FOR EACH v2 IN { "-1", "0", "1", "2", "5", "99" }
            ?? Shown( &( "SubStr( " + v0 + ", " + v1 + ", " + v2 + " )" ) )
         NEXT
      NEXT
   NEXT
   ? "SubStr(snm) input"
   FOR EACH v0 IN aIn
      ? ""
      FOR EACH v1 IN { -10, -3, -1, 0, 1, 2, 3, 5, 12, 13, 99, 2.7 }
         FOR EACH v2 IN { -1, 0, 1, 2, 5, 99 }
            ?? Shown( SubStr( v0, v1, v2 ) )
         NEXT
      NEXT
   NEXT
RETURN

STATIC PROCEDURE F02( aStr, aText, aIn )
   LOCAL v0, v1
   ? "Left(sn) literal"
   ? ""
   ?? Shown( Left( "", -10 ) )
   ?? Shown( Left( "", -3 ) )
   ?? Shown( Left( "", -1 ) )
   ?? Shown( Left( "", 0 ) )
   ?? Shown( Left( "", 1 ) )
   ?? Shown( Left( "", 2 ) )
   ?? Shown( Left( "", 3 ) )
   ?? Shown( Left( "", 5 ) )
   ?? Shown( Left( "", 12 ) )
   ?? Shown( Left( "", 13 ) )
   ?? Shown( Left( "", 99 ) )
   ?? Shown( Left( "", 2.7 ) )
   ? ""
   ?? Shown( Left( " ", -10 ) )
   ?? Shown( Left( " ", -3 ) )
   ?? Shown( Left( " ", -1 ) )
   ?? Shown( Left( " ", 0 ) )
   ?? Shown( Left( " ", 1 ) )
   ?? Shown( Left( " ", 2 ) )
   ?? Shown( Left( " ", 3 ) )
   ?? Shown( Left( " ", 5 ) )
   ?? Shown( Left( " ", 12 ) )
   ?? Shown( Left( " ", 13 ) )
   ?? Shown( Left( " ", 99 ) )
   ?? Shown( Left( " ", 2.7 ) )
   ? ""
   ?? Shown( Left( "a", -10 ) )
   ?? Shown( Left( "a", -3 ) )
   ?? Shown( Left( "a", -1 ) )
   ?? Shown( Left( "a", 0 ) )
   ?? Shown( Left( "a", 1 ) )
   ?? Shown( Left( "a", 2 ) )
   ?? Shown( Left( "a", 3 ) )
   ?? Shown( Left( "a", 5 ) )
   ?? Shown( Left( "a", 12 ) )
   ?? Shown( Left( "a", 13 ) )
   ?? Shown( Left( "a", 99 ) )
   ?? Shown( Left( "a", 2.7 ) )
   ? ""
   ?? Shown( Left( "abc", -10 ) )
   ?? Shown( Left( "abc", -3 ) )
   ?? Shown( Left( "abc", -1 ) )
   ?? Shown( Left( "abc", 0 ) )
   ?? Shown( Left( "abc", 1 ) )
   ?? Shown( Left( "abc", 2 ) )
   ?? Shown( Left( "abc", 3 ) )
   ?? Shown( Left( "abc", 5 ) )
   ?? Shown( Left( "abc", 12 ) )
   ?? Shown( Left( "abc", 13 ) )
   ?? Shown( Left( "abc", 99 ) )
   ?? Shown( Left( "abc", 2.7 ) )
   ? ""
   ?? Shown( Left( "  ab  ", -10 ) )
   ?? Shown( Left( "  ab  ", -3 ) )
   ?? Shown( Left( "  ab  ", -1 ) )
   ?? Shown( Left( "  ab  ", 0 ) )
   ?? Shown( Left( "  ab  ", 1 ) )
   ?? Shown( Left( "  ab  ", 2 ) )
   ?? Shown( Left( "  ab  ", 3 ) )
   ?? Shown( Left( "  ab  ", 5 ) )
   ?? Shown( Left( "  ab  ", 12 ) )
   ?? Shown( Left( "  ab  ", 13 ) )
   ?? Shown( Left( "  ab  ", 99 ) )
   ?? Shown( Left( "  ab  ", 2.7 ) )
   ? ""
   ?? Shown( Left( "Hello, World", -10 ) )
   ?? Shown( Left( "Hello, World", -3 ) )
   ?? Shown( Left( "Hello, World", -1 ) )
   ?? Shown( Left( "Hello, World", 0 ) )
   ?? Shown( Left( "Hello, World", 1 ) )
   ?? Shown( Left( "Hello, World", 2 ) )
   ?? Shown( Left( "Hello, World", 3 ) )
   ?? Shown( Left( "Hello, World", 5 ) )
   ?? Shown( Left( "Hello, World", 12 ) )
   ?? Shown( Left( "Hello, World", 13 ) )
   ?? Shown( Left( "Hello, World", 99 ) )
   ?? Shown( Left( "Hello, World", 2.7 ) )
   ? ""
   ?? Shown( Left( "a" + Chr( 0 ) + "b", -10 ) )
   ?? Shown( Left( "a" + Chr( 0 ) + "b", -3 ) )
   ?? Shown( Left( "a" + Chr( 0 ) + "b", -1 ) )
   ?? Shown( Left( "a" + Chr( 0 ) + "b", 0 ) )
   ?? Shown( Left( "a" + Chr( 0 ) + "b", 1 ) )
   ?? Shown( Left( "a" + Chr( 0 ) + "b", 2 ) )
   ?? Shown( Left( "a" + Chr( 0 ) + "b", 3 ) )
   ?? Shown( Left( "a" + Chr( 0 ) + "b", 5 ) )
   ?? Shown( Left( "a" + Chr( 0 ) + "b", 12 ) )
   ?? Shown( Left( "a" + Chr( 0 ) + "b", 13 ) )
   ?? Shown( Left( "a" + Chr( 0 ) + "b", 99 ) )
   ?? Shown( Left( "a" + Chr( 0 ) + "b", 2.7 ) )
   ? ""
   ?? Shown( Left( Chr( 200 ) + "x", -10 ) )
   ?? Shown( Left( Chr( 200 ) + "x", -3 ) )
   ?? Shown( Left( Chr( 200 ) + "x", -1 ) )
   ?? Shown( Left( Chr( 200 ) + "x", 0 ) )
   ?? Shown( Left( Chr( 200 ) + "x", 1 ) )
   ?? Shown( Left( Chr( 200 ) + "x", 2 ) )
   ?? Shown( Left( Chr( 200 ) + "x", 3 ) )
   ?? Shown( Left( Chr( 200 ) + "x", 5 ) )
   ?? Shown( Left( Chr( 200 ) + "x", 12 ) )
   ?? Shown( Left( Chr( 200 ) + "x", 13 ) )
   ?? Shown( Left( Chr( 200 ) + "x", 99 ) )
   ?? Shown( Left( Chr( 200 ) + "x", 2.7 ) )
   ? ""
   ?? Shown( Left( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -10 ) )
   ?? Shown( Left( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -3 ) )
   ?? Shown( Left( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -1 ) )
   ?? Shown( Left( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 0 ) )
   ?? Shown( Left( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 1 ) )
   ?? Shown( Left( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 2 ) )
   ?? Shown( Left( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 3 ) )
   ?? Shown( Left( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 5 ) )
   ?? Shown( Left( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 12 ) )
   ?? Shown( Left( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 13 ) )
   ?? Shown( Left( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 99 ) )
   ?? Shown( Left( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 2.7 ) )
   ? "Left(sn) runtime"
   FOR EACH v0 IN aStr
      ? ""
      FOR EACH v1 IN { -10, -3, -1, 0, 1, 2, 3, 5, 12, 13, 99, 2.7 }
         ?? Shown( Left( v0, v1 ) )
      NEXT
   NEXT
   ? "Left(sn) macro"
   FOR EACH v0 IN aText
      ? ""
      FOR EACH v1 IN { "-10", "-3", "-1", "0", "1", "2", "3", "5", "12", "13", "99", "2.7" }
         ?? Shown( &( "Left( " + v0 + ", " + v1 + " )" ) )
      NEXT
   NEXT
   ? "Left(sn) input"
   FOR EACH v0 IN aIn
      ? ""
      FOR EACH v1 IN { -10, -3, -1, 0, 1, 2, 3, 5, 12, 13, 99, 2.7 }
         ?? Shown( Left( v0, v1 ) )
      NEXT
   NEXT
RETURN

STATIC PROCEDURE F03( aStr, aText, aIn )
   LOCAL v0, v1
   ? "Right(sn) literal"
   ? ""
   ?? Shown( Right( "", -10 ) )
   ?? Shown( Right( "", -3 ) )
   ?? Shown( Right( "", -1 ) )
   ?? Shown( Right( "", 0 ) )
   ?? Shown( Right( "", 1 ) )
   ?? Shown( Right( "", 2 ) )
   ?? Shown( Right( "", 3 ) )
   ?? Shown( Right( "", 5 ) )
   ?? Shown( Right( "", 12 ) )
   ?? Shown( Right( "", 13 ) )
   ?? Shown( Right( "", 99 ) )
   ?? Shown( Right( "", 2.7 ) )
   ? ""
   ?? Shown( Right( " ", -10 ) )
   ?? Shown( Right( " ", -3 ) )
   ?? Shown( Right( " ", -1 ) )
   ?? Shown( Right( " ", 0 ) )
   ?? Shown( Right( " ", 1 ) )
   ?? Shown( Right( " ", 2 ) )
   ?? Shown( Right( " ", 3 ) )
   ?? Shown( Right( " ", 5 ) )
   ?? Shown( Right( " ", 12 ) )
   ?? Shown( Right( " ", 13 ) )
   ?? Shown( Right( " ", 99 ) )
   ?? Shown( Right( " ", 2.7 ) )
   ? ""
   ?? Shown( Right( "a", -10 ) )
   ?? Shown( Right( "a", -3 ) )
   ?? Shown( Right( "a", -1 ) )
   ?? Shown( Right( "a", 0 ) )
   ?? Shown( Right( "a", 1 ) )
   ?? Shown( Right( "a", 2 ) )
   ?? Shown( Right( "a", 3 ) )
   ?? Shown( Right( "a", 5 ) )
   ?? Shown( Right( "a", 12 ) )
   ?? Shown( Right( "a", 13 ) )
   ?? Shown( Right( "a", 99 ) )
   ?? Shown( Right( "a", 2.7 ) )
   ? ""
   ?? Shown( Right( "abc", -10 ) )
   ?? Shown( Right( "abc", -3 ) )
   ?? Shown( Right( "abc", -1 ) )
   ?? Shown( Right( "abc", 0 ) )
   ?? Shown( Right( "abc", 1 ) )
   ?? Shown( Right( "abc", 2 ) )
   ?? Shown( Right( "abc", 3 ) )
   ?? Shown( Right( "abc", 5 ) )
   ?? Shown( Right( "abc", 12 ) )
   ?? Shown( Right( "abc", 13 ) )
   ?? Shown( Right( "abc", 99 ) )
   ?? Shown( Right( "abc", 2.7 ) )
   ? ""
   ?? Shown( Right( "  ab  ", -10 ) )
   ?? Shown( Right( "  ab  ", -3 ) )
   ?? Shown( Right( "  ab  ", -1 ) )
   ?? Shown( Right( "  ab  ", 0 ) )
   ?? Shown( Right( "  ab  ", 1 ) )
   ?? Shown( Right( "  ab  ", 2 ) )
   ?? Shown( Right( "  ab  ", 3 ) )
   ?? Shown( Right( "  ab  ", 5 ) )
   ?? Shown( Right( "  ab  ", 12 ) )
   ?? Shown( Right( "  ab  ", 13 ) )
   ?? Shown( Right( "  ab  ", 99 ) )
   ?? Shown( Right( "  ab  ", 2.7 ) )
   ? ""
   ?? Shown( Right( "Hello, World", -10 ) )
   ?? Shown( Right( "Hello, World", -3 ) )
   ?? Shown( Right( "Hello, World", -1 ) )
   ?? Shown( Right( "Hello, World", 0 ) )
   ?? Shown( Right( "Hello, World", 1 ) )
   ?? Shown( Right( "Hello, World", 2 ) )
   ?? Shown( Right( "Hello, World", 3 ) )
   ?? Shown( Right( "Hello, World", 5 ) )
   ?? Shown( Right( "Hello, World", 12 ) )
   ?? Shown( Right( "Hello, World", 13 ) )
   ?? Shown( Right( "Hello, World", 99 ) )
   ?? Shown( Right( "Hello, World", 2.7 ) )
   ? ""
   ?? Shown( Right( "a" + Chr( 0 ) + "b", -10 ) )
   ?? Shown( Right( "a" + Chr( 0 ) + "b", -3 ) )
   ?? Shown( Right( "a" + Chr( 0 ) + "b", -1 ) )
   ?? Shown( Right( "a" + Chr( 0 ) + "b", 0 ) )
   ?? Shown( Right( "a" + Chr( 0 ) + "b", 1 ) )
   ?? Shown( Right( "a" + Chr( 0 ) + "b", 2 ) )
   ?? Shown( Right( "a" + Chr( 0 ) + "b", 3 ) )
   ?? Shown( Right( "a" + Chr( 0 ) + "b", 5 ) )
   ?? Shown( Right( "a" + Chr( 0 ) + "b", 12 ) )
   ?? Shown( Right( "a" + Chr( 0 ) + "b", 13 ) )
   ?? Shown( Right( "a" + Chr( 0 ) + "b", 99 ) )
   ?? Shown( Right( "a" + Chr( 0 ) + "b", 2.7 ) )
   ? ""
   ?? Shown( Right( Chr( 200 ) + "x", -10 ) )
   ?? Shown( Right( Chr( 200 ) + "x", -3 ) )
   ?? Shown( Right( Chr( 200 ) + "x", -1 ) )
   ?? Shown( Right( Chr( 200 ) + "x", 0 ) )
   ?? Shown( Right( Chr( 200 ) + "x", 1 ) )
   ?? Shown( Right( Chr( 200 ) + "x", 2 ) )
   ?? Shown( Right( Chr( 200 ) + "x", 3 ) )
   ?? Shown( Right( Chr( 200 ) + "x", 5 ) )
   ?? Shown( Right( Chr( 200 ) + "x", 12 ) )
   ?? Shown( Right( Chr( 200 ) + "x", 13 ) )
   ?? Shown( Right( Chr( 200 ) + "x", 99 ) )
   ?? Shown( Right( Chr( 200 ) + "x", 2.7 ) )
   ? ""
   ?? Shown( Right( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -10 ) )
   ?? Shown( Right( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -3 ) )
   ?? Shown( Right( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -1 ) )
   ?? Shown( Right( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 0 ) )
   ?? Shown( Right( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 1 ) )
   ?? Shown( Right( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 2 ) )
   ?? Shown( Right( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 3 ) )
   ?? Shown( Right( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 5 ) )
   ?? Shown( Right( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 12 ) )
   ?? Shown( Right( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 13 ) )
   ?? Shown( Right( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 99 ) )
   ?? Shown( Right( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 2.7 ) )
   ? "Right(sn) runtime"
   FOR EACH v0 IN aStr
      ? ""
      FOR EACH v1 IN { -10, -3, -1, 0, 1, 2, 3, 5, 12, 13, 99, 2.7 }
         ?? Shown( Right( v0, v1 ) )
      NEXT
   NEXT
   ? "Right(sn) macro"
   FOR EACH v0 IN aText
      ? ""
      FOR EACH v1 IN { "-10", "-3", "-1", "0", "1", "2", "3", "5", "12", "13", "99", "2.7" }
         ?? Shown( &( "Right( " + v0 + ", " + v1 + " )" ) )
      NEXT
   NEXT
   ? "Right(sn) input"
   FOR EACH v0 IN aIn
      ? ""
      FOR EACH v1 IN { -10, -3, -1, 0, 1, 2, 3, 5, 12, 13, 99, 2.7 }
         ?? Shown( Right( v0, v1 ) )
      NEXT
   NEXT
RETURN

STATIC PROCEDURE F04( aStr, aText, aIn )
   LOCAL v0, v1
   ? "PadL(sn) literal"
   ? ""
   ?? Shown( PadL( "", -10 ) )
   ?? Shown( PadL( "", -3 ) )
   ?? Shown( PadL( "", -1 ) )
   ?? Shown( PadL( "", 0 ) )
   ?? Shown( PadL( "", 1 ) )
   ?? Shown( PadL( "", 2 ) )
   ?? Shown( PadL( "", 3 ) )
   ?? Shown( PadL( "", 5 ) )
   ?? Shown( PadL( "", 12 ) )
   ?? Shown( PadL( "", 13 ) )
   ?? Shown( PadL( "", 99 ) )
   ?? Shown( PadL( "", 2.7 ) )
   ? ""
   ?? Shown( PadL( " ", -10 ) )
   ?? Shown( PadL( " ", -3 ) )
   ?? Shown( PadL( " ", -1 ) )
   ?? Shown( PadL( " ", 0 ) )
   ?? Shown( PadL( " ", 1 ) )
   ?? Shown( PadL( " ", 2 ) )
   ?? Shown( PadL( " ", 3 ) )
   ?? Shown( PadL( " ", 5 ) )
   ?? Shown( PadL( " ", 12 ) )
   ?? Shown( PadL( " ", 13 ) )
   ?? Shown( PadL( " ", 99 ) )
   ?? Shown( PadL( " ", 2.7 ) )
   ? ""
   ?? Shown( PadL( "a", -10 ) )
   ?? Shown( PadL( "a", -3 ) )
   ?? Shown( PadL( "a", -1 ) )
   ?? Shown( PadL( "a", 0 ) )
   ?? Shown( PadL( "a", 1 ) )
   ?? Shown( PadL( "a", 2 ) )
   ?? Shown( PadL( "a", 3 ) )
   ?? Shown( PadL( "a", 5 ) )
   ?? Shown( PadL( "a", 12 ) )
   ?? Shown( PadL( "a", 13 ) )
   ?? Shown( PadL( "a", 99 ) )
   ?? Shown( PadL( "a", 2.7 ) )
   ? ""
   ?? Shown( PadL( "abc", -10 ) )
   ?? Shown( PadL( "abc", -3 ) )
   ?? Shown( PadL( "abc", -1 ) )
   ?? Shown( PadL( "abc", 0 ) )
   ?? Shown( PadL( "abc", 1 ) )
   ?? Shown( PadL( "abc", 2 ) )
   ?? Shown( PadL( "abc", 3 ) )
   ?? Shown( PadL( "abc", 5 ) )
   ?? Shown( PadL( "abc", 12 ) )
   ?? Shown( PadL( "abc", 13 ) )
   ?? Shown( PadL( "abc", 99 ) )
   ?? Shown( PadL( "abc", 2.7 ) )
   ? ""
   ?? Shown( PadL( "  ab  ", -10 ) )
   ?? Shown( PadL( "  ab  ", -3 ) )
   ?? Shown( PadL( "  ab  ", -1 ) )
   ?? Shown( PadL( "  ab  ", 0 ) )
   ?? Shown( PadL( "  ab  ", 1 ) )
   ?? Shown( PadL( "  ab  ", 2 ) )
   ?? Shown( PadL( "  ab  ", 3 ) )
   ?? Shown( PadL( "  ab  ", 5 ) )
   ?? Shown( PadL( "  ab  ", 12 ) )
   ?? Shown( PadL( "  ab  ", 13 ) )
   ?? Shown( PadL( "  ab  ", 99 ) )
   ?? Shown( PadL( "  ab  ", 2.7 ) )
   ? ""
   ?? Shown( PadL( "Hello, World", -10 ) )
   ?? Shown( PadL( "Hello, World", -3 ) )
   ?? Shown( PadL( "Hello, World", -1 ) )
   ?? Shown( PadL( "Hello, World", 0 ) )
   ?? Shown( PadL( "Hello, World", 1 ) )
   ?? Shown( PadL( "Hello, World", 2 ) )
   ?? Shown( PadL( "Hello, World", 3 ) )
   ?? Shown( PadL( "Hello, World", 5 ) )
   ?? Shown( PadL( "Hello, World", 12 ) )
   ?? Shown( PadL( "Hello, World", 13 ) )
   ?? Shown( PadL( "Hello, World", 99 ) )
   ?? Shown( PadL( "Hello, World", 2.7 ) )
   ? ""
   ?? Shown( PadL( "a" + Chr( 0 ) + "b", -10 ) )
   ?? Shown( PadL( "a" + Chr( 0 ) + "b", -3 ) )
   ?? Shown( PadL( "a" + Chr( 0 ) + "b", -1 ) )
   ?? Shown( PadL( "a" + Chr( 0 ) + "b", 0 ) )
   ?? Shown( PadL( "a" + Chr( 0 ) + "b", 1 ) )
   ?? Shown( PadL( "a" + Chr( 0 ) + "b", 2 ) )
   ?? Shown( PadL( "a" + Chr( 0 ) + "b", 3 ) )
   ?? Shown( PadL( "a" + Chr( 0 ) + "b", 5 ) )
   ?? Shown( PadL( "a" + Chr( 0 ) + "b", 12 ) )
   ?? Shown( PadL( "a" + Chr( 0 ) + "b", 13 ) )
   ?? Shown( PadL( "a" + Chr( 0 ) + "b", 99 ) )
   ?? Shown( PadL( "a" + Chr( 0 ) + "b", 2.7 ) )
   ? ""
   ?? Shown( PadL( Chr( 200 ) + "x", -10 ) )
   ?? Shown( PadL( Chr( 200 ) + "x", -3 ) )
   ?? Shown( PadL( Chr( 200 ) + "x", -1 ) )
   ?? Shown( PadL( Chr( 200 ) + "x", 0 ) )
   ?? Shown( PadL( Chr( 200 ) + "x", 1 ) )
   ?? Shown( PadL( Chr( 200 ) + "x", 2 ) )
   ?? Shown( PadL( Chr( 200 ) + "x", 3 ) )
   ?? Shown( PadL( Chr( 200 ) + "x", 5 ) )
   ?? Shown( PadL( Chr( 200 ) + "x", 12 ) )
   ?? Shown( PadL( Chr( 200 ) + "x", 13 ) )
   ?? Shown( PadL( Chr( 200 ) + "x", 99 ) )
   ?? Shown( PadL( Chr( 200 ) + "x", 2.7 ) )
   ? ""
   ?? Shown( PadL( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -10 ) )
   ?? Shown( PadL( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -3 ) )
   ?? Shown( PadL( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -1 ) )
   ?? Shown( PadL( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 0 ) )
   ?? Shown( PadL( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 1 ) )
   ?? Shown( PadL( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 2 ) )
   ?? Shown( PadL( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 3 ) )
   ?? Shown( PadL( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 5 ) )
   ?? Shown( PadL( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 12 ) )
   ?? Shown( PadL( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 13 ) )
   ?? Shown( PadL( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 99 ) )
   ?? Shown( PadL( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 2.7 ) )
   ? "PadL(sn) runtime"
   FOR EACH v0 IN aStr
      ? ""
      FOR EACH v1 IN { -10, -3, -1, 0, 1, 2, 3, 5, 12, 13, 99, 2.7 }
         ?? Shown( PadL( v0, v1 ) )
      NEXT
   NEXT
   ? "PadL(sn) macro"
   FOR EACH v0 IN aText
      ? ""
      FOR EACH v1 IN { "-10", "-3", "-1", "0", "1", "2", "3", "5", "12", "13", "99", "2.7" }
         ?? Shown( &( "PadL( " + v0 + ", " + v1 + " )" ) )
      NEXT
   NEXT
   ? "PadL(sn) input"
   FOR EACH v0 IN aIn
      ? ""
      FOR EACH v1 IN { -10, -3, -1, 0, 1, 2, 3, 5, 12, 13, 99, 2.7 }
         ?? Shown( PadL( v0, v1 ) )
      NEXT
   NEXT
RETURN

STATIC PROCEDURE F05( aStr, aText, aIn )
   LOCAL v0, v1
   ? "PadR(sn) literal"
   ? ""
   ?? Shown( PadR( "", -10 ) )
   ?? Shown( PadR( "", -3 ) )
   ?? Shown( PadR( "", -1 ) )
   ?? Shown( PadR( "", 0 ) )
   ?? Shown( PadR( "", 1 ) )
   ?? Shown( PadR( "", 2 ) )
   ?? Shown( PadR( "", 3 ) )
   ?? Shown( PadR( "", 5 ) )
   ?? Shown( PadR( "", 12 ) )
   ?? Shown( PadR( "", 13 ) )
   ?? Shown( PadR( "", 99 ) )
   ?? Shown( PadR( "", 2.7 ) )
   ? ""
   ?? Shown( PadR( " ", -10 ) )
   ?? Shown( PadR( " ", -3 ) )
   ?? Shown( PadR( " ", -1 ) )
   ?? Shown( PadR( " ", 0 ) )
   ?? Shown( PadR( " ", 1 ) )
   ?? Shown( PadR( " ", 2 ) )
   ?? Shown( PadR( " ", 3 ) )
   ?? Shown( PadR( " ", 5 ) )
   ?? Shown( PadR( " ", 12 ) )
   ?? Shown( PadR( " ", 13 ) )
   ?? Shown( PadR( " ", 99 ) )
   ?? Shown( PadR( " ", 2.7 ) )
   ? ""
   ?? Shown( PadR( "a", -10 ) )
   ?? Shown( PadR( "a", -3 ) )
   ?? Shown( PadR( "a", -1 ) )
   ?? Shown( PadR( "a", 0 ) )
   ?? Shown( PadR( "a", 1 ) )
   ?? Shown( PadR( "a", 2 ) )
   ?? Shown( PadR( "a", 3 ) )
   ?? Shown( PadR( "a", 5 ) )
   ?? Shown( PadR( "a", 12 ) )
   ?? Shown( PadR( "a", 13 ) )
   ?? Shown( PadR( "a", 99 ) )
   ?? Shown( PadR( "a", 2.7 ) )
   ? ""
   ?? Shown( PadR( "abc", -10 ) )
   ?? Shown( PadR( "abc", -3 ) )
   ?? Shown( PadR( "abc", -1 ) )
   ?? Shown( PadR( "abc", 0 ) )
   ?? Shown( PadR( "abc", 1 ) )
   ?? Shown( PadR( "abc", 2 ) )
   ?? Shown( PadR( "abc", 3 ) )
   ?? Shown( PadR( "abc", 5 ) )
   ?? Shown( PadR( "abc", 12 ) )
   ?? Shown( PadR( "abc", 13 ) )
   ?? Shown( PadR( "abc", 99 ) )
   ?? Shown( PadR( "abc", 2.7 ) )
   ? ""
   ?? Shown( PadR( "  ab  ", -10 ) )
   ?? Shown( PadR( "  ab  ", -3 ) )
   ?? Shown( PadR( "  ab  ", -1 ) )
   ?? Shown( PadR( "  ab  ", 0 ) )
   ?? Shown( PadR( "  ab  ", 1 ) )
   ?? Shown( PadR( "  ab  ", 2 ) )
   ?? Shown( PadR( "  ab  ", 3 ) )
   ?? Shown( PadR( "  ab  ", 5 ) )
   ?? Shown( PadR( "  ab  ", 12 ) )
   ?? Shown( PadR( "  ab  ", 13 ) )
   ?? Shown( PadR( "  ab  ", 99 ) )
   ?? Shown( PadR( "  ab  ", 2.7 ) )
   ? ""
   ?? Shown( PadR( "Hello, World", -10 ) )
   ?? Shown( PadR( "Hello, World", -3 ) )
   ?? Shown( PadR( "Hello, World", -1 ) )
   ?? Shown( PadR( "Hello, World", 0 ) )
   ?? Shown( PadR( "Hello, World", 1 ) )
   ?? Shown( PadR( "Hello, World", 2 ) )
   ?? Shown( PadR( "Hello, World", 3 ) )
   ?? Shown( PadR( "Hello, World", 5 ) )
   ?? Shown( PadR( "Hello, World", 12 ) )
   ?? Shown( PadR( "Hello, World", 13 ) )
   ?? Shown( PadR( "Hello, World", 99 ) )
   ?? Shown( PadR( "Hello, World", 2.7 ) )
   ? ""
   ?? Shown( PadR( "a" + Chr( 0 ) + "b", -10 ) )
   ?? Shown( PadR( "a" + Chr( 0 ) + "b", -3 ) )
   ?? Shown( PadR( "a" + Chr( 0 ) + "b", -1 ) )
   ?? Shown( PadR( "a" + Chr( 0 ) + "b", 0 ) )
   ?? Shown( PadR( "a" + Chr( 0 ) + "b", 1 ) )
   ?? Shown( PadR( "a" + Chr( 0 ) + "b", 2 ) )
   ?? Shown( PadR( "a" + Chr( 0 ) + "b", 3 ) )
   ?? Shown( PadR( "a" + Chr( 0 ) + "b", 5 ) )
   ?? Shown( PadR( "a" + Chr( 0 ) + "b", 12 ) )
   ?? Shown( PadR( "a" + Chr( 0 ) + "b", 13 ) )
   ?? Shown( PadR( "a" + Chr( 0 ) + "b", 99 ) )
   ?? Shown( PadR( "a" + Chr( 0 ) + "b", 2.7 ) )
   ? ""
   ?? Shown( PadR( Chr( 200 ) + "x", -10 ) )
   ?? Shown( PadR( Chr( 200 ) + "x", -3 ) )
   ?? Shown( PadR( Chr( 200 ) + "x", -1 ) )
   ?? Shown( PadR( Chr( 200 ) + "x", 0 ) )
   ?? Shown( PadR( Chr( 200 ) + "x", 1 ) )
   ?? Shown( PadR( Chr( 200 ) + "x", 2 ) )
   ?? Shown( PadR( Chr( 200 ) + "x", 3 ) )
   ?? Shown( PadR( Chr( 200 ) + "x", 5 ) )
   ?? Shown( PadR( Chr( 200 ) + "x", 12 ) )
   ?? Shown( PadR( Chr( 200 ) + "x", 13 ) )
   ?? Shown( PadR( Chr( 200 ) + "x", 99 ) )
   ?? Shown( PadR( Chr( 200 ) + "x", 2.7 ) )
   ? ""
   ?? Shown( PadR( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -10 ) )
   ?? Shown( PadR( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -3 ) )
   ?? Shown( PadR( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -1 ) )
   ?? Shown( PadR( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 0 ) )
   ?? Shown( PadR( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 1 ) )
   ?? Shown( PadR( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 2 ) )
   ?? Shown( PadR( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 3 ) )
   ?? Shown( PadR( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 5 ) )
   ?? Shown( PadR( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 12 ) )
   ?? Shown( PadR( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 13 ) )
   ?? Shown( PadR( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 99 ) )
   ?? Shown( PadR( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 2.7 ) )
   ? "PadR(sn) runtime"
   FOR EACH v0 IN aStr
      ? ""
      FOR EACH v1 IN { -10, -3, -1, 0, 1, 2, 3, 5, 12, 13, 99, 2.7 }
         ?? Shown( PadR( v0, v1 ) )
      NEXT
   NEXT
   ? "PadR(sn) macro"
   FOR EACH v0 IN aText
      ? ""
      FOR EACH v1 IN { "-10", "-3", "-1", "0", "1", "2", "3", "5", "12", "13", "99", "2.7" }
         ?? Shown( &( "PadR( " + v0 + ", " + v1 + " )" ) )
      NEXT
   NEXT
   ? "PadR(sn) input"
   FOR EACH v0 IN aIn
      ? ""
      FOR EACH v1 IN { -10, -3, -1, 0, 1, 2, 3, 5, 12, 13, 99, 2.7 }
         ?? Shown( PadR( v0, v1 ) )
      NEXT
   NEXT
RETURN

STATIC PROCEDURE F06( aStr, aText, aIn )
   LOCAL v0, v1
   ? "PadC(sn) literal"
   ? ""
   ?? Shown( PadC( "", -10 ) )
   ?? Shown( PadC( "", -3 ) )
   ?? Shown( PadC( "", -1 ) )
   ?? Shown( PadC( "", 0 ) )
   ?? Shown( PadC( "", 1 ) )
   ?? Shown( PadC( "", 2 ) )
   ?? Shown( PadC( "", 3 ) )
   ?? Shown( PadC( "", 5 ) )
   ?? Shown( PadC( "", 12 ) )
   ?? Shown( PadC( "", 13 ) )
   ?? Shown( PadC( "", 99 ) )
   ?? Shown( PadC( "", 2.7 ) )
   ? ""
   ?? Shown( PadC( " ", -10 ) )
   ?? Shown( PadC( " ", -3 ) )
   ?? Shown( PadC( " ", -1 ) )
   ?? Shown( PadC( " ", 0 ) )
   ?? Shown( PadC( " ", 1 ) )
   ?? Shown( PadC( " ", 2 ) )
   ?? Shown( PadC( " ", 3 ) )
   ?? Shown( PadC( " ", 5 ) )
   ?? Shown( PadC( " ", 12 ) )
   ?? Shown( PadC( " ", 13 ) )
   ?? Shown( PadC( " ", 99 ) )
   ?? Shown( PadC( " ", 2.7 ) )
   ? ""
   ?? Shown( PadC( "a", -10 ) )
   ?? Shown( PadC( "a", -3 ) )
   ?? Shown( PadC( "a", -1 ) )
   ?? Shown( PadC( "a", 0 ) )
   ?? Shown( PadC( "a", 1 ) )
   ?? Shown( PadC( "a", 2 ) )
   ?? Shown( PadC( "a", 3 ) )
   ?? Shown( PadC( "a", 5 ) )
   ?? Shown( PadC( "a", 12 ) )
   ?? Shown( PadC( "a", 13 ) )
   ?? Shown( PadC( "a", 99 ) )
   ?? Shown( PadC( "a", 2.7 ) )
   ? ""
   ?? Shown( PadC( "abc", -10 ) )
   ?? Shown( PadC( "abc", -3 ) )
   ?? Shown( PadC( "abc", -1 ) )
   ?? Shown( PadC( "abc", 0 ) )
   ?? Shown( PadC( "abc", 1 ) )
   ?? Shown( PadC( "abc", 2 ) )
   ?? Shown( PadC( "abc", 3 ) )
   ?? Shown( PadC( "abc", 5 ) )
   ?? Shown( PadC( "abc", 12 ) )
   ?? Shown( PadC( "abc", 13 ) )
   ?? Shown( PadC( "abc", 99 ) )
   ?? Shown( PadC( "abc", 2.7 ) )
   ? ""
   ?? Shown( PadC( "  ab  ", -10 ) )
   ?? Shown( PadC( "  ab  ", -3 ) )
   ?? Shown( PadC( "  ab  ", -1 ) )
   ?? Shown( PadC( "  ab  ", 0 ) )
   ?? Shown( PadC( "  ab  ", 1 ) )
   ?? Shown( PadC( "  ab  ", 2 ) )
   ?? Shown( PadC( "  ab  ", 3 ) )
   ?? Shown( PadC( "  ab  ", 5 ) )
   ?? Shown( PadC( "  ab  ", 12 ) )
   ?? Shown( PadC( "  ab  ", 13 ) )
   ?? Shown( PadC( "  ab  ", 99 ) )
   ?? Shown( PadC( "  ab  ", 2.7 ) )
   ? ""
   ?? Shown( PadC( "Hello, World", -10 ) )
   ?? Shown( PadC( "Hello, World", -3 ) )
   ?? Shown( PadC( "Hello, World", -1 ) )
   ?? Shown( PadC( "Hello, World", 0 ) )
   ?? Shown( PadC( "Hello, World", 1 ) )
   ?? Shown( PadC( "Hello, World", 2 ) )
   ?? Shown( PadC( "Hello, World", 3 ) )
   ?? Shown( PadC( "Hello, World", 5 ) )
   ?? Shown( PadC( "Hello, World", 12 ) )
   ?? Shown( PadC( "Hello, World", 13 ) )
   ?? Shown( PadC( "Hello, World", 99 ) )
   ?? Shown( PadC( "Hello, World", 2.7 ) )
   ? ""
   ?? Shown( PadC( "a" + Chr( 0 ) + "b", -10 ) )
   ?? Shown( PadC( "a" + Chr( 0 ) + "b", -3 ) )
   ?? Shown( PadC( "a" + Chr( 0 ) + "b", -1 ) )
   ?? Shown( PadC( "a" + Chr( 0 ) + "b", 0 ) )
   ?? Shown( PadC( "a" + Chr( 0 ) + "b", 1 ) )
   ?? Shown( PadC( "a" + Chr( 0 ) + "b", 2 ) )
   ?? Shown( PadC( "a" + Chr( 0 ) + "b", 3 ) )
   ?? Shown( PadC( "a" + Chr( 0 ) + "b", 5 ) )
   ?? Shown( PadC( "a" + Chr( 0 ) + "b", 12 ) )
   ?? Shown( PadC( "a" + Chr( 0 ) + "b", 13 ) )
   ?? Shown( PadC( "a" + Chr( 0 ) + "b", 99 ) )
   ?? Shown( PadC( "a" + Chr( 0 ) + "b", 2.7 ) )
   ? ""
   ?? Shown( PadC( Chr( 200 ) + "x", -10 ) )
   ?? Shown( PadC( Chr( 200 ) + "x", -3 ) )
   ?? Shown( PadC( Chr( 200 ) + "x", -1 ) )
   ?? Shown( PadC( Chr( 200 ) + "x", 0 ) )
   ?? Shown( PadC( Chr( 200 ) + "x", 1 ) )
   ?? Shown( PadC( Chr( 200 ) + "x", 2 ) )
   ?? Shown( PadC( Chr( 200 ) + "x", 3 ) )
   ?? Shown( PadC( Chr( 200 ) + "x", 5 ) )
   ?? Shown( PadC( Chr( 200 ) + "x", 12 ) )
   ?? Shown( PadC( Chr( 200 ) + "x", 13 ) )
   ?? Shown( PadC( Chr( 200 ) + "x", 99 ) )
   ?? Shown( PadC( Chr( 200 ) + "x", 2.7 ) )
   ? ""
   ?? Shown( PadC( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -10 ) )
   ?? Shown( PadC( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -3 ) )
   ?? Shown( PadC( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -1 ) )
   ?? Shown( PadC( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 0 ) )
   ?? Shown( PadC( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 1 ) )
   ?? Shown( PadC( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 2 ) )
   ?? Shown( PadC( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 3 ) )
   ?? Shown( PadC( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 5 ) )
   ?? Shown( PadC( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 12 ) )
   ?? Shown( PadC( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 13 ) )
   ?? Shown( PadC( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 99 ) )
   ?? Shown( PadC( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 2.7 ) )
   ? "PadC(sn) runtime"
   FOR EACH v0 IN aStr
      ? ""
      FOR EACH v1 IN { -10, -3, -1, 0, 1, 2, 3, 5, 12, 13, 99, 2.7 }
         ?? Shown( PadC( v0, v1 ) )
      NEXT
   NEXT
   ? "PadC(sn) macro"
   FOR EACH v0 IN aText
      ? ""
      FOR EACH v1 IN { "-10", "-3", "-1", "0", "1", "2", "3", "5", "12", "13", "99", "2.7" }
         ?? Shown( &( "PadC( " + v0 + ", " + v1 + " )" ) )
      NEXT
   NEXT
   ? "PadC(sn) input"
   FOR EACH v0 IN aIn
      ? ""
      FOR EACH v1 IN { -10, -3, -1, 0, 1, 2, 3, 5, 12, 13, 99, 2.7 }
         ?? Shown( PadC( v0, v1 ) )
      NEXT
   NEXT
RETURN

STATIC PROCEDURE F07( aStr, aText, aIn )
   LOCAL v0, v1
   ? "PadC(sn*) literal"
   ? ""
   ?? Shown( PadC( "", -10, "*" ) )
   ?? Shown( PadC( "", -3, "*" ) )
   ?? Shown( PadC( "", -1, "*" ) )
   ?? Shown( PadC( "", 0, "*" ) )
   ?? Shown( PadC( "", 1, "*" ) )
   ?? Shown( PadC( "", 2, "*" ) )
   ?? Shown( PadC( "", 3, "*" ) )
   ?? Shown( PadC( "", 5, "*" ) )
   ?? Shown( PadC( "", 12, "*" ) )
   ?? Shown( PadC( "", 13, "*" ) )
   ?? Shown( PadC( "", 99, "*" ) )
   ?? Shown( PadC( "", 2.7, "*" ) )
   ? ""
   ?? Shown( PadC( " ", -10, "*" ) )
   ?? Shown( PadC( " ", -3, "*" ) )
   ?? Shown( PadC( " ", -1, "*" ) )
   ?? Shown( PadC( " ", 0, "*" ) )
   ?? Shown( PadC( " ", 1, "*" ) )
   ?? Shown( PadC( " ", 2, "*" ) )
   ?? Shown( PadC( " ", 3, "*" ) )
   ?? Shown( PadC( " ", 5, "*" ) )
   ?? Shown( PadC( " ", 12, "*" ) )
   ?? Shown( PadC( " ", 13, "*" ) )
   ?? Shown( PadC( " ", 99, "*" ) )
   ?? Shown( PadC( " ", 2.7, "*" ) )
   ? ""
   ?? Shown( PadC( "a", -10, "*" ) )
   ?? Shown( PadC( "a", -3, "*" ) )
   ?? Shown( PadC( "a", -1, "*" ) )
   ?? Shown( PadC( "a", 0, "*" ) )
   ?? Shown( PadC( "a", 1, "*" ) )
   ?? Shown( PadC( "a", 2, "*" ) )
   ?? Shown( PadC( "a", 3, "*" ) )
   ?? Shown( PadC( "a", 5, "*" ) )
   ?? Shown( PadC( "a", 12, "*" ) )
   ?? Shown( PadC( "a", 13, "*" ) )
   ?? Shown( PadC( "a", 99, "*" ) )
   ?? Shown( PadC( "a", 2.7, "*" ) )
   ? ""
   ?? Shown( PadC( "abc", -10, "*" ) )
   ?? Shown( PadC( "abc", -3, "*" ) )
   ?? Shown( PadC( "abc", -1, "*" ) )
   ?? Shown( PadC( "abc", 0, "*" ) )
   ?? Shown( PadC( "abc", 1, "*" ) )
   ?? Shown( PadC( "abc", 2, "*" ) )
   ?? Shown( PadC( "abc", 3, "*" ) )
   ?? Shown( PadC( "abc", 5, "*" ) )
   ?? Shown( PadC( "abc", 12, "*" ) )
   ?? Shown( PadC( "abc", 13, "*" ) )
   ?? Shown( PadC( "abc", 99, "*" ) )
   ?? Shown( PadC( "abc", 2.7, "*" ) )
   ? ""
   ?? Shown( PadC( "  ab  ", -10, "*" ) )
   ?? Shown( PadC( "  ab  ", -3, "*" ) )
   ?? Shown( PadC( "  ab  ", -1, "*" ) )
   ?? Shown( PadC( "  ab  ", 0, "*" ) )
   ?? Shown( PadC( "  ab  ", 1, "*" ) )
   ?? Shown( PadC( "  ab  ", 2, "*" ) )
   ?? Shown( PadC( "  ab  ", 3, "*" ) )
   ?? Shown( PadC( "  ab  ", 5, "*" ) )
   ?? Shown( PadC( "  ab  ", 12, "*" ) )
   ?? Shown( PadC( "  ab  ", 13, "*" ) )
   ?? Shown( PadC( "  ab  ", 99, "*" ) )
   ?? Shown( PadC( "  ab  ", 2.7, "*" ) )
   ? ""
   ?? Shown( PadC( "Hello, World", -10, "*" ) )
   ?? Shown( PadC( "Hello, World", -3, "*" ) )
   ?? Shown( PadC( "Hello, World", -1, "*" ) )
   ?? Shown( PadC( "Hello, World", 0, "*" ) )
   ?? Shown( PadC( "Hello, World", 1, "*" ) )
   ?? Shown( PadC( "Hello, World", 2, "*" ) )
   ?? Shown( PadC( "Hello, World", 3, "*" ) )
   ?? Shown( PadC( "Hello, World", 5, "*" ) )
   ?? Shown( PadC( "Hello, World", 12, "*" ) )
   ?? Shown( PadC( "Hello, World", 13, "*" ) )
   ?? Shown( PadC( "Hello, World", 99, "*" ) )
   ?? Shown( PadC( "Hello, World", 2.7, "*" ) )
   ? ""
   ?? Shown( PadC( "a" + Chr( 0 ) + "b", -10, "*" ) )
   ?? Shown( PadC( "a" + Chr( 0 ) + "b", -3, "*" ) )
   ?? Shown( PadC( "a" + Chr( 0 ) + "b", -1, "*" ) )
   ?? Shown( PadC( "a" + Chr( 0 ) + "b", 0, "*" ) )
   ?? Shown( PadC( "a" + Chr( 0 ) + "b", 1, "*" ) )
   ?? Shown( PadC( "a" + Chr( 0 ) + "b", 2, "*" ) )
   ?? Shown( PadC( "a" + Chr( 0 ) + "b", 3, "*" ) )
   ?? Shown( PadC( "a" + Chr( 0 ) + "b", 5, "*" ) )
   ?? Shown( PadC( "a" + Chr( 0 ) + "b", 12, "*" ) )
   ?? Shown( PadC( "a" + Chr( 0 ) + "b", 13, "*" ) )
   ?? Shown( PadC( "a" + Chr( 0 ) + "b", 99, "*" ) )
   ?? Shown( PadC( "a" + Chr( 0 ) + "b", 2.7, "*" ) )
   ? ""
   ?? Shown( PadC( Chr( 200 ) + "x", -10, "*" ) )
   ?? Shown( PadC( Chr( 200 ) + "x", -3, "*" ) )
   ?? Shown( PadC( Chr( 200 ) + "x", -1, "*" ) )
   ?? Shown( PadC( Chr( 200 ) + "x", 0, "*" ) )
   ?? Shown( PadC( Chr( 200 ) + "x", 1, "*" ) )
   ?? Shown( PadC( Chr( 200 ) + "x", 2, "*" ) )
   ?? Shown( PadC( Chr( 200 ) + "x", 3, "*" ) )
   ?? Shown( PadC( Chr( 200 ) + "x", 5, "*" ) )
   ?? Shown( PadC( Chr( 200 ) + "x", 12, "*" ) )
   ?? Shown( PadC( Chr( 200 ) + "x", 13, "*" ) )
   ?? Shown( PadC( Chr( 200 ) + "x", 99, "*" ) )
   ?? Shown( PadC( Chr( 200 ) + "x", 2.7, "*" ) )
   ? ""
   ?? Shown( PadC( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -10, "*" ) )
   ?? Shown( PadC( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -3, "*" ) )
   ?? Shown( PadC( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -1, "*" ) )
   ?? Shown( PadC( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 0, "*" ) )
   ?? Shown( PadC( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 1, "*" ) )
   ?? Shown( PadC( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 2, "*" ) )
   ?? Shown( PadC( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 3, "*" ) )
   ?? Shown( PadC( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 5, "*" ) )
   ?? Shown( PadC( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 12, "*" ) )
   ?? Shown( PadC( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 13, "*" ) )
   ?? Shown( PadC( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 99, "*" ) )
   ?? Shown( PadC( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 2.7, "*" ) )
   ? "PadC(sn*) runtime"
   FOR EACH v0 IN aStr
      ? ""
      FOR EACH v1 IN { -10, -3, -1, 0, 1, 2, 3, 5, 12, 13, 99, 2.7 }
         ?? Shown( PadC( v0, v1, "*" ) )
      NEXT
   NEXT
   ? "PadC(sn*) macro"
   FOR EACH v0 IN aText
      ? ""
      FOR EACH v1 IN { "-10", "-3", "-1", "0", "1", "2", "3", "5", "12", "13", "99", "2.7" }
         ?? Shown( &( "PadC( " + v0 + ", " + v1 + ", " + "'*'" + " )" ) )
      NEXT
   NEXT
   ? "PadC(sn*) input"
   FOR EACH v0 IN aIn
      ? ""
      FOR EACH v1 IN { -10, -3, -1, 0, 1, 2, 3, 5, 12, 13, 99, 2.7 }
         ?? Shown( PadC( v0, v1, "*" ) )
      NEXT
   NEXT
RETURN

STATIC PROCEDURE F08( aStr, aText, aIn )
   LOCAL v0, v1
   ? "Replicate(sm) literal"
   ? ""
   ?? Shown( Replicate( "", -1 ) )
   ?? Shown( Replicate( "", 0 ) )
   ?? Shown( Replicate( "", 1 ) )
   ?? Shown( Replicate( "", 2 ) )
   ?? Shown( Replicate( "", 5 ) )
   ?? Shown( Replicate( "", 99 ) )
   ? ""
   ?? Shown( Replicate( " ", -1 ) )
   ?? Shown( Replicate( " ", 0 ) )
   ?? Shown( Replicate( " ", 1 ) )
   ?? Shown( Replicate( " ", 2 ) )
   ?? Shown( Replicate( " ", 5 ) )
   ?? Shown( Replicate( " ", 99 ) )
   ? ""
   ?? Shown( Replicate( "a", -1 ) )
   ?? Shown( Replicate( "a", 0 ) )
   ?? Shown( Replicate( "a", 1 ) )
   ?? Shown( Replicate( "a", 2 ) )
   ?? Shown( Replicate( "a", 5 ) )
   ?? Shown( Replicate( "a", 99 ) )
   ? ""
   ?? Shown( Replicate( "abc", -1 ) )
   ?? Shown( Replicate( "abc", 0 ) )
   ?? Shown( Replicate( "abc", 1 ) )
   ?? Shown( Replicate( "abc", 2 ) )
   ?? Shown( Replicate( "abc", 5 ) )
   ?? Shown( Replicate( "abc", 99 ) )
   ? ""
   ?? Shown( Replicate( "  ab  ", -1 ) )
   ?? Shown( Replicate( "  ab  ", 0 ) )
   ?? Shown( Replicate( "  ab  ", 1 ) )
   ?? Shown( Replicate( "  ab  ", 2 ) )
   ?? Shown( Replicate( "  ab  ", 5 ) )
   ?? Shown( Replicate( "  ab  ", 99 ) )
   ? ""
   ?? Shown( Replicate( "Hello, World", -1 ) )
   ?? Shown( Replicate( "Hello, World", 0 ) )
   ?? Shown( Replicate( "Hello, World", 1 ) )
   ?? Shown( Replicate( "Hello, World", 2 ) )
   ?? Shown( Replicate( "Hello, World", 5 ) )
   ?? Shown( Replicate( "Hello, World", 99 ) )
   ? ""
   ?? Shown( Replicate( "a" + Chr( 0 ) + "b", -1 ) )
   ?? Shown( Replicate( "a" + Chr( 0 ) + "b", 0 ) )
   ?? Shown( Replicate( "a" + Chr( 0 ) + "b", 1 ) )
   ?? Shown( Replicate( "a" + Chr( 0 ) + "b", 2 ) )
   ?? Shown( Replicate( "a" + Chr( 0 ) + "b", 5 ) )
   ?? Shown( Replicate( "a" + Chr( 0 ) + "b", 99 ) )
   ? ""
   ?? Shown( Replicate( Chr( 200 ) + "x", -1 ) )
   ?? Shown( Replicate( Chr( 200 ) + "x", 0 ) )
   ?? Shown( Replicate( Chr( 200 ) + "x", 1 ) )
   ?? Shown( Replicate( Chr( 200 ) + "x", 2 ) )
   ?? Shown( Replicate( Chr( 200 ) + "x", 5 ) )
   ?? Shown( Replicate( Chr( 200 ) + "x", 99 ) )
   ? ""
   ?? Shown( Replicate( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -1 ) )
   ?? Shown( Replicate( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 0 ) )
   ?? Shown( Replicate( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 1 ) )
   ?? Shown( Replicate( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 2 ) )
   ?? Shown( Replicate( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 5 ) )
   ?? Shown( Replicate( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 99 ) )
   ? "Replicate(sm) runtime"
   FOR EACH v0 IN aStr
      ? ""
      FOR EACH v1 IN { -1, 0, 1, 2, 5, 99 }
         ?? Shown( Replicate( v0, v1 ) )
      NEXT
   NEXT
   ? "Replicate(sm) macro"
   FOR EACH v0 IN aText
      ? ""
      FOR EACH v1 IN { "-1", "0", "1", "2", "5", "99" }
         ?? Shown( &( "Replicate( " + v0 + ", " + v1 + " )" ) )
      NEXT
   NEXT
   ? "Replicate(sm) input"
   FOR EACH v0 IN aIn
      ? ""
      FOR EACH v1 IN { -1, 0, 1, 2, 5, 99 }
         ?? Shown( Replicate( v0, v1 ) )
      NEXT
   NEXT
RETURN

STATIC PROCEDURE F09( aStr, aText, aIn )
   LOCAL v0, v1, v2
   ? "Stuff(snm+) literal"
   ? ""
   ?? Shown( Stuff( "", -10, -1, "XY" ) )
   ?? Shown( Stuff( "", -10, 0, "XY" ) )
   ?? Shown( Stuff( "", -10, 1, "XY" ) )
   ?? Shown( Stuff( "", -10, 2, "XY" ) )
   ?? Shown( Stuff( "", -10, 5, "XY" ) )
   ?? Shown( Stuff( "", -10, 99, "XY" ) )
   ?? Shown( Stuff( "", -3, -1, "XY" ) )
   ?? Shown( Stuff( "", -3, 0, "XY" ) )
   ?? Shown( Stuff( "", -3, 1, "XY" ) )
   ?? Shown( Stuff( "", -3, 2, "XY" ) )
   ?? Shown( Stuff( "", -3, 5, "XY" ) )
   ?? Shown( Stuff( "", -3, 99, "XY" ) )
   ?? Shown( Stuff( "", -1, -1, "XY" ) )
   ?? Shown( Stuff( "", -1, 0, "XY" ) )
   ?? Shown( Stuff( "", -1, 1, "XY" ) )
   ?? Shown( Stuff( "", -1, 2, "XY" ) )
   ?? Shown( Stuff( "", -1, 5, "XY" ) )
   ?? Shown( Stuff( "", -1, 99, "XY" ) )
   ?? Shown( Stuff( "", 0, -1, "XY" ) )
   ?? Shown( Stuff( "", 0, 0, "XY" ) )
   ?? Shown( Stuff( "", 0, 1, "XY" ) )
   ?? Shown( Stuff( "", 0, 2, "XY" ) )
   ?? Shown( Stuff( "", 0, 5, "XY" ) )
   ?? Shown( Stuff( "", 0, 99, "XY" ) )
   ?? Shown( Stuff( "", 1, -1, "XY" ) )
   ?? Shown( Stuff( "", 1, 0, "XY" ) )
   ?? Shown( Stuff( "", 1, 1, "XY" ) )
   ?? Shown( Stuff( "", 1, 2, "XY" ) )
   ?? Shown( Stuff( "", 1, 5, "XY" ) )
   ?? Shown( Stuff( "", 1, 99, "XY" ) )
   ?? Shown( Stuff( "", 2, -1, "XY" ) )
   ?? Shown( Stuff( "", 2, 0, "XY" ) )
   ?? Shown( Stuff( "", 2, 1, "XY" ) )
   ?? Shown( Stuff( "", 2, 2, "XY" ) )
   ?? Shown( Stuff( "", 2, 5, "XY" ) )
   ?? Shown( Stuff( "", 2, 99, "XY" ) )
   ?? Shown( Stuff( "", 3, -1, "XY" ) )
   ?? Shown( Stuff( "", 3, 0, "XY" ) )
   ?? Shown( Stuff( "", 3, 1, "XY" ) )
   ?? Shown( Stuff( "", 3, 2, "XY" ) )
   ?? Shown( Stuff( "", 3, 5, "XY" ) )
   ?? Shown( Stuff( "", 3, 99, "XY" ) )
   ?? Shown( Stuff( "", 5, -1, "XY" ) )
   ?? Shown( Stuff( "", 5, 0, "XY" ) )
   ?? Shown( Stuff( "", 5, 1, "XY" ) )
   ?? Shown( Stuff( "", 5, 2, "XY" ) )
   ?? Shown( Stuff( "", 5, 5, "XY" ) )
   ?? Shown( Stuff( "", 5, 99, "XY" ) )
   ?? Shown( Stuff( "", 12, -1, "XY" ) )
   ?? Shown( Stuff( "", 12, 0, "XY" ) )
   ?? Shown( Stuff( "", 12, 1, "XY" ) )
   ?? Shown( Stuff( "", 12, 2, "XY" ) )
   ?? Shown( Stuff( "", 12, 5, "XY" ) )
   ?? Shown( Stuff( "", 12, 99, "XY" ) )
   ?? Shown( Stuff( "", 13, -1, "XY" ) )
   ?? Shown( Stuff( "", 13, 0, "XY" ) )
   ?? Shown( Stuff( "", 13, 1, "XY" ) )
   ?? Shown( Stuff( "", 13, 2, "XY" ) )
   ?? Shown( Stuff( "", 13, 5, "XY" ) )
   ?? Shown( Stuff( "", 13, 99, "XY" ) )
   ?? Shown( Stuff( "", 99, -1, "XY" ) )
   ?? Shown( Stuff( "", 99, 0, "XY" ) )
   ?? Shown( Stuff( "", 99, 1, "XY" ) )
   ?? Shown( Stuff( "", 99, 2, "XY" ) )
   ?? Shown( Stuff( "", 99, 5, "XY" ) )
   ?? Shown( Stuff( "", 99, 99, "XY" ) )
   ?? Shown( Stuff( "", 2.7, -1, "XY" ) )
   ?? Shown( Stuff( "", 2.7, 0, "XY" ) )
   ?? Shown( Stuff( "", 2.7, 1, "XY" ) )
   ?? Shown( Stuff( "", 2.7, 2, "XY" ) )
   ?? Shown( Stuff( "", 2.7, 5, "XY" ) )
   ?? Shown( Stuff( "", 2.7, 99, "XY" ) )
   ? ""
   ?? Shown( Stuff( " ", -10, -1, "XY" ) )
   ?? Shown( Stuff( " ", -10, 0, "XY" ) )
   ?? Shown( Stuff( " ", -10, 1, "XY" ) )
   ?? Shown( Stuff( " ", -10, 2, "XY" ) )
   ?? Shown( Stuff( " ", -10, 5, "XY" ) )
   ?? Shown( Stuff( " ", -10, 99, "XY" ) )
   ?? Shown( Stuff( " ", -3, -1, "XY" ) )
   ?? Shown( Stuff( " ", -3, 0, "XY" ) )
   ?? Shown( Stuff( " ", -3, 1, "XY" ) )
   ?? Shown( Stuff( " ", -3, 2, "XY" ) )
   ?? Shown( Stuff( " ", -3, 5, "XY" ) )
   ?? Shown( Stuff( " ", -3, 99, "XY" ) )
   ?? Shown( Stuff( " ", -1, -1, "XY" ) )
   ?? Shown( Stuff( " ", -1, 0, "XY" ) )
   ?? Shown( Stuff( " ", -1, 1, "XY" ) )
   ?? Shown( Stuff( " ", -1, 2, "XY" ) )
   ?? Shown( Stuff( " ", -1, 5, "XY" ) )
   ?? Shown( Stuff( " ", -1, 99, "XY" ) )
   ?? Shown( Stuff( " ", 0, -1, "XY" ) )
   ?? Shown( Stuff( " ", 0, 0, "XY" ) )
   ?? Shown( Stuff( " ", 0, 1, "XY" ) )
   ?? Shown( Stuff( " ", 0, 2, "XY" ) )
   ?? Shown( Stuff( " ", 0, 5, "XY" ) )
   ?? Shown( Stuff( " ", 0, 99, "XY" ) )
   ?? Shown( Stuff( " ", 1, -1, "XY" ) )
   ?? Shown( Stuff( " ", 1, 0, "XY" ) )
   ?? Shown( Stuff( " ", 1, 1, "XY" ) )
   ?? Shown( Stuff( " ", 1, 2, "XY" ) )
   ?? Shown( Stuff( " ", 1, 5, "XY" ) )
   ?? Shown( Stuff( " ", 1, 99, "XY" ) )
   ?? Shown( Stuff( " ", 2, -1, "XY" ) )
   ?? Shown( Stuff( " ", 2, 0, "XY" ) )
   ?? Shown( Stuff( " ", 2, 1, "XY" ) )
   ?? Shown( Stuff( " ", 2, 2, "XY" ) )
   ?? Shown( Stuff( " ", 2, 5, "XY" ) )
   ?? Shown( Stuff( " ", 2, 99, "XY" ) )
   ?? Shown( Stuff( " ", 3, -1, "XY" ) )
   ?? Shown( Stuff( " ", 3, 0, "XY" ) )
   ?? Shown( Stuff( " ", 3, 1, "XY" ) )
   ?? Shown( Stuff( " ", 3, 2, "XY" ) )
   ?? Shown( Stuff( " ", 3, 5, "XY" ) )
   ?? Shown( Stuff( " ", 3, 99, "XY" ) )
   ?? Shown( Stuff( " ", 5, -1, "XY" ) )
   ?? Shown( Stuff( " ", 5, 0, "XY" ) )
   ?? Shown( Stuff( " ", 5, 1, "XY" ) )
   ?? Shown( Stuff( " ", 5, 2, "XY" ) )
   ?? Shown( Stuff( " ", 5, 5, "XY" ) )
   ?? Shown( Stuff( " ", 5, 99, "XY" ) )
   ?? Shown( Stuff( " ", 12, -1, "XY" ) )
   ?? Shown( Stuff( " ", 12, 0, "XY" ) )
   ?? Shown( Stuff( " ", 12, 1, "XY" ) )
   ?? Shown( Stuff( " ", 12, 2, "XY" ) )
   ?? Shown( Stuff( " ", 12, 5, "XY" ) )
   ?? Shown( Stuff( " ", 12, 99, "XY" ) )
   ?? Shown( Stuff( " ", 13, -1, "XY" ) )
   ?? Shown( Stuff( " ", 13, 0, "XY" ) )
   ?? Shown( Stuff( " ", 13, 1, "XY" ) )
   ?? Shown( Stuff( " ", 13, 2, "XY" ) )
   ?? Shown( Stuff( " ", 13, 5, "XY" ) )
   ?? Shown( Stuff( " ", 13, 99, "XY" ) )
   ?? Shown( Stuff( " ", 99, -1, "XY" ) )
   ?? Shown( Stuff( " ", 99, 0, "XY" ) )
   ?? Shown( Stuff( " ", 99, 1, "XY" ) )
   ?? Shown( Stuff( " ", 99, 2, "XY" ) )
   ?? Shown( Stuff( " ", 99, 5, "XY" ) )
   ?? Shown( Stuff( " ", 99, 99, "XY" ) )
   ?? Shown( Stuff( " ", 2.7, -1, "XY" ) )
   ?? Shown( Stuff( " ", 2.7, 0, "XY" ) )
   ?? Shown( Stuff( " ", 2.7, 1, "XY" ) )
   ?? Shown( Stuff( " ", 2.7, 2, "XY" ) )
   ?? Shown( Stuff( " ", 2.7, 5, "XY" ) )
   ?? Shown( Stuff( " ", 2.7, 99, "XY" ) )
   ? ""
   ?? Shown( Stuff( "a", -10, -1, "XY" ) )
   ?? Shown( Stuff( "a", -10, 0, "XY" ) )
   ?? Shown( Stuff( "a", -10, 1, "XY" ) )
   ?? Shown( Stuff( "a", -10, 2, "XY" ) )
   ?? Shown( Stuff( "a", -10, 5, "XY" ) )
   ?? Shown( Stuff( "a", -10, 99, "XY" ) )
   ?? Shown( Stuff( "a", -3, -1, "XY" ) )
   ?? Shown( Stuff( "a", -3, 0, "XY" ) )
   ?? Shown( Stuff( "a", -3, 1, "XY" ) )
   ?? Shown( Stuff( "a", -3, 2, "XY" ) )
   ?? Shown( Stuff( "a", -3, 5, "XY" ) )
   ?? Shown( Stuff( "a", -3, 99, "XY" ) )
   ?? Shown( Stuff( "a", -1, -1, "XY" ) )
   ?? Shown( Stuff( "a", -1, 0, "XY" ) )
   ?? Shown( Stuff( "a", -1, 1, "XY" ) )
   ?? Shown( Stuff( "a", -1, 2, "XY" ) )
   ?? Shown( Stuff( "a", -1, 5, "XY" ) )
   ?? Shown( Stuff( "a", -1, 99, "XY" ) )
   ?? Shown( Stuff( "a", 0, -1, "XY" ) )
   ?? Shown( Stuff( "a", 0, 0, "XY" ) )
   ?? Shown( Stuff( "a", 0, 1, "XY" ) )
   ?? Shown( Stuff( "a", 0, 2, "XY" ) )
   ?? Shown( Stuff( "a", 0, 5, "XY" ) )
   ?? Shown( Stuff( "a", 0, 99, "XY" ) )
   ?? Shown( Stuff( "a", 1, -1, "XY" ) )
   ?? Shown( Stuff( "a", 1, 0, "XY" ) )
   ?? Shown( Stuff( "a", 1, 1, "XY" ) )
   ?? Shown( Stuff( "a", 1, 2, "XY" ) )
   ?? Shown( Stuff( "a", 1, 5, "XY" ) )
   ?? Shown( Stuff( "a", 1, 99, "XY" ) )
   ?? Shown( Stuff( "a", 2, -1, "XY" ) )
   ?? Shown( Stuff( "a", 2, 0, "XY" ) )
   ?? Shown( Stuff( "a", 2, 1, "XY" ) )
   ?? Shown( Stuff( "a", 2, 2, "XY" ) )
   ?? Shown( Stuff( "a", 2, 5, "XY" ) )
   ?? Shown( Stuff( "a", 2, 99, "XY" ) )
   ?? Shown( Stuff( "a", 3, -1, "XY" ) )
   ?? Shown( Stuff( "a", 3, 0, "XY" ) )
   ?? Shown( Stuff( "a", 3, 1, "XY" ) )
   ?? Shown( Stuff( "a", 3, 2, "XY" ) )
   ?? Shown( Stuff( "a", 3, 5, "XY" ) )
   ?? Shown( Stuff( "a", 3, 99, "XY" ) )
   ?? Shown( Stuff( "a", 5, -1, "XY" ) )
   ?? Shown( Stuff( "a", 5, 0, "XY" ) )
   ?? Shown( Stuff( "a", 5, 1, "XY" ) )
   ?? Shown( Stuff( "a", 5, 2, "XY" ) )
   ?? Shown( Stuff( "a", 5, 5, "XY" ) )
   ?? Shown( Stuff( "a", 5, 99, "XY" ) )
   ?? Shown( Stuff( "a", 12, -1, "XY" ) )
   ?? Shown( Stuff( "a", 12, 0, "XY" ) )
   ?? Shown( Stuff( "a", 12, 1, "XY" ) )
   ?? Shown( Stuff( "a", 12, 2, "XY" ) )
   ?? Shown( Stuff( "a", 12, 5, "XY" ) )
   ?? Shown( Stuff( "a", 12, 99, "XY" ) )
   ?? Shown( Stuff( "a", 13, -1, "XY" ) )
   ?? Shown( Stuff( "a", 13, 0, "XY" ) )
   ?? Shown( Stuff( "a", 13, 1, "XY" ) )
   ?? Shown( Stuff( "a", 13, 2, "XY" ) )
   ?? Shown( Stuff( "a", 13, 5, "XY" ) )
   ?? Shown( Stuff( "a", 13, 99, "XY" ) )
   ?? Shown( Stuff( "a", 99, -1, "XY" ) )
   ?? Shown( Stuff( "a", 99, 0, "XY" ) )
   ?? Shown( Stuff( "a", 99, 1, "XY" ) )
   ?? Shown( Stuff( "a", 99, 2, "XY" ) )
   ?? Shown( Stuff( "a", 99, 5, "XY" ) )
   ?? Shown( Stuff( "a", 99, 99, "XY" ) )
   ?? Shown( Stuff( "a", 2.7, -1, "XY" ) )
   ?? Shown( Stuff( "a", 2.7, 0, "XY" ) )
   ?? Shown( Stuff( "a", 2.7, 1, "XY" ) )
   ?? Shown( Stuff( "a", 2.7, 2, "XY" ) )
   ?? Shown( Stuff( "a", 2.7, 5, "XY" ) )
   ?? Shown( Stuff( "a", 2.7, 99, "XY" ) )
   ? ""
   ?? Shown( Stuff( "abc", -10, -1, "XY" ) )
   ?? Shown( Stuff( "abc", -10, 0, "XY" ) )
   ?? Shown( Stuff( "abc", -10, 1, "XY" ) )
   ?? Shown( Stuff( "abc", -10, 2, "XY" ) )
   ?? Shown( Stuff( "abc", -10, 5, "XY" ) )
   ?? Shown( Stuff( "abc", -10, 99, "XY" ) )
   ?? Shown( Stuff( "abc", -3, -1, "XY" ) )
   ?? Shown( Stuff( "abc", -3, 0, "XY" ) )
   ?? Shown( Stuff( "abc", -3, 1, "XY" ) )
   ?? Shown( Stuff( "abc", -3, 2, "XY" ) )
   ?? Shown( Stuff( "abc", -3, 5, "XY" ) )
   ?? Shown( Stuff( "abc", -3, 99, "XY" ) )
   ?? Shown( Stuff( "abc", -1, -1, "XY" ) )
   ?? Shown( Stuff( "abc", -1, 0, "XY" ) )
   ?? Shown( Stuff( "abc", -1, 1, "XY" ) )
   ?? Shown( Stuff( "abc", -1, 2, "XY" ) )
   ?? Shown( Stuff( "abc", -1, 5, "XY" ) )
   ?? Shown( Stuff( "abc", -1, 99, "XY" ) )
   ?? Shown( Stuff( "abc", 0, -1, "XY" ) )
   ?? Shown( Stuff( "abc", 0, 0, "XY" ) )
   ?? Shown( Stuff( "abc", 0, 1, "XY" ) )
   ?? Shown( Stuff( "abc", 0, 2, "XY" ) )
   ?? Shown( Stuff( "abc", 0, 5, "XY" ) )
   ?? Shown( Stuff( "abc", 0, 99, "XY" ) )
   ?? Shown( Stuff( "abc", 1, -1, "XY" ) )
   ?? Shown( Stuff( "abc", 1, 0, "XY" ) )
   ?? Shown( Stuff( "abc", 1, 1, "XY" ) )
   ?? Shown( Stuff( "abc", 1, 2, "XY" ) )
   ?? Shown( Stuff( "abc", 1, 5, "XY" ) )
   ?? Shown( Stuff( "abc", 1, 99, "XY" ) )
   ?? Shown( Stuff( "abc", 2, -1, "XY" ) )
   ?? Shown( Stuff( "abc", 2, 0, "XY" ) )
   ?? Shown( Stuff( "abc", 2, 1, "XY" ) )
   ?? Shown( Stuff( "abc", 2, 2, "XY" ) )
   ?? Shown( Stuff( "abc", 2, 5, "XY" ) )
   ?? Shown( Stuff( "abc", 2, 99, "XY" ) )
   ?? Shown( Stuff( "abc", 3, -1, "XY" ) )
   ?? Shown( Stuff( "abc", 3, 0, "XY" ) )
   ?? Shown( Stuff( "abc", 3, 1, "XY" ) )
   ?? Shown( Stuff( "abc", 3, 2, "XY" ) )
   ?? Shown( Stuff( "abc", 3, 5, "XY" ) )
   ?? Shown( Stuff( "abc", 3, 99, "XY" ) )
   ?? Shown( Stuff( "abc", 5, -1, "XY" ) )
   ?? Shown( Stuff( "abc", 5, 0, "XY" ) )
   ?? Shown( Stuff( "abc", 5, 1, "XY" ) )
   ?? Shown( Stuff( "abc", 5, 2, "XY" ) )
   ?? Shown( Stuff( "abc", 5, 5, "XY" ) )
   ?? Shown( Stuff( "abc", 5, 99, "XY" ) )
   ?? Shown( Stuff( "abc", 12, -1, "XY" ) )
   ?? Shown( Stuff( "abc", 12, 0, "XY" ) )
   ?? Shown( Stuff( "abc", 12, 1, "XY" ) )
   ?? Shown( Stuff( "abc", 12, 2, "XY" ) )
   ?? Shown( Stuff( "abc", 12, 5, "XY" ) )
   ?? Shown( Stuff( "abc", 12, 99, "XY" ) )
   ?? Shown( Stuff( "abc", 13, -1, "XY" ) )
   ?? Shown( Stuff( "abc", 13, 0, "XY" ) )
   ?? Shown( Stuff( "abc", 13, 1, "XY" ) )
   ?? Shown( Stuff( "abc", 13, 2, "XY" ) )
   ?? Shown( Stuff( "abc", 13, 5, "XY" ) )
   ?? Shown( Stuff( "abc", 13, 99, "XY" ) )
   ?? Shown( Stuff( "abc", 99, -1, "XY" ) )
   ?? Shown( Stuff( "abc", 99, 0, "XY" ) )
   ?? Shown( Stuff( "abc", 99, 1, "XY" ) )
   ?? Shown( Stuff( "abc", 99, 2, "XY" ) )
   ?? Shown( Stuff( "abc", 99, 5, "XY" ) )
   ?? Shown( Stuff( "abc", 99, 99, "XY" ) )
   ?? Shown( Stuff( "abc", 2.7, -1, "XY" ) )
   ?? Shown( Stuff( "abc", 2.7, 0, "XY" ) )
   ?? Shown( Stuff( "abc", 2.7, 1, "XY" ) )
   ?? Shown( Stuff( "abc", 2.7, 2, "XY" ) )
   ?? Shown( Stuff( "abc", 2.7, 5, "XY" ) )
   ?? Shown( Stuff( "abc", 2.7, 99, "XY" ) )
   ? ""
   ?? Shown( Stuff( "  ab  ", -10, -1, "XY" ) )
   ?? Shown( Stuff( "  ab  ", -10, 0, "XY" ) )
   ?? Shown( Stuff( "  ab  ", -10, 1, "XY" ) )
   ?? Shown( Stuff( "  ab  ", -10, 2, "XY" ) )
   ?? Shown( Stuff( "  ab  ", -10, 5, "XY" ) )
   ?? Shown( Stuff( "  ab  ", -10, 99, "XY" ) )
   ?? Shown( Stuff( "  ab  ", -3, -1, "XY" ) )
   ?? Shown( Stuff( "  ab  ", -3, 0, "XY" ) )
   ?? Shown( Stuff( "  ab  ", -3, 1, "XY" ) )
   ?? Shown( Stuff( "  ab  ", -3, 2, "XY" ) )
   ?? Shown( Stuff( "  ab  ", -3, 5, "XY" ) )
   ?? Shown( Stuff( "  ab  ", -3, 99, "XY" ) )
   ?? Shown( Stuff( "  ab  ", -1, -1, "XY" ) )
   ?? Shown( Stuff( "  ab  ", -1, 0, "XY" ) )
   ?? Shown( Stuff( "  ab  ", -1, 1, "XY" ) )
   ?? Shown( Stuff( "  ab  ", -1, 2, "XY" ) )
   ?? Shown( Stuff( "  ab  ", -1, 5, "XY" ) )
   ?? Shown( Stuff( "  ab  ", -1, 99, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 0, -1, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 0, 0, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 0, 1, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 0, 2, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 0, 5, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 0, 99, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 1, -1, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 1, 0, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 1, 1, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 1, 2, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 1, 5, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 1, 99, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 2, -1, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 2, 0, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 2, 1, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 2, 2, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 2, 5, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 2, 99, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 3, -1, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 3, 0, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 3, 1, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 3, 2, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 3, 5, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 3, 99, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 5, -1, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 5, 0, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 5, 1, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 5, 2, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 5, 5, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 5, 99, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 12, -1, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 12, 0, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 12, 1, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 12, 2, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 12, 5, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 12, 99, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 13, -1, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 13, 0, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 13, 1, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 13, 2, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 13, 5, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 13, 99, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 99, -1, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 99, 0, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 99, 1, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 99, 2, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 99, 5, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 99, 99, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 2.7, -1, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 2.7, 0, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 2.7, 1, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 2.7, 2, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 2.7, 5, "XY" ) )
   ?? Shown( Stuff( "  ab  ", 2.7, 99, "XY" ) )
   ? ""
   ?? Shown( Stuff( "Hello, World", -10, -1, "XY" ) )
   ?? Shown( Stuff( "Hello, World", -10, 0, "XY" ) )
   ?? Shown( Stuff( "Hello, World", -10, 1, "XY" ) )
   ?? Shown( Stuff( "Hello, World", -10, 2, "XY" ) )
   ?? Shown( Stuff( "Hello, World", -10, 5, "XY" ) )
   ?? Shown( Stuff( "Hello, World", -10, 99, "XY" ) )
   ?? Shown( Stuff( "Hello, World", -3, -1, "XY" ) )
   ?? Shown( Stuff( "Hello, World", -3, 0, "XY" ) )
   ?? Shown( Stuff( "Hello, World", -3, 1, "XY" ) )
   ?? Shown( Stuff( "Hello, World", -3, 2, "XY" ) )
   ?? Shown( Stuff( "Hello, World", -3, 5, "XY" ) )
   ?? Shown( Stuff( "Hello, World", -3, 99, "XY" ) )
   ?? Shown( Stuff( "Hello, World", -1, -1, "XY" ) )
   ?? Shown( Stuff( "Hello, World", -1, 0, "XY" ) )
   ?? Shown( Stuff( "Hello, World", -1, 1, "XY" ) )
   ?? Shown( Stuff( "Hello, World", -1, 2, "XY" ) )
   ?? Shown( Stuff( "Hello, World", -1, 5, "XY" ) )
   ?? Shown( Stuff( "Hello, World", -1, 99, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 0, -1, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 0, 0, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 0, 1, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 0, 2, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 0, 5, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 0, 99, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 1, -1, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 1, 0, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 1, 1, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 1, 2, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 1, 5, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 1, 99, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 2, -1, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 2, 0, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 2, 1, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 2, 2, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 2, 5, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 2, 99, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 3, -1, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 3, 0, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 3, 1, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 3, 2, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 3, 5, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 3, 99, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 5, -1, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 5, 0, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 5, 1, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 5, 2, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 5, 5, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 5, 99, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 12, -1, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 12, 0, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 12, 1, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 12, 2, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 12, 5, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 12, 99, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 13, -1, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 13, 0, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 13, 1, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 13, 2, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 13, 5, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 13, 99, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 99, -1, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 99, 0, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 99, 1, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 99, 2, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 99, 5, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 99, 99, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 2.7, -1, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 2.7, 0, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 2.7, 1, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 2.7, 2, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 2.7, 5, "XY" ) )
   ?? Shown( Stuff( "Hello, World", 2.7, 99, "XY" ) )
   ? ""
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", -10, -1, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", -10, 0, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", -10, 1, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", -10, 2, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", -10, 5, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", -10, 99, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", -3, -1, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", -3, 0, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", -3, 1, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", -3, 2, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", -3, 5, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", -3, 99, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", -1, -1, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", -1, 0, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", -1, 1, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", -1, 2, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", -1, 5, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", -1, 99, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 0, -1, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 0, 0, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 0, 1, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 0, 2, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 0, 5, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 0, 99, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 1, -1, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 1, 0, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 1, 1, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 1, 2, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 1, 5, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 1, 99, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 2, -1, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 2, 0, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 2, 1, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 2, 2, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 2, 5, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 2, 99, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 3, -1, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 3, 0, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 3, 1, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 3, 2, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 3, 5, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 3, 99, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 5, -1, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 5, 0, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 5, 1, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 5, 2, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 5, 5, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 5, 99, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 12, -1, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 12, 0, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 12, 1, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 12, 2, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 12, 5, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 12, 99, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 13, -1, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 13, 0, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 13, 1, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 13, 2, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 13, 5, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 13, 99, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 99, -1, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 99, 0, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 99, 1, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 99, 2, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 99, 5, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 99, 99, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 2.7, -1, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 2.7, 0, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 2.7, 1, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 2.7, 2, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 2.7, 5, "XY" ) )
   ?? Shown( Stuff( "a" + Chr( 0 ) + "b", 2.7, 99, "XY" ) )
   ? ""
   ?? Shown( Stuff( Chr( 200 ) + "x", -10, -1, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", -10, 0, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", -10, 1, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", -10, 2, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", -10, 5, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", -10, 99, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", -3, -1, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", -3, 0, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", -3, 1, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", -3, 2, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", -3, 5, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", -3, 99, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", -1, -1, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", -1, 0, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", -1, 1, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", -1, 2, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", -1, 5, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", -1, 99, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 0, -1, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 0, 0, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 0, 1, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 0, 2, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 0, 5, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 0, 99, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 1, -1, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 1, 0, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 1, 1, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 1, 2, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 1, 5, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 1, 99, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 2, -1, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 2, 0, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 2, 1, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 2, 2, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 2, 5, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 2, 99, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 3, -1, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 3, 0, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 3, 1, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 3, 2, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 3, 5, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 3, 99, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 5, -1, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 5, 0, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 5, 1, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 5, 2, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 5, 5, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 5, 99, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 12, -1, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 12, 0, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 12, 1, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 12, 2, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 12, 5, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 12, 99, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 13, -1, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 13, 0, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 13, 1, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 13, 2, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 13, 5, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 13, 99, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 99, -1, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 99, 0, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 99, 1, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 99, 2, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 99, 5, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 99, 99, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 2.7, -1, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 2.7, 0, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 2.7, 1, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 2.7, 2, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 2.7, 5, "XY" ) )
   ?? Shown( Stuff( Chr( 200 ) + "x", 2.7, 99, "XY" ) )
   ? ""
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -10, -1, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -10, 0, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -10, 1, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -10, 2, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -10, 5, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -10, 99, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -3, -1, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -3, 0, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -3, 1, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -3, 2, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -3, 5, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -3, 99, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -1, -1, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -1, 0, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -1, 1, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -1, 2, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -1, 5, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), -1, 99, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 0, -1, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 0, 0, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 0, 1, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 0, 2, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 0, 5, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 0, 99, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 1, -1, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 1, 0, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 1, 1, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 1, 2, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 1, 5, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 1, 99, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 2, -1, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 2, 0, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 2, 1, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 2, 2, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 2, 5, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 2, 99, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 3, -1, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 3, 0, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 3, 1, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 3, 2, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 3, 5, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 3, 99, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 5, -1, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 5, 0, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 5, 1, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 5, 2, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 5, 5, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 5, 99, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 12, -1, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 12, 0, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 12, 1, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 12, 2, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 12, 5, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 12, 99, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 13, -1, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 13, 0, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 13, 1, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 13, 2, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 13, 5, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 13, 99, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 99, -1, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 99, 0, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 99, 1, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 99, 2, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 99, 5, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 99, 99, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 2.7, -1, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 2.7, 0, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 2.7, 1, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 2.7, 2, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 2.7, 5, "XY" ) )
   ?? Shown( Stuff( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), 2.7, 99, "XY" ) )
   ? "Stuff(snm+) runtime"
   FOR EACH v0 IN aStr
      ? ""
      FOR EACH v1 IN { -10, -3, -1, 0, 1, 2, 3, 5, 12, 13, 99, 2.7 }
         FOR EACH v2 IN { -1, 0, 1, 2, 5, 99 }
            ?? Shown( Stuff( v0, v1, v2, "XY" ) )
         NEXT
      NEXT
   NEXT
   ? "Stuff(snm+) macro"
   FOR EACH v0 IN aText
      ? ""
      FOR EACH v1 IN { "-10", "-3", "-1", "0", "1", "2", "3", "5", "12", "13", "99", "2.7" }
         FOR EACH v2 IN { "-1", "0", "1", "2", "5", "99" }
            ?? Shown( &( "Stuff( " + v0 + ", " + v1 + ", " + v2 + ", " + "'XY'" + " )" ) )
         NEXT
      NEXT
   NEXT
   ? "Stuff(snm+) input"
   FOR EACH v0 IN aIn
      ? ""
      FOR EACH v1 IN { -10, -3, -1, 0, 1, 2, 3, 5, 12, 13, 99, 2.7 }
         FOR EACH v2 IN { -1, 0, 1, 2, 5, 99 }
            ?? Shown( Stuff( v0, v1, v2, "XY" ) )
         NEXT
      NEXT
   NEXT
RETURN

STATIC PROCEDURE F10( aStr, aText, aIn )
   LOCAL v0, v1
   ? "At(ss) literal"
   ? ""
   ?? Shown( At( "", "" ) )
   ?? Shown( At( "", " " ) )
   ?? Shown( At( "", "a" ) )
   ?? Shown( At( "", "abc" ) )
   ?? Shown( At( "", "  ab  " ) )
   ?? Shown( At( "", "Hello, World" ) )
   ?? Shown( At( "", "a" + Chr( 0 ) + "b" ) )
   ?? Shown( At( "", Chr( 200 ) + "x" ) )
   ?? Shown( At( "", Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ) ) )
   ? ""
   ?? Shown( At( " ", "" ) )
   ?? Shown( At( " ", " " ) )
   ?? Shown( At( " ", "a" ) )
   ?? Shown( At( " ", "abc" ) )
   ?? Shown( At( " ", "  ab  " ) )
   ?? Shown( At( " ", "Hello, World" ) )
   ?? Shown( At( " ", "a" + Chr( 0 ) + "b" ) )
   ?? Shown( At( " ", Chr( 200 ) + "x" ) )
   ?? Shown( At( " ", Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ) ) )
   ? ""
   ?? Shown( At( "a", "" ) )
   ?? Shown( At( "a", " " ) )
   ?? Shown( At( "a", "a" ) )
   ?? Shown( At( "a", "abc" ) )
   ?? Shown( At( "a", "  ab  " ) )
   ?? Shown( At( "a", "Hello, World" ) )
   ?? Shown( At( "a", "a" + Chr( 0 ) + "b" ) )
   ?? Shown( At( "a", Chr( 200 ) + "x" ) )
   ?? Shown( At( "a", Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ) ) )
   ? ""
   ?? Shown( At( "abc", "" ) )
   ?? Shown( At( "abc", " " ) )
   ?? Shown( At( "abc", "a" ) )
   ?? Shown( At( "abc", "abc" ) )
   ?? Shown( At( "abc", "  ab  " ) )
   ?? Shown( At( "abc", "Hello, World" ) )
   ?? Shown( At( "abc", "a" + Chr( 0 ) + "b" ) )
   ?? Shown( At( "abc", Chr( 200 ) + "x" ) )
   ?? Shown( At( "abc", Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ) ) )
   ? ""
   ?? Shown( At( "  ab  ", "" ) )
   ?? Shown( At( "  ab  ", " " ) )
   ?? Shown( At( "  ab  ", "a" ) )
   ?? Shown( At( "  ab  ", "abc" ) )
   ?? Shown( At( "  ab  ", "  ab  " ) )
   ?? Shown( At( "  ab  ", "Hello, World" ) )
   ?? Shown( At( "  ab  ", "a" + Chr( 0 ) + "b" ) )
   ?? Shown( At( "  ab  ", Chr( 200 ) + "x" ) )
   ?? Shown( At( "  ab  ", Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ) ) )
   ? ""
   ?? Shown( At( "Hello, World", "" ) )
   ?? Shown( At( "Hello, World", " " ) )
   ?? Shown( At( "Hello, World", "a" ) )
   ?? Shown( At( "Hello, World", "abc" ) )
   ?? Shown( At( "Hello, World", "  ab  " ) )
   ?? Shown( At( "Hello, World", "Hello, World" ) )
   ?? Shown( At( "Hello, World", "a" + Chr( 0 ) + "b" ) )
   ?? Shown( At( "Hello, World", Chr( 200 ) + "x" ) )
   ?? Shown( At( "Hello, World", Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ) ) )
   ? ""
   ?? Shown( At( "a" + Chr( 0 ) + "b", "" ) )
   ?? Shown( At( "a" + Chr( 0 ) + "b", " " ) )
   ?? Shown( At( "a" + Chr( 0 ) + "b", "a" ) )
   ?? Shown( At( "a" + Chr( 0 ) + "b", "abc" ) )
   ?? Shown( At( "a" + Chr( 0 ) + "b", "  ab  " ) )
   ?? Shown( At( "a" + Chr( 0 ) + "b", "Hello, World" ) )
   ?? Shown( At( "a" + Chr( 0 ) + "b", "a" + Chr( 0 ) + "b" ) )
   ?? Shown( At( "a" + Chr( 0 ) + "b", Chr( 200 ) + "x" ) )
   ?? Shown( At( "a" + Chr( 0 ) + "b", Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ) ) )
   ? ""
   ?? Shown( At( Chr( 200 ) + "x", "" ) )
   ?? Shown( At( Chr( 200 ) + "x", " " ) )
   ?? Shown( At( Chr( 200 ) + "x", "a" ) )
   ?? Shown( At( Chr( 200 ) + "x", "abc" ) )
   ?? Shown( At( Chr( 200 ) + "x", "  ab  " ) )
   ?? Shown( At( Chr( 200 ) + "x", "Hello, World" ) )
   ?? Shown( At( Chr( 200 ) + "x", "a" + Chr( 0 ) + "b" ) )
   ?? Shown( At( Chr( 200 ) + "x", Chr( 200 ) + "x" ) )
   ?? Shown( At( Chr( 200 ) + "x", Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ) ) )
   ? ""
   ?? Shown( At( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), "" ) )
   ?? Shown( At( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), " " ) )
   ?? Shown( At( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), "a" ) )
   ?? Shown( At( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), "abc" ) )
   ?? Shown( At( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), "  ab  " ) )
   ?? Shown( At( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), "Hello, World" ) )
   ?? Shown( At( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), "a" + Chr( 0 ) + "b" ) )
   ?? Shown( At( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), Chr( 200 ) + "x" ) )
   ?? Shown( At( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ) ) )
   ? "At(ss) runtime"
   FOR EACH v0 IN aStr
      ? ""
      FOR EACH v1 IN aStr
         ?? Shown( At( v0, v1 ) )
      NEXT
   NEXT
   ? "At(ss) macro"
   FOR EACH v0 IN aText
      ? ""
      FOR EACH v1 IN aText
         ?? Shown( &( "At( " + v0 + ", " + v1 + " )" ) )
      NEXT
   NEXT
   ? "At(ss) input"
   FOR EACH v0 IN aIn
      ? ""
      FOR EACH v1 IN aIn
         ?? Shown( At( v0, v1 ) )
      NEXT
   NEXT
RETURN

STATIC PROCEDURE F11( aStr, aText, aIn )
   LOCAL v0, v1
   ? "RAt(ss) literal"
   ? ""
   ?? Shown( RAt( "", "" ) )
   ?? Shown( RAt( "", " " ) )
   ?? Shown( RAt( "", "a" ) )
   ?? Shown( RAt( "", "abc" ) )
   ?? Shown( RAt( "", "  ab  " ) )
   ?? Shown( RAt( "", "Hello, World" ) )
   ?? Shown( RAt( "", "a" + Chr( 0 ) + "b" ) )
   ?? Shown( RAt( "", Chr( 200 ) + "x" ) )
   ?? Shown( RAt( "", Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ) ) )
   ? ""
   ?? Shown( RAt( " ", "" ) )
   ?? Shown( RAt( " ", " " ) )
   ?? Shown( RAt( " ", "a" ) )
   ?? Shown( RAt( " ", "abc" ) )
   ?? Shown( RAt( " ", "  ab  " ) )
   ?? Shown( RAt( " ", "Hello, World" ) )
   ?? Shown( RAt( " ", "a" + Chr( 0 ) + "b" ) )
   ?? Shown( RAt( " ", Chr( 200 ) + "x" ) )
   ?? Shown( RAt( " ", Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ) ) )
   ? ""
   ?? Shown( RAt( "a", "" ) )
   ?? Shown( RAt( "a", " " ) )
   ?? Shown( RAt( "a", "a" ) )
   ?? Shown( RAt( "a", "abc" ) )
   ?? Shown( RAt( "a", "  ab  " ) )
   ?? Shown( RAt( "a", "Hello, World" ) )
   ?? Shown( RAt( "a", "a" + Chr( 0 ) + "b" ) )
   ?? Shown( RAt( "a", Chr( 200 ) + "x" ) )
   ?? Shown( RAt( "a", Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ) ) )
   ? ""
   ?? Shown( RAt( "abc", "" ) )
   ?? Shown( RAt( "abc", " " ) )
   ?? Shown( RAt( "abc", "a" ) )
   ?? Shown( RAt( "abc", "abc" ) )
   ?? Shown( RAt( "abc", "  ab  " ) )
   ?? Shown( RAt( "abc", "Hello, World" ) )
   ?? Shown( RAt( "abc", "a" + Chr( 0 ) + "b" ) )
   ?? Shown( RAt( "abc", Chr( 200 ) + "x" ) )
   ?? Shown( RAt( "abc", Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ) ) )
   ? ""
   ?? Shown( RAt( "  ab  ", "" ) )
   ?? Shown( RAt( "  ab  ", " " ) )
   ?? Shown( RAt( "  ab  ", "a" ) )
   ?? Shown( RAt( "  ab  ", "abc" ) )
   ?? Shown( RAt( "  ab  ", "  ab  " ) )
   ?? Shown( RAt( "  ab  ", "Hello, World" ) )
   ?? Shown( RAt( "  ab  ", "a" + Chr( 0 ) + "b" ) )
   ?? Shown( RAt( "  ab  ", Chr( 200 ) + "x" ) )
   ?? Shown( RAt( "  ab  ", Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ) ) )
   ? ""
   ?? Shown( RAt( "Hello, World", "" ) )
   ?? Shown( RAt( "Hello, World", " " ) )
   ?? Shown( RAt( "Hello, World", "a" ) )
   ?? Shown( RAt( "Hello, World", "abc" ) )
   ?? Shown( RAt( "Hello, World", "  ab  " ) )
   ?? Shown( RAt( "Hello, World", "Hello, World" ) )
   ?? Shown( RAt( "Hello, World", "a" + Chr( 0 ) + "b" ) )
   ?? Shown( RAt( "Hello, World", Chr( 200 ) + "x" ) )
   ?? Shown( RAt( "Hello, World", Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ) ) )
   ? ""
   ?? Shown( RAt( "a" + Chr( 0 ) + "b", "" ) )
   ?? Shown( RAt( "a" + Chr( 0 ) + "b", " " ) )
   ?? Shown( RAt( "a" + Chr( 0 ) + "b", "a" ) )
   ?? Shown( RAt( "a" + Chr( 0 ) + "b", "abc" ) )
   ?? Shown( RAt( "a" + Chr( 0 ) + "b", "  ab  " ) )
   ?? Shown( RAt( "a" + Chr( 0 ) + "b", "Hello, World" ) )
   ?? Shown( RAt( "a" + Chr( 0 ) + "b", "a" + Chr( 0 ) + "b" ) )
   ?? Shown( RAt( "a" + Chr( 0 ) + "b", Chr( 200 ) + "x" ) )
   ?? Shown( RAt( "a" + Chr( 0 ) + "b", Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ) ) )
   ? ""
   ?? Shown( RAt( Chr( 200 ) + "x", "" ) )
   ?? Shown( RAt( Chr( 200 ) + "x", " " ) )
   ?? Shown( RAt( Chr( 200 ) + "x", "a" ) )
   ?? Shown( RAt( Chr( 200 ) + "x", "abc" ) )
   ?? Shown( RAt( Chr( 200 ) + "x", "  ab  " ) )
   ?? Shown( RAt( Chr( 200 ) + "x", "Hello, World" ) )
   ?? Shown( RAt( Chr( 200 ) + "x", "a" + Chr( 0 ) + "b" ) )
   ?? Shown( RAt( Chr( 200 ) + "x", Chr( 200 ) + "x" ) )
   ?? Shown( RAt( Chr( 200 ) + "x", Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ) ) )
   ? ""
   ?? Shown( RAt( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), "" ) )
   ?? Shown( RAt( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), " " ) )
   ?? Shown( RAt( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), "a" ) )
   ?? Shown( RAt( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), "abc" ) )
   ?? Shown( RAt( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), "  ab  " ) )
   ?? Shown( RAt( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), "Hello, World" ) )
   ?? Shown( RAt( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), "a" + Chr( 0 ) + "b" ) )
   ?? Shown( RAt( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), Chr( 200 ) + "x" ) )
   ?? Shown( RAt( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ) ) )
   ? "RAt(ss) runtime"
   FOR EACH v0 IN aStr
      ? ""
      FOR EACH v1 IN aStr
         ?? Shown( RAt( v0, v1 ) )
      NEXT
   NEXT
   ? "RAt(ss) macro"
   FOR EACH v0 IN aText
      ? ""
      FOR EACH v1 IN aText
         ?? Shown( &( "RAt( " + v0 + ", " + v1 + " )" ) )
      NEXT
   NEXT
   ? "RAt(ss) input"
   FOR EACH v0 IN aIn
      ? ""
      FOR EACH v1 IN aIn
         ?? Shown( RAt( v0, v1 ) )
      NEXT
   NEXT
RETURN

STATIC PROCEDURE F12( aStr, aText, aIn )
   LOCAL v0, v1
   ? "StrTran(ss) literal"
   ? ""
   ?? Shown( StrTran( "", "" ) )
   ?? Shown( StrTran( "", " " ) )
   ?? Shown( StrTran( "", "a" ) )
   ?? Shown( StrTran( "", "abc" ) )
   ?? Shown( StrTran( "", "  ab  " ) )
   ?? Shown( StrTran( "", "Hello, World" ) )
   ?? Shown( StrTran( "", "a" + Chr( 0 ) + "b" ) )
   ?? Shown( StrTran( "", Chr( 200 ) + "x" ) )
   ?? Shown( StrTran( "", Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ) ) )
   ? ""
   ?? Shown( StrTran( " ", "" ) )
   ?? Shown( StrTran( " ", " " ) )
   ?? Shown( StrTran( " ", "a" ) )
   ?? Shown( StrTran( " ", "abc" ) )
   ?? Shown( StrTran( " ", "  ab  " ) )
   ?? Shown( StrTran( " ", "Hello, World" ) )
   ?? Shown( StrTran( " ", "a" + Chr( 0 ) + "b" ) )
   ?? Shown( StrTran( " ", Chr( 200 ) + "x" ) )
   ?? Shown( StrTran( " ", Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ) ) )
   ? ""
   ?? Shown( StrTran( "a", "" ) )
   ?? Shown( StrTran( "a", " " ) )
   ?? Shown( StrTran( "a", "a" ) )
   ?? Shown( StrTran( "a", "abc" ) )
   ?? Shown( StrTran( "a", "  ab  " ) )
   ?? Shown( StrTran( "a", "Hello, World" ) )
   ?? Shown( StrTran( "a", "a" + Chr( 0 ) + "b" ) )
   ?? Shown( StrTran( "a", Chr( 200 ) + "x" ) )
   ?? Shown( StrTran( "a", Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ) ) )
   ? ""
   ?? Shown( StrTran( "abc", "" ) )
   ?? Shown( StrTran( "abc", " " ) )
   ?? Shown( StrTran( "abc", "a" ) )
   ?? Shown( StrTran( "abc", "abc" ) )
   ?? Shown( StrTran( "abc", "  ab  " ) )
   ?? Shown( StrTran( "abc", "Hello, World" ) )
   ?? Shown( StrTran( "abc", "a" + Chr( 0 ) + "b" ) )
   ?? Shown( StrTran( "abc", Chr( 200 ) + "x" ) )
   ?? Shown( StrTran( "abc", Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ) ) )
   ? ""
   ?? Shown( StrTran( "  ab  ", "" ) )
   ?? Shown( StrTran( "  ab  ", " " ) )
   ?? Shown( StrTran( "  ab  ", "a" ) )
   ?? Shown( StrTran( "  ab  ", "abc" ) )
   ?? Shown( StrTran( "  ab  ", "  ab  " ) )
   ?? Shown( StrTran( "  ab  ", "Hello, World" ) )
   ?? Shown( StrTran( "  ab  ", "a" + Chr( 0 ) + "b" ) )
   ?? Shown( StrTran( "  ab  ", Chr( 200 ) + "x" ) )
   ?? Shown( StrTran( "  ab  ", Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ) ) )
   ? ""
   ?? Shown( StrTran( "Hello, World", "" ) )
   ?? Shown( StrTran( "Hello, World", " " ) )
   ?? Shown( StrTran( "Hello, World", "a" ) )
   ?? Shown( StrTran( "Hello, World", "abc" ) )
   ?? Shown( StrTran( "Hello, World", "  ab  " ) )
   ?? Shown( StrTran( "Hello, World", "Hello, World" ) )
   ?? Shown( StrTran( "Hello, World", "a" + Chr( 0 ) + "b" ) )
   ?? Shown( StrTran( "Hello, World", Chr( 200 ) + "x" ) )
   ?? Shown( StrTran( "Hello, World", Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ) ) )
   ? ""
   ?? Shown( StrTran( "a" + Chr( 0 ) + "b", "" ) )
   ?? Shown( StrTran( "a" + Chr( 0 ) + "b", " " ) )
   ?? Shown( StrTran( "a" + Chr( 0 ) + "b", "a" ) )
   ?? Shown( StrTran( "a" + Chr( 0 ) + "b", "abc" ) )
   ?? Shown( StrTran( "a" + Chr( 0 ) + "b", "  ab  " ) )
   ?? Shown( StrTran( "a" + Chr( 0 ) + "b", "Hello, World" ) )
   ?? Shown( StrTran( "a" + Chr( 0 ) + "b", "a" + Chr( 0 ) + "b" ) )
   ?? Shown( StrTran( "a" + Chr( 0 ) + "b", Chr( 200 ) + "x" ) )
   ?? Shown( StrTran( "a" + Chr( 0 ) + "b", Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ) ) )
   ? ""
   ?? Shown( StrTran( Chr( 200 ) + "x", "" ) )
   ?? Shown( StrTran( Chr( 200 ) + "x", " " ) )
   ?? Shown( StrTran( Chr( 200 ) + "x", "a" ) )
   ?? Shown( StrTran( Chr( 200 ) + "x", "abc" ) )
   ?? Shown( StrTran( Chr( 200 ) + "x", "  ab  " ) )
   ?? Shown( StrTran( Chr( 200 ) + "x", "Hello, World" ) )
   ?? Shown( StrTran( Chr( 200 ) + "x", "a" + Chr( 0 ) + "b" ) )
   ?? Shown( StrTran( Chr( 200 ) + "x", Chr( 200 ) + "x" ) )
   ?? Shown( StrTran( Chr( 200 ) + "x", Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ) ) )
   ? ""
   ?? Shown( StrTran( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), "" ) )
   ?? Shown( StrTran( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), " " ) )
   ?? Shown( StrTran( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), "a" ) )
   ?? Shown( StrTran( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), "abc" ) )
   ?? Shown( StrTran( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), "  ab  " ) )
   ?? Shown( StrTran( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), "Hello, World" ) )
   ?? Shown( StrTran( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), "a" + Chr( 0 ) + "b" ) )
   ?? Shown( StrTran( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), Chr( 200 ) + "x" ) )
   ?? Shown( StrTran( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ) ) )
   ? "StrTran(ss) runtime"
   FOR EACH v0 IN aStr
      ? ""
      FOR EACH v1 IN aStr
         ?? Shown( StrTran( v0, v1 ) )
      NEXT
   NEXT
   ? "StrTran(ss) macro"
   FOR EACH v0 IN aText
      ? ""
      FOR EACH v1 IN aText
         ?? Shown( &( "StrTran( " + v0 + ", " + v1 + " )" ) )
      NEXT
   NEXT
   ? "StrTran(ss) input"
   FOR EACH v0 IN aIn
      ? ""
      FOR EACH v1 IN aIn
         ?? Shown( StrTran( v0, v1 ) )
      NEXT
   NEXT
RETURN

STATIC PROCEDURE F13( aStr, aText, aIn )
   LOCAL v0, v1
   ? "StrTran(ss+) literal"
   ? ""
   ?? Shown( StrTran( "", "", "XY" ) )
   ?? Shown( StrTran( "", " ", "XY" ) )
   ?? Shown( StrTran( "", "a", "XY" ) )
   ?? Shown( StrTran( "", "abc", "XY" ) )
   ?? Shown( StrTran( "", "  ab  ", "XY" ) )
   ?? Shown( StrTran( "", "Hello, World", "XY" ) )
   ?? Shown( StrTran( "", "a" + Chr( 0 ) + "b", "XY" ) )
   ?? Shown( StrTran( "", Chr( 200 ) + "x", "XY" ) )
   ?? Shown( StrTran( "", Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), "XY" ) )
   ? ""
   ?? Shown( StrTran( " ", "", "XY" ) )
   ?? Shown( StrTran( " ", " ", "XY" ) )
   ?? Shown( StrTran( " ", "a", "XY" ) )
   ?? Shown( StrTran( " ", "abc", "XY" ) )
   ?? Shown( StrTran( " ", "  ab  ", "XY" ) )
   ?? Shown( StrTran( " ", "Hello, World", "XY" ) )
   ?? Shown( StrTran( " ", "a" + Chr( 0 ) + "b", "XY" ) )
   ?? Shown( StrTran( " ", Chr( 200 ) + "x", "XY" ) )
   ?? Shown( StrTran( " ", Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), "XY" ) )
   ? ""
   ?? Shown( StrTran( "a", "", "XY" ) )
   ?? Shown( StrTran( "a", " ", "XY" ) )
   ?? Shown( StrTran( "a", "a", "XY" ) )
   ?? Shown( StrTran( "a", "abc", "XY" ) )
   ?? Shown( StrTran( "a", "  ab  ", "XY" ) )
   ?? Shown( StrTran( "a", "Hello, World", "XY" ) )
   ?? Shown( StrTran( "a", "a" + Chr( 0 ) + "b", "XY" ) )
   ?? Shown( StrTran( "a", Chr( 200 ) + "x", "XY" ) )
   ?? Shown( StrTran( "a", Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), "XY" ) )
   ? ""
   ?? Shown( StrTran( "abc", "", "XY" ) )
   ?? Shown( StrTran( "abc", " ", "XY" ) )
   ?? Shown( StrTran( "abc", "a", "XY" ) )
   ?? Shown( StrTran( "abc", "abc", "XY" ) )
   ?? Shown( StrTran( "abc", "  ab  ", "XY" ) )
   ?? Shown( StrTran( "abc", "Hello, World", "XY" ) )
   ?? Shown( StrTran( "abc", "a" + Chr( 0 ) + "b", "XY" ) )
   ?? Shown( StrTran( "abc", Chr( 200 ) + "x", "XY" ) )
   ?? Shown( StrTran( "abc", Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), "XY" ) )
   ? ""
   ?? Shown( StrTran( "  ab  ", "", "XY" ) )
   ?? Shown( StrTran( "  ab  ", " ", "XY" ) )
   ?? Shown( StrTran( "  ab  ", "a", "XY" ) )
   ?? Shown( StrTran( "  ab  ", "abc", "XY" ) )
   ?? Shown( StrTran( "  ab  ", "  ab  ", "XY" ) )
   ?? Shown( StrTran( "  ab  ", "Hello, World", "XY" ) )
   ?? Shown( StrTran( "  ab  ", "a" + Chr( 0 ) + "b", "XY" ) )
   ?? Shown( StrTran( "  ab  ", Chr( 200 ) + "x", "XY" ) )
   ?? Shown( StrTran( "  ab  ", Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), "XY" ) )
   ? ""
   ?? Shown( StrTran( "Hello, World", "", "XY" ) )
   ?? Shown( StrTran( "Hello, World", " ", "XY" ) )
   ?? Shown( StrTran( "Hello, World", "a", "XY" ) )
   ?? Shown( StrTran( "Hello, World", "abc", "XY" ) )
   ?? Shown( StrTran( "Hello, World", "  ab  ", "XY" ) )
   ?? Shown( StrTran( "Hello, World", "Hello, World", "XY" ) )
   ?? Shown( StrTran( "Hello, World", "a" + Chr( 0 ) + "b", "XY" ) )
   ?? Shown( StrTran( "Hello, World", Chr( 200 ) + "x", "XY" ) )
   ?? Shown( StrTran( "Hello, World", Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), "XY" ) )
   ? ""
   ?? Shown( StrTran( "a" + Chr( 0 ) + "b", "", "XY" ) )
   ?? Shown( StrTran( "a" + Chr( 0 ) + "b", " ", "XY" ) )
   ?? Shown( StrTran( "a" + Chr( 0 ) + "b", "a", "XY" ) )
   ?? Shown( StrTran( "a" + Chr( 0 ) + "b", "abc", "XY" ) )
   ?? Shown( StrTran( "a" + Chr( 0 ) + "b", "  ab  ", "XY" ) )
   ?? Shown( StrTran( "a" + Chr( 0 ) + "b", "Hello, World", "XY" ) )
   ?? Shown( StrTran( "a" + Chr( 0 ) + "b", "a" + Chr( 0 ) + "b", "XY" ) )
   ?? Shown( StrTran( "a" + Chr( 0 ) + "b", Chr( 200 ) + "x", "XY" ) )
   ?? Shown( StrTran( "a" + Chr( 0 ) + "b", Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), "XY" ) )
   ? ""
   ?? Shown( StrTran( Chr( 200 ) + "x", "", "XY" ) )
   ?? Shown( StrTran( Chr( 200 ) + "x", " ", "XY" ) )
   ?? Shown( StrTran( Chr( 200 ) + "x", "a", "XY" ) )
   ?? Shown( StrTran( Chr( 200 ) + "x", "abc", "XY" ) )
   ?? Shown( StrTran( Chr( 200 ) + "x", "  ab  ", "XY" ) )
   ?? Shown( StrTran( Chr( 200 ) + "x", "Hello, World", "XY" ) )
   ?? Shown( StrTran( Chr( 200 ) + "x", "a" + Chr( 0 ) + "b", "XY" ) )
   ?? Shown( StrTran( Chr( 200 ) + "x", Chr( 200 ) + "x", "XY" ) )
   ?? Shown( StrTran( Chr( 200 ) + "x", Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), "XY" ) )
   ? ""
   ?? Shown( StrTran( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), "", "XY" ) )
   ?? Shown( StrTran( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), " ", "XY" ) )
   ?? Shown( StrTran( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), "a", "XY" ) )
   ?? Shown( StrTran( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), "abc", "XY" ) )
   ?? Shown( StrTran( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), "  ab  ", "XY" ) )
   ?? Shown( StrTran( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), "Hello, World", "XY" ) )
   ?? Shown( StrTran( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), "a" + Chr( 0 ) + "b", "XY" ) )
   ?? Shown( StrTran( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), Chr( 200 ) + "x", "XY" ) )
   ?? Shown( StrTran( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ), "XY" ) )
   ? "StrTran(ss+) runtime"
   FOR EACH v0 IN aStr
      ? ""
      FOR EACH v1 IN aStr
         ?? Shown( StrTran( v0, v1, "XY" ) )
      NEXT
   NEXT
   ? "StrTran(ss+) macro"
   FOR EACH v0 IN aText
      ? ""
      FOR EACH v1 IN aText
         ?? Shown( &( "StrTran( " + v0 + ", " + v1 + ", " + "'XY'" + " )" ) )
      NEXT
   NEXT
   ? "StrTran(ss+) input"
   FOR EACH v0 IN aIn
      ? ""
      FOR EACH v1 IN aIn
         ?? Shown( StrTran( v0, v1, "XY" ) )
      NEXT
   NEXT
RETURN

STATIC PROCEDURE F14( aStr, aText, aIn )
   LOCAL v0
   ? "AllTrim(s) literal"
   ? ""
   ?? Shown( AllTrim( "" ) )
   ? ""
   ?? Shown( AllTrim( " " ) )
   ? ""
   ?? Shown( AllTrim( "a" ) )
   ? ""
   ?? Shown( AllTrim( "abc" ) )
   ? ""
   ?? Shown( AllTrim( "  ab  " ) )
   ? ""
   ?? Shown( AllTrim( "Hello, World" ) )
   ? ""
   ?? Shown( AllTrim( "a" + Chr( 0 ) + "b" ) )
   ? ""
   ?? Shown( AllTrim( Chr( 200 ) + "x" ) )
   ? ""
   ?? Shown( AllTrim( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ) ) )
   ? "AllTrim(s) runtime"
   FOR EACH v0 IN aStr
      ? ""
      ?? Shown( AllTrim( v0 ) )
   NEXT
   ? "AllTrim(s) macro"
   FOR EACH v0 IN aText
      ? ""
      ?? Shown( &( "AllTrim( " + v0 + " )" ) )
   NEXT
   ? "AllTrim(s) input"
   FOR EACH v0 IN aIn
      ? ""
      ?? Shown( AllTrim( v0 ) )
   NEXT
RETURN

STATIC PROCEDURE F15( aStr, aText, aIn )
   LOCAL v0
   ? "LTrim(s) literal"
   ? ""
   ?? Shown( LTrim( "" ) )
   ? ""
   ?? Shown( LTrim( " " ) )
   ? ""
   ?? Shown( LTrim( "a" ) )
   ? ""
   ?? Shown( LTrim( "abc" ) )
   ? ""
   ?? Shown( LTrim( "  ab  " ) )
   ? ""
   ?? Shown( LTrim( "Hello, World" ) )
   ? ""
   ?? Shown( LTrim( "a" + Chr( 0 ) + "b" ) )
   ? ""
   ?? Shown( LTrim( Chr( 200 ) + "x" ) )
   ? ""
   ?? Shown( LTrim( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ) ) )
   ? "LTrim(s) runtime"
   FOR EACH v0 IN aStr
      ? ""
      ?? Shown( LTrim( v0 ) )
   NEXT
   ? "LTrim(s) macro"
   FOR EACH v0 IN aText
      ? ""
      ?? Shown( &( "LTrim( " + v0 + " )" ) )
   NEXT
   ? "LTrim(s) input"
   FOR EACH v0 IN aIn
      ? ""
      ?? Shown( LTrim( v0 ) )
   NEXT
RETURN

STATIC PROCEDURE F16( aStr, aText, aIn )
   LOCAL v0
   ? "RTrim(s) literal"
   ? ""
   ?? Shown( RTrim( "" ) )
   ? ""
   ?? Shown( RTrim( " " ) )
   ? ""
   ?? Shown( RTrim( "a" ) )
   ? ""
   ?? Shown( RTrim( "abc" ) )
   ? ""
   ?? Shown( RTrim( "  ab  " ) )
   ? ""
   ?? Shown( RTrim( "Hello, World" ) )
   ? ""
   ?? Shown( RTrim( "a" + Chr( 0 ) + "b" ) )
   ? ""
   ?? Shown( RTrim( Chr( 200 ) + "x" ) )
   ? ""
   ?? Shown( RTrim( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ) ) )
   ? "RTrim(s) runtime"
   FOR EACH v0 IN aStr
      ? ""
      ?? Shown( RTrim( v0 ) )
   NEXT
   ? "RTrim(s) macro"
   FOR EACH v0 IN aText
      ? ""
      ?? Shown( &( "RTrim( " + v0 + " )" ) )
   NEXT
   ? "RTrim(s) input"
   FOR EACH v0 IN aIn
      ? ""
      ?? Shown( RTrim( v0 ) )
   NEXT
RETURN

STATIC PROCEDURE F17( aStr, aText, aIn )
   LOCAL v0
   ? "Upper(s) literal"
   ? ""
   ?? Shown( Upper( "" ) )
   ? ""
   ?? Shown( Upper( " " ) )
   ? ""
   ?? Shown( Upper( "a" ) )
   ? ""
   ?? Shown( Upper( "abc" ) )
   ? ""
   ?? Shown( Upper( "  ab  " ) )
   ? ""
   ?? Shown( Upper( "Hello, World" ) )
   ? ""
   ?? Shown( Upper( "a" + Chr( 0 ) + "b" ) )
   ? ""
   ?? Shown( Upper( Chr( 200 ) + "x" ) )
   ? ""
   ?? Shown( Upper( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ) ) )
   ? "Upper(s) runtime"
   FOR EACH v0 IN aStr
      ? ""
      ?? Shown( Upper( v0 ) )
   NEXT
   ? "Upper(s) macro"
   FOR EACH v0 IN aText
      ? ""
      ?? Shown( &( "Upper( " + v0 + " )" ) )
   NEXT
   ? "Upper(s) input"
   FOR EACH v0 IN aIn
      ? ""
      ?? Shown( Upper( v0 ) )
   NEXT
RETURN

STATIC PROCEDURE F18( aStr, aText, aIn )
   LOCAL v0
   ? "Lower(s) literal"
   ? ""
   ?? Shown( Lower( "" ) )
   ? ""
   ?? Shown( Lower( " " ) )
   ? ""
   ?? Shown( Lower( "a" ) )
   ? ""
   ?? Shown( Lower( "abc" ) )
   ? ""
   ?? Shown( Lower( "  ab  " ) )
   ? ""
   ?? Shown( Lower( "Hello, World" ) )
   ? ""
   ?? Shown( Lower( "a" + Chr( 0 ) + "b" ) )
   ? ""
   ?? Shown( Lower( Chr( 200 ) + "x" ) )
   ? ""
   ?? Shown( Lower( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ) ) )
   ? "Lower(s) runtime"
   FOR EACH v0 IN aStr
      ? ""
      ?? Shown( Lower( v0 ) )
   NEXT
   ? "Lower(s) macro"
   FOR EACH v0 IN aText
      ? ""
      ?? Shown( &( "Lower( " + v0 + " )" ) )
   NEXT
   ? "Lower(s) input"
   FOR EACH v0 IN aIn
      ? ""
      ?? Shown( Lower( v0 ) )
   NEXT
RETURN

STATIC PROCEDURE F19( aStr, aText, aIn )
   LOCAL v0
   ? "Len(s) literal"
   ? ""
   ?? Shown( Len( "" ) )
   ? ""
   ?? Shown( Len( " " ) )
   ? ""
   ?? Shown( Len( "a" ) )
   ? ""
   ?? Shown( Len( "abc" ) )
   ? ""
   ?? Shown( Len( "  ab  " ) )
   ? ""
   ?? Shown( Len( "Hello, World" ) )
   ? ""
   ?? Shown( Len( "a" + Chr( 0 ) + "b" ) )
   ? ""
   ?? Shown( Len( Chr( 200 ) + "x" ) )
   ? ""
   ?? Shown( Len( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ) ) )
   ? "Len(s) runtime"
   FOR EACH v0 IN aStr
      ? ""
      ?? Shown( Len( v0 ) )
   NEXT
   ? "Len(s) macro"
   FOR EACH v0 IN aText
      ? ""
      ?? Shown( &( "Len( " + v0 + " )" ) )
   NEXT
   ? "Len(s) input"
   FOR EACH v0 IN aIn
      ? ""
      ?? Shown( Len( v0 ) )
   NEXT
RETURN

STATIC PROCEDURE F20( aStr, aText, aIn )
   LOCAL v0
   ? "Asc(s) literal"
   ? ""
   ?? Shown( Asc( "" ) )
   ? ""
   ?? Shown( Asc( " " ) )
   ? ""
   ?? Shown( Asc( "a" ) )
   ? ""
   ?? Shown( Asc( "abc" ) )
   ? ""
   ?? Shown( Asc( "  ab  " ) )
   ? ""
   ?? Shown( Asc( "Hello, World" ) )
   ? ""
   ?? Shown( Asc( "a" + Chr( 0 ) + "b" ) )
   ? ""
   ?? Shown( Asc( Chr( 200 ) + "x" ) )
   ? ""
   ?? Shown( Asc( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ) ) )
   ? "Asc(s) runtime"
   FOR EACH v0 IN aStr
      ? ""
      ?? Shown( Asc( v0 ) )
   NEXT
   ? "Asc(s) macro"
   FOR EACH v0 IN aText
      ? ""
      ?? Shown( &( "Asc( " + v0 + " )" ) )
   NEXT
   ? "Asc(s) input"
   FOR EACH v0 IN aIn
      ? ""
      ?? Shown( Asc( v0 ) )
   NEXT
RETURN

STATIC PROCEDURE F21( aStr, aText, aIn )
   LOCAL v0
   ? "Space(m) literal"
   ? ""
   ?? Shown( Space( -1 ) )
   ? ""
   ?? Shown( Space( 0 ) )
   ? ""
   ?? Shown( Space( 1 ) )
   ? ""
   ?? Shown( Space( 2 ) )
   ? ""
   ?? Shown( Space( 5 ) )
   ? ""
   ?? Shown( Space( 99 ) )
   ? "Space(m) runtime"
   FOR EACH v0 IN { -1, 0, 1, 2, 5, 99 }
      ? ""
      ?? Shown( Space( v0 ) )
   NEXT
   ? "Space(m) macro"
   FOR EACH v0 IN { "-1", "0", "1", "2", "5", "99" }
      ? ""
      ?? Shown( &( "Space( " + v0 + " )" ) )
   NEXT
RETURN

STATIC PROCEDURE F22( aStr, aText, aIn )
   LOCAL v0
   ? "IsAlpha(s) literal"
   ? ""
   ?? Shown( IsAlpha( "" ) )
   ? ""
   ?? Shown( IsAlpha( " " ) )
   ? ""
   ?? Shown( IsAlpha( "a" ) )
   ? ""
   ?? Shown( IsAlpha( "abc" ) )
   ? ""
   ?? Shown( IsAlpha( "  ab  " ) )
   ? ""
   ?? Shown( IsAlpha( "Hello, World" ) )
   ? ""
   ?? Shown( IsAlpha( "a" + Chr( 0 ) + "b" ) )
   ? ""
   ?? Shown( IsAlpha( Chr( 200 ) + "x" ) )
   ? ""
   ?? Shown( IsAlpha( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ) ) )
   ? "IsAlpha(s) runtime"
   FOR EACH v0 IN aStr
      ? ""
      ?? Shown( IsAlpha( v0 ) )
   NEXT
   ? "IsAlpha(s) macro"
   FOR EACH v0 IN aText
      ? ""
      ?? Shown( &( "IsAlpha( " + v0 + " )" ) )
   NEXT
   ? "IsAlpha(s) input"
   FOR EACH v0 IN aIn
      ? ""
      ?? Shown( IsAlpha( v0 ) )
   NEXT
RETURN

STATIC PROCEDURE F23( aStr, aText, aIn )
   LOCAL v0
   ? "IsDigit(s) literal"
   ? ""
   ?? Shown( IsDigit( "" ) )
   ? ""
   ?? Shown( IsDigit( " " ) )
   ? ""
   ?? Shown( IsDigit( "a" ) )
   ? ""
   ?? Shown( IsDigit( "abc" ) )
   ? ""
   ?? Shown( IsDigit( "  ab  " ) )
   ? ""
   ?? Shown( IsDigit( "Hello, World" ) )
   ? ""
   ?? Shown( IsDigit( "a" + Chr( 0 ) + "b" ) )
   ? ""
   ?? Shown( IsDigit( Chr( 200 ) + "x" ) )
   ? ""
   ?? Shown( IsDigit( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ) ) )
   ? "IsDigit(s) runtime"
   FOR EACH v0 IN aStr
      ? ""
      ?? Shown( IsDigit( v0 ) )
   NEXT
   ? "IsDigit(s) macro"
   FOR EACH v0 IN aText
      ? ""
      ?? Shown( &( "IsDigit( " + v0 + " )" ) )
   NEXT
   ? "IsDigit(s) input"
   FOR EACH v0 IN aIn
      ? ""
      ?? Shown( IsDigit( v0 ) )
   NEXT
RETURN

STATIC PROCEDURE F24( aStr, aText, aIn )
   LOCAL v0
   ? "Empty(s) literal"
   ? ""
   ?? Shown( Empty( "" ) )
   ? ""
   ?? Shown( Empty( " " ) )
   ? ""
   ?? Shown( Empty( "a" ) )
   ? ""
   ?? Shown( Empty( "abc" ) )
   ? ""
   ?? Shown( Empty( "  ab  " ) )
   ? ""
   ?? Shown( Empty( "Hello, World" ) )
   ? ""
   ?? Shown( Empty( "a" + Chr( 0 ) + "b" ) )
   ? ""
   ?? Shown( Empty( Chr( 200 ) + "x" ) )
   ? ""
   ?? Shown( Empty( Chr( 9 ) + "t" + Chr( 13 ) + Chr( 10 ) ) )
   ? "Empty(s) runtime"
   FOR EACH v0 IN aStr
      ? ""
      ?? Shown( Empty( v0 ) )
   NEXT
   ? "Empty(s) macro"
   FOR EACH v0 IN aText
      ? ""
      ?? Shown( &( "Empty( " + v0 + " )" ) )
   NEXT
   ? "Empty(s) input"
   FOR EACH v0 IN aIn
      ? ""
      ?? Shown( Empty( v0 ) )
   NEXT
RETURN

// The type and text of a value, bytes outside 32-126 as <n>.
STATIC FUNCTION Shown( x )
   LOCAL c := "", i, n
   IF ValType( x ) != "C"
      RETURN " " + ValType( x ) + ":" + iif( ValType( x ) == "N", LTrim( Str( x ) ), iif( x, "T", "F" ) )
   ENDIF
   FOR i := 1 TO Len( x )
      n := Asc( SubStr( x, i, 1 ) )
      c += iif( n < 32 .OR. n > 126, "<" + LTrim( Str( n ) ) + ">", Chr( n ) )
   NEXT
RETURN " [" + c + "]"

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
