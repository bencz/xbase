// Declarations of the runtime's xBase-callable functions, included by std.ch (a program compiled with -u or a custom
// std file includes it itself). A call to a function that is neither defined in the program nor declared here (or in
// another EXTERN FUNCTION) goes to another module's function, found when the program is linked.
//
//   EXTERN FUNCTION Name( param, [optionalParam], ... ) [attributes]
//
// `@` before a parameter (`@param`, `[@param]`, `@...`) of a NATIVE function: an argument written `@var` there
// reaches it as a reference (an XB_T_CELL or XB_T_REF item); elsewhere a native function receives the value.
//
// Attributes:
//   NATIVE    written in C to the xBase calling convention (symbol xbf_NAME); without it, the function is an xBase
//             function of another module of the program (symbol xbp_NAME)
//   RESERVED  the program cannot define a function of this name (Clipper's predefined functions)
//   CHECKED   the number of arguments is checked at compile time (from the required and optional parameters,
//             unlimited with `...`)
//   ABBREV n  calls may abbreviate the name to its first n or more letters
//   FOLD      calls with constant arguments are evaluated at compile time
//   ARGS      reads the arguments the function calling it received: that function keeps them in its frame
//   PARAMS    ARGS, and reads and writes that function's parameters: it keeps them in its frame slots
//   PRIVATES  creates PRIVATEs of the function calling it, which ends them when it returns
//
// Every function here exists in the runtime, and every xBase-callable runtime function is declared here (the
// runtime.declarations test checks both).

// Arrays
EXTERN FUNCTION AAdd( aTarget, xValue ) NATIVE RESERVED CHECKED
EXTERN FUNCTION AClone( aSource ) NATIVE
EXTERN FUNCTION ACopy( aSource, aTarget, [nStart], [nCount], [nTargetPos] ) NATIVE
EXTERN FUNCTION ADel( aTarget, [nPosition] ) NATIVE
EXTERN FUNCTION AEval( aArray, bBlock, [nStart], [nCount] ) NATIVE
EXTERN FUNCTION AFill( aTarget, [xValue], [nStart], [nCount] ) NATIVE
EXTERN FUNCTION AIns( aTarget, [nPosition] ) NATIVE
EXTERN FUNCTION Array( nElements, ... ) NATIVE
EXTERN FUNCTION AScan( aTarget, xSearch, [nStart], [nCount] ) NATIVE
EXTERN FUNCTION ASize( aTarget, nLength ) NATIVE
EXTERN FUNCTION ASort( aTarget, [nStart], [nCount], [bOrder] ) NATIVE
EXTERN FUNCTION ATail( aArray ) NATIVE
EXTERN FUNCTION Len( xValue ) NATIVE RESERVED CHECKED FOLD

// Hashes (xbase names)
EXTERN FUNCTION HAllocate( hHash, nCount ) NATIVE
EXTERN FUNCTION HCaseMatch( hHash, [lMatch] ) NATIVE
EXTERN FUNCTION HClear( hHash ) NATIVE
EXTERN FUNCTION HClone( hHash ) NATIVE
EXTERN FUNCTION HCopy( hSource, hDest, [nStart], [nCount] ) NATIVE
EXTERN FUNCTION HDel( hHash, xKey ) NATIVE
EXTERN FUNCTION HDelAt( hHash, nPos ) NATIVE
EXTERN FUNCTION HEval( hHash, bBlock, [nStart], [nCount] ) NATIVE
EXTERN FUNCTION HFill( hHash, xValue ) NATIVE
EXTERN FUNCTION HGet( hHash, xKey ) NATIVE
EXTERN FUNCTION HGetDef( hHash, xKey, [xDefault] ) NATIVE
EXTERN FUNCTION HHasKey( hHash, xKey ) NATIVE
EXTERN FUNCTION HKeyAt( hHash, nPos ) NATIVE
EXTERN FUNCTION HKeys( hHash ) NATIVE
EXTERN FUNCTION HMerge( hDest, hSource, [bFilter] ) NATIVE
EXTERN FUNCTION HPairAt( hHash, nPos ) NATIVE
EXTERN FUNCTION HPos( hHash, xKey ) NATIVE
EXTERN FUNCTION HScan( hHash, xSearch, [nStart], [nCount] ) NATIVE
EXTERN FUNCTION HSet( hHash, xKey, xValue ) NATIVE
EXTERN FUNCTION HSort( hHash, [bOrder] ) NATIVE
EXTERN FUNCTION HValueAt( hHash, nPos, [xNew] ) NATIVE
EXTERN FUNCTION HValues( hHash ) NATIVE
EXTERN FUNCTION Hash( ... ) NATIVE

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

// Bits (xbase functions)
EXTERN FUNCTION BitAnd( n1, n2 ) NATIVE CHECKED FOLD
EXTERN FUNCTION BitNor( n1, n2 ) NATIVE CHECKED FOLD
EXTERN FUNCTION BitNot( n ) NATIVE CHECKED FOLD
EXTERN FUNCTION BitOr( n1, n2 ) NATIVE CHECKED FOLD
EXTERN FUNCTION BitShl( n, nBits ) NATIVE CHECKED FOLD
EXTERN FUNCTION BitShr( n, nBits ) NATIVE CHECKED FOLD
EXTERN FUNCTION BitXor( n1, n2 ) NATIVE CHECKED FOLD

// Dates
EXTERN FUNCTION CDoW( dDate ) NATIVE RESERVED CHECKED
EXTERN FUNCTION CMonth( dDate ) NATIVE RESERVED CHECKED ABBREV 4
EXTERN FUNCTION CToD( cDate ) NATIVE RESERVED CHECKED
EXTERN FUNCTION Date() NATIVE RESERVED CHECKED
EXTERN FUNCTION Day( dDate ) NATIVE RESERVED CHECKED
EXTERN FUNCTION DoW( dDate ) NATIVE RESERVED CHECKED
EXTERN FUNCTION DToC( dDate ) NATIVE RESERVED CHECKED
EXTERN FUNCTION DToS( dDate ) NATIVE RESERVED CHECKED
EXTERN FUNCTION ElapsedMs() NATIVE CHECKED
EXTERN FUNCTION Month( dDate ) NATIVE RESERVED CHECKED ABBREV 4
EXTERN FUNCTION Seconds() NATIVE RESERVED CHECKED ABBREV 4
EXTERN FUNCTION SToD( cDate ) NATIVE
EXTERN FUNCTION Time() NATIVE RESERVED CHECKED
EXTERN FUNCTION Year( dDate ) NATIVE RESERVED CHECKED

// Errors
EXTERN FUNCTION Break( [xValue] ) NATIVE RESERVED
EXTERN FUNCTION ErrorBlock( [bNew] ) NATIVE
EXTERN FUNCTION ErrorLevel( [nNew] ) NATIVE
EXTERN FUNCTION ErrorNew() NATIVE
EXTERN FUNCTION ErrorSys() NATIVE

// Language, settings and runtime
EXTERN FUNCTION Do( xFunction, @... ) NATIVE
EXTERN FUNCTION Empty( xValue ) NATIVE RESERVED CHECKED ABBREV 4
EXTERN FUNCTION Eval( bBlock, @... ) NATIVE RESERVED CHECKED
EXTERN FUNCTION PCount() NATIVE RESERVED CHECKED ARGS ABBREV 4
EXTERN FUNCTION ProcFile( [nLevel] ) NATIVE
EXTERN FUNCTION ProcLine( [nLevel] ) NATIVE
EXTERN FUNCTION ProcName( [nLevel] ) NATIVE
EXTERN FUNCTION PValue( nArg, [xNew] ) NATIVE PARAMS
EXTERN FUNCTION Set( nSetting, ... ) NATIVE
EXTERN FUNCTION Type( cExpression ) NATIVE RESERVED CHECKED PRIVATES
EXTERN FUNCTION ValType( xValue ) NATIVE RESERVED CHECKED ABBREV 4

// Console and standard streams
EXTERN FUNCTION OutErr( ... ) NATIVE
EXTERN FUNCTION OutStd( ... ) NATIVE
EXTERN FUNCTION QOut( ... ) NATIVE
EXTERN FUNCTION QQOut( ... ) NATIVE

// Internal functions used by std.ch commands, by tests and by the runtime's xBase code
EXTERN FUNCTION __ArgError( nSubCode ) NATIVE ARGS
EXTERN FUNCTION __Break( [xValue] ) NATIVE
EXTERN FUNCTION __ConsoleWrite( xValue ) NATIVE
EXTERN FUNCTION __GcCollect( [lYoung] ) NATIVE
EXTERN FUNCTION __MemClear() NATIVE
EXTERN FUNCTION __MemRelease( ... ) NATIVE
EXTERN FUNCTION __MemReleaseAll( cSkeleton, lLike ) NATIVE
EXTERN FUNCTION __MemRestore( cFile, [lAdditive] ) NATIVE PRIVATES
EXTERN FUNCTION __MemSave( cFile, [cSkeleton], [lLike] ) NATIVE
EXTERN FUNCTION __Quit() NATIVE
EXTERN FUNCTION __RangeCount( nRest, [nCount] ) NATIVE
EXTERN FUNCTION __RangeStart( nLen, [nStart] ) NATIVE
EXTERN FUNCTION __SetCentury( [lOn] ) NATIVE
EXTERN FUNCTION __StdWrite( nHandle, xValue ) NATIVE
