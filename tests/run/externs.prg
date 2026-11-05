// Calls resolved through EXTERN FUNCTION declarations: abbreviated names of functions that allow them, a runtime
// function the program redefines (not reserved), an identical redeclaration, and Len() folded on constants.
EXTERN FUNCTION SubStr( cString, nStart, [nCount] ) NATIVE RESERVED CHECKED ABBREV 4

PROCEDURE Main()
   LOCAL a := { 1, 2, 3 }
   ? SubS( "abcdef", 2, 3 ), Repl( "ab", 3 ), Uppe( "xbase" ), ValT( 1 ), Empt( "" ), Mont( SToD( "20240315" ) )
   ? StrTran( "abc", "b", "x" )
   ? Len( "hello" ), Len( { 1, "two", { 3 } } ), Len( {} ), Len( a ), Len( "" )
   RETURN

FUNCTION StrTran( cString, cSearch, cReplace )
   RETURN "own " + cString + cSearch + cReplace
