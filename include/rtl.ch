// Declarations of the runtime's xBase-callable functions, included by std.ch (a program compiled with -u or a custom
// std file includes it itself). A call to a function that is neither defined in the program nor declared here (or in
// another EXTERN FUNCTION) is a compile error.
//
//   EXTERN FUNCTION Name( param, [optionalParam], ... ) [attributes]
//
// Attributes:
//   NATIVE    written in C to the xBase calling convention (symbol xbf_NAME); without it, the function is an xBase
//             function of another module of the program (symbol xbp_NAME)
//   RESERVED  the program cannot define a function of this name (Clipper's predefined functions)
//   CHECKED   the number of arguments is checked at compile time (from the required and optional parameters,
//             unlimited with `...`)
//   ABBREV n  calls may abbreviate the name to its first n or more letters
//   FOLD      calls with constant arguments are evaluated at compile time
//
// Every function here exists in the runtime, and every xBase-callable runtime function is declared here (the
// runtime.declarations test checks both).

// Arrays
EXTERN FUNCTION AAdd( aTarget, xValue ) NATIVE RESERVED CHECKED
EXTERN FUNCTION AClone( aSource ) NATIVE
EXTERN FUNCTION Array( nElements, ... ) NATIVE
EXTERN FUNCTION ASize( aTarget, nLength ) NATIVE
EXTERN FUNCTION ATail( aArray ) NATIVE
EXTERN FUNCTION Len( xValue ) NATIVE RESERVED CHECKED FOLD

// Strings
EXTERN FUNCTION AllTrim( cString ) NATIVE
EXTERN FUNCTION Asc( cExp ) NATIVE RESERVED CHECKED
EXTERN FUNCTION At( cSearch, cTarget ) NATIVE RESERVED CHECKED
EXTERN FUNCTION Chr( nCode ) NATIVE RESERVED CHECKED
EXTERN FUNCTION IsAlpha( cString ) NATIVE
EXTERN FUNCTION IsDigit( cString ) NATIVE
EXTERN FUNCTION IsLower( cString ) NATIVE
EXTERN FUNCTION IsUpper( cString ) NATIVE
EXTERN FUNCTION Left( cString, nCount ) NATIVE RESERVED CHECKED
EXTERN FUNCTION Lower( cString ) NATIVE RESERVED CHECKED ABBREV 4
EXTERN FUNCTION LTrim( cString ) NATIVE RESERVED CHECKED ABBREV 4
EXTERN FUNCTION RAt( cSearch, cTarget ) NATIVE
EXTERN FUNCTION Replicate( cString, nCount ) NATIVE RESERVED CHECKED ABBREV 4
EXTERN FUNCTION Right( cString, nCount ) NATIVE
EXTERN FUNCTION RTrim( cString ) NATIVE RESERVED CHECKED ABBREV 4
EXTERN FUNCTION Space( nCount ) NATIVE RESERVED CHECKED ABBREV 4
EXTERN FUNCTION StrTran( cString, cSearch, [cReplace], [nStart], [nCount] ) NATIVE
EXTERN FUNCTION Stuff( cString, nStart, nDelete, cInsert ) NATIVE
EXTERN FUNCTION SubStr( cString, nStart, [nCount] ) NATIVE RESERVED CHECKED ABBREV 4
EXTERN FUNCTION Trim( cString ) NATIVE RESERVED CHECKED
EXTERN FUNCTION Upper( cString ) NATIVE RESERVED CHECKED ABBREV 4

// Numbers
EXTERN FUNCTION Abs( nValue ) NATIVE RESERVED CHECKED
EXTERN FUNCTION Exp( nExponent ) NATIVE RESERVED CHECKED
EXTERN FUNCTION Int( nValue ) NATIVE RESERVED CHECKED
EXTERN FUNCTION Log( nValue ) NATIVE RESERVED CHECKED
EXTERN FUNCTION Max( xValue1, xValue2 ) NATIVE RESERVED CHECKED
EXTERN FUNCTION Min( xValue1, xValue2 ) NATIVE RESERVED CHECKED
EXTERN FUNCTION Round( nValue, nDecimals ) NATIVE RESERVED CHECKED ABBREV 4
EXTERN FUNCTION Sqrt( nValue ) NATIVE RESERVED CHECKED
EXTERN FUNCTION Str( nNumber, [nLength], [nDecimals] ) NATIVE RESERVED CHECKED
EXTERN FUNCTION StrZero( nNumber, [nLength], [nDecimals] ) NATIVE
EXTERN FUNCTION Val( cNumber ) NATIVE RESERVED CHECKED

// Dates
EXTERN FUNCTION CDoW( dDate ) NATIVE RESERVED CHECKED
EXTERN FUNCTION CMonth( dDate ) NATIVE RESERVED CHECKED ABBREV 4
EXTERN FUNCTION CToD( cDate ) NATIVE RESERVED CHECKED
EXTERN FUNCTION Date() NATIVE RESERVED CHECKED
EXTERN FUNCTION Day( dDate ) NATIVE RESERVED CHECKED
EXTERN FUNCTION DoW( dDate ) NATIVE RESERVED CHECKED
EXTERN FUNCTION DToC( dDate ) NATIVE RESERVED CHECKED
EXTERN FUNCTION DToS( dDate ) NATIVE RESERVED CHECKED
EXTERN FUNCTION Month( dDate ) NATIVE RESERVED CHECKED ABBREV 4
EXTERN FUNCTION SToD( cDate ) NATIVE
EXTERN FUNCTION Year( dDate ) NATIVE RESERVED CHECKED

// Language, settings and runtime
EXTERN FUNCTION Empty( xValue ) NATIVE RESERVED CHECKED ABBREV 4
EXTERN FUNCTION Eval( bBlock, ... ) NATIVE CHECKED
EXTERN FUNCTION Set( nSetting, ... ) NATIVE
EXTERN FUNCTION ValType( xValue ) NATIVE RESERVED CHECKED ABBREV 4

// Console
EXTERN FUNCTION QOut( ... ) NATIVE
EXTERN FUNCTION QQOut( ... ) NATIVE

// Internal functions used by std.ch commands and by tests
EXTERN FUNCTION __GcCollect( [lYoung] ) NATIVE
EXTERN FUNCTION __Quit() NATIVE
EXTERN FUNCTION __SetCentury( [lOn] ) NATIVE
