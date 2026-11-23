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
//   LIBRARY name  a public function of the xbase library `name` (symbol xbx_NAME$FUNCTION; docs/projects.md)
//   FOLD      calls with constant arguments are evaluated at compile time
//   ARGS      reads the arguments the function calling it received: that function keeps them in its frame
//   PARAMS    ARGS, and reads and writes that function's parameters: it keeps them in its frame slots
//   PRIVATES  creates PRIVATEs of the function calling it, which ends them when it returns
//   RESULT t  the type of what a call that raises no error returns: NIL, LOGICAL, INTEGER (RANGE lo TO hi: within
//             those), DOUBLE, NUMERIC (an integer or a double), STRING, DATE, ARRAY, HASH, BLOCK, OBJECT, SYMBOL,
//             joined by | (each named once), or ANY alone (any value); a parameter's `AS t` is the type of what it
//             takes (no RANGE)
//   TOTAL     a call whose arguments are of their parameters' types raises no error (with a RESULT)
//   CHOOSES   a call that raises no error returns one of its arguments as it received it: of their types
//
// Every function here exists in the runtime, and every xBase-callable runtime function is declared here (the
// runtime.declarations test checks both).

// Arrays
EXTERN FUNCTION AAdd( aTarget, xValue ) NATIVE RESERVED CHECKED
EXTERN FUNCTION AClone( aSource ) NATIVE RESULT ARRAY|NIL TOTAL
EXTERN FUNCTION ACopy( aSource, aTarget, [nStart], [nCount], [nTargetPos] ) NATIVE
EXTERN FUNCTION ADel( aTarget, [nPosition] ) NATIVE
EXTERN FUNCTION AEval( aArray, bBlock, [nStart], [nCount] ) NATIVE
EXTERN FUNCTION AFill( aTarget, [xValue], [nStart], [nCount] ) NATIVE
EXTERN FUNCTION AIns( aTarget, [nPosition] ) NATIVE
EXTERN FUNCTION Array( nElements, ... ) NATIVE RESULT ARRAY|NIL
EXTERN FUNCTION AScan( aTarget, xSearch, [nStart], [nCount] ) NATIVE
EXTERN FUNCTION ASize( aTarget, nLength ) NATIVE
EXTERN FUNCTION ASort( aTarget, [nStart], [nCount], [bOrder] ) NATIVE
EXTERN FUNCTION ATail( aArray ) NATIVE
EXTERN FUNCTION Len( xValue AS STRING|ARRAY|HASH ) NATIVE RESERVED CHECKED FOLD ;
   RESULT INTEGER RANGE 0 TO 9223372036854775807 TOTAL

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

// Objects: reflection and scalar classes (xbase names); Enumerable() is the class every collection inherits
EXTERN FUNCTION Enumerable() NATIVE
EXTERN FUNCTION ClassAssociate( oObject, cType ) NATIVE
EXTERN FUNCTION ObjHasMethod( oObject, cName ) NATIVE
EXTERN FUNCTION ObjHasVar( oObject, cName ) NATIVE
EXTERN FUNCTION ObjImplements( oObject, cInterface ) NATIVE RESULT LOGICAL TOTAL
EXTERN FUNCTION ObjIsKindOf( oObject, cClass ) NATIVE
EXTERN FUNCTION ObjMethods( oObject ) NATIVE
EXTERN FUNCTION ObjSend( xValue, cMessage, ... ) NATIVE
EXTERN FUNCTION ObjVars( oObject ) NATIVE

// Strings
EXTERN FUNCTION AllTrim( cString AS STRING ) NATIVE FOLD RESULT STRING TOTAL
EXTERN FUNCTION Asc( cExp AS STRING ) NATIVE RESERVED CHECKED FOLD RESULT INTEGER RANGE 0 TO 255 TOTAL
EXTERN FUNCTION At( cSearch AS STRING, cTarget AS STRING ) NATIVE RESERVED CHECKED FOLD ;
   RESULT INTEGER RANGE 0 TO 9223372036854775807 TOTAL
EXTERN FUNCTION CharAsc( cString ) NATIVE
EXTERN FUNCTION CharAt( cSearch, cString ) NATIVE
EXTERN FUNCTION CharChr( nCode ) NATIVE
EXTERN FUNCTION CharLeft( cString, nCount ) NATIVE
EXTERN FUNCTION CharLen( cString ) NATIVE
EXTERN FUNCTION CharRight( cString, nCount ) NATIVE
EXTERN FUNCTION CharSubStr( cString, nStart, [nCount] ) NATIVE
EXTERN FUNCTION Chr( nCode AS NUMERIC ) NATIVE RESERVED CHECKED FOLD RESULT STRING TOTAL
EXTERN FUNCTION CPConvert( cText, cFrom, cTo ) NATIVE
EXTERN FUNCTION HardCR( cString ) NATIVE
EXTERN FUNCTION IsAlpha( cString ) NATIVE
EXTERN FUNCTION IsDigit( cString ) NATIVE
EXTERN FUNCTION IsLower( cString ) NATIVE
EXTERN FUNCTION IsUpper( cString ) NATIVE
EXTERN FUNCTION Left( cString AS STRING, nCount AS NUMERIC ) NATIVE RESERVED CHECKED FOLD RESULT STRING TOTAL
EXTERN FUNCTION Lower( cString AS STRING ) NATIVE RESERVED CHECKED ABBREV 4 FOLD RESULT STRING TOTAL
EXTERN FUNCTION LTrim( cString AS STRING ) NATIVE RESERVED CHECKED ABBREV 4 FOLD RESULT STRING TOTAL
EXTERN FUNCTION MemoLine( cString, [nLineLength], [nLineNumber], [nTabSize], [lWrap] ) NATIVE
EXTERN FUNCTION MemoTran( cString, [cHardCR], [cSoftCR] ) NATIVE
EXTERN FUNCTION MLCount( cString, [nLineLength], [nTabSize], [lWrap] ) NATIVE
EXTERN FUNCTION MLCToPos( cString, nLineLength, nLine, nCol, [nTabSize], [lWrap] ) NATIVE
EXTERN FUNCTION MLPos( cString, nLineLength, nLine, [nTabSize], [lWrap] ) NATIVE
EXTERN FUNCTION MPosToLC( cString, nLineLength, nPos, [nTabSize], [lWrap] ) NATIVE
EXTERN FUNCTION Pad( xValue, nLength, [cFill] ) NATIVE
EXTERN FUNCTION PadC( xValue, nLength, [cFill] ) NATIVE RESULT STRING
EXTERN FUNCTION PadL( xValue, nLength, [cFill] ) NATIVE RESULT STRING
EXTERN FUNCTION PadR( xValue, nLength, [cFill] ) NATIVE RESULT STRING
EXTERN FUNCTION RAt( cSearch, cTarget ) NATIVE FOLD RESULT INTEGER RANGE 0 TO 9223372036854775807 TOTAL
EXTERN FUNCTION Replicate( cString, nCount ) NATIVE RESERVED CHECKED ABBREV 4 FOLD RESULT STRING
EXTERN FUNCTION Right( cString, nCount ) NATIVE FOLD RESULT STRING TOTAL
EXTERN FUNCTION RTrim( cString AS STRING ) NATIVE RESERVED CHECKED ABBREV 4 FOLD RESULT STRING TOTAL
EXTERN FUNCTION SoundEx( cString ) NATIVE
EXTERN FUNCTION Space( nCount ) NATIVE RESERVED CHECKED ABBREV 4 FOLD RESULT STRING
EXTERN FUNCTION StrTran( cString, cSearch, [cReplace], [nStart], [nCount] ) NATIVE
EXTERN FUNCTION Stuff( cString, nStart, nDelete, cInsert ) NATIVE
EXTERN FUNCTION SubStr( cString AS STRING, nStart AS NUMERIC, [nCount] AS NUMERIC ) NATIVE RESERVED CHECKED ;
   ABBREV 4 FOLD RESULT STRING TOTAL
EXTERN FUNCTION Transform( xValue, [cPicture] ) NATIVE RESERVED CHECKED
EXTERN FUNCTION Trim( cString AS STRING ) NATIVE RESERVED CHECKED FOLD RESULT STRING TOTAL
EXTERN FUNCTION Upper( cString AS STRING ) NATIVE RESERVED CHECKED ABBREV 4 FOLD RESULT STRING TOTAL

// Numbers
EXTERN FUNCTION Abs( nValue AS NUMERIC ) NATIVE RESERVED CHECKED RESULT NUMERIC TOTAL
EXTERN FUNCTION Bin2I( cBinary ) NATIVE
EXTERN FUNCTION Bin2L( cBinary ) NATIVE
EXTERN FUNCTION Bin2W( cBinary ) NATIVE
EXTERN FUNCTION Descend( xValue ) NATIVE
EXTERN FUNCTION Exp( nExponent ) NATIVE RESERVED CHECKED RESULT NUMERIC
EXTERN FUNCTION I2Bin( nInteger ) NATIVE
EXTERN FUNCTION Int( nValue ) NATIVE RESERVED CHECKED RESULT NUMERIC
EXTERN FUNCTION L2Bin( nLong ) NATIVE
EXTERN FUNCTION LenNum( nNumber ) NATIVE
EXTERN FUNCTION Log( nValue ) NATIVE RESERVED CHECKED RESULT NUMERIC
EXTERN FUNCTION Max( xValue1, xValue2 ) NATIVE RESERVED CHECKED RESULT NUMERIC|DATE CHOOSES
EXTERN FUNCTION Min( xValue1, xValue2 ) NATIVE RESERVED CHECKED RESULT NUMERIC|DATE CHOOSES
EXTERN FUNCTION Mod( nNumber, nBase ) NATIVE RESULT NUMERIC
EXTERN FUNCTION Round( nValue, nDecimals ) NATIVE RESERVED CHECKED ABBREV 4 RESULT NUMERIC
EXTERN FUNCTION Sqrt( nValue ) NATIVE RESERVED CHECKED RESULT NUMERIC
EXTERN FUNCTION Str( nNumber, [nLength], [nDecimals] ) NATIVE RESERVED CHECKED RESULT STRING
EXTERN FUNCTION StrZero( nNumber, [nLength], [nDecimals] ) NATIVE RESULT STRING
EXTERN FUNCTION Val( cNumber ) NATIVE RESERVED CHECKED RESULT NUMERIC

// Math (CA-Tools): not RESERVED, so a program's own function of one of these names comes first
EXTERN FUNCTION ACos( nCosine ) NATIVE CHECKED RESULT NUMERIC
EXTERN FUNCTION ASin( nSine ) NATIVE CHECKED RESULT NUMERIC
EXTERN FUNCTION ATan( nTangent ) NATIVE CHECKED RESULT NUMERIC
EXTERN FUNCTION Atn2( nY, nX ) NATIVE CHECKED RESULT NUMERIC
EXTERN FUNCTION Ceiling( nValue ) NATIVE CHECKED RESULT NUMERIC
EXTERN FUNCTION Cos( nRadians ) NATIVE CHECKED RESULT NUMERIC
EXTERN FUNCTION Cosh( nValue ) NATIVE CHECKED RESULT NUMERIC
EXTERN FUNCTION Cot( nRadians ) NATIVE CHECKED RESULT NUMERIC
EXTERN FUNCTION DToR( nDegrees ) NATIVE CHECKED RESULT NUMERIC
EXTERN FUNCTION Fact( nValue ) NATIVE CHECKED RESULT NUMERIC
EXTERN FUNCTION Floor( nValue ) NATIVE CHECKED RESULT NUMERIC
EXTERN FUNCTION Log10( nValue ) NATIVE CHECKED RESULT NUMERIC
EXTERN FUNCTION Pi() NATIVE CHECKED RESULT NUMERIC
EXTERN FUNCTION RToD( nRadians ) NATIVE CHECKED RESULT NUMERIC
EXTERN FUNCTION Sign( nValue AS NUMERIC ) NATIVE CHECKED RESULT INTEGER RANGE -1 TO 1 TOTAL
EXTERN FUNCTION Sin( nRadians ) NATIVE CHECKED RESULT NUMERIC
EXTERN FUNCTION Sinh( nValue ) NATIVE CHECKED RESULT NUMERIC
EXTERN FUNCTION Tan( nRadians ) NATIVE CHECKED RESULT NUMERIC
EXTERN FUNCTION Tanh( nValue ) NATIVE CHECKED RESULT NUMERIC

// Bits (xbase functions)
EXTERN FUNCTION BitAnd( n1 AS NUMERIC, n2 AS NUMERIC ) NATIVE CHECKED FOLD RESULT INTEGER TOTAL
EXTERN FUNCTION BitNor( n1 AS NUMERIC, n2 AS NUMERIC ) NATIVE CHECKED FOLD RESULT INTEGER TOTAL
EXTERN FUNCTION BitNot( n AS NUMERIC ) NATIVE CHECKED FOLD RESULT INTEGER TOTAL
EXTERN FUNCTION BitOr( n1 AS NUMERIC, n2 AS NUMERIC ) NATIVE CHECKED FOLD RESULT INTEGER TOTAL
EXTERN FUNCTION BitShl( n AS NUMERIC, nBits AS NUMERIC ) NATIVE CHECKED FOLD RESULT INTEGER TOTAL
EXTERN FUNCTION BitShr( n AS NUMERIC, nBits AS NUMERIC ) NATIVE CHECKED FOLD RESULT INTEGER TOTAL
EXTERN FUNCTION BitXor( n1 AS NUMERIC, n2 AS NUMERIC ) NATIVE CHECKED FOLD RESULT INTEGER TOTAL

// Dates
EXTERN FUNCTION CDoW( dDate ) NATIVE RESERVED CHECKED
EXTERN FUNCTION CMonth( dDate ) NATIVE RESERVED CHECKED ABBREV 4
EXTERN FUNCTION CToD( cDate AS STRING ) NATIVE RESERVED CHECKED RESULT DATE TOTAL
EXTERN FUNCTION Date() NATIVE RESERVED CHECKED RESULT DATE TOTAL
EXTERN FUNCTION Day( dDate AS DATE ) NATIVE RESERVED CHECKED RESULT INTEGER RANGE 0 TO 31 TOTAL
EXTERN FUNCTION DoW( dDate AS DATE ) NATIVE RESERVED CHECKED RESULT INTEGER RANGE 0 TO 7 TOTAL
EXTERN FUNCTION DToC( dDate ) NATIVE RESERVED CHECKED RESULT STRING
EXTERN FUNCTION DToS( dDate ) NATIVE RESERVED CHECKED RESULT STRING
EXTERN FUNCTION ElapsedMs() NATIVE CHECKED RESULT INTEGER RANGE 0 TO 9223372036854775807 TOTAL
EXTERN FUNCTION Month( dDate AS DATE ) NATIVE RESERVED CHECKED ABBREV 4 RESULT INTEGER RANGE 0 TO 12 TOTAL
EXTERN FUNCTION Seconds() NATIVE RESERVED CHECKED ABBREV 4 RESULT NUMERIC
EXTERN FUNCTION SToD( cDate ) NATIVE RESULT DATE TOTAL
EXTERN FUNCTION Time() NATIVE RESERVED CHECKED RESULT STRING
EXTERN FUNCTION Year( dDate AS DATE ) NATIVE RESERVED CHECKED RESULT INTEGER TOTAL

// Errors
EXTERN FUNCTION Break( [xValue] ) NATIVE RESERVED
EXTERN FUNCTION ErrorBlock( [bNew] ) NATIVE
EXTERN FUNCTION ErrorLevel( [nNew] ) NATIVE
EXTERN FUNCTION ErrorNew() NATIVE
EXTERN FUNCTION ErrorSys() NATIVE
EXTERN FUNCTION Throw( [xValue] ) NATIVE

// Language, settings and runtime
EXTERN FUNCTION AMPM( cTime ) NATIVE
EXTERN FUNCTION Days( nSeconds ) NATIVE
EXTERN FUNCTION Do( xFunction, @... ) NATIVE
EXTERN FUNCTION ElapTime( cStart, cEnd ) NATIVE
EXTERN FUNCTION Empty( xValue ) NATIVE RESERVED CHECKED ABBREV 4 FOLD RESULT LOGICAL TOTAL
EXTERN FUNCTION Eval( bBlock, @... ) NATIVE RESERVED CHECKED
EXTERN FUNCTION MemVarBlock( cName ) NATIVE
EXTERN FUNCTION PCount() NATIVE RESERVED CHECKED ARGS ABBREV 4
EXTERN FUNCTION ProcFile( [nLevel] ) NATIVE
EXTERN FUNCTION ProcLine( [nLevel] ) NATIVE
EXTERN FUNCTION ProcName( [nLevel] ) NATIVE
EXTERN FUNCTION PValue( nArg, [xNew] ) NATIVE PARAMS
EXTERN FUNCTION Secs( cTime ) NATIVE
EXTERN FUNCTION Set( nSetting, ... ) NATIVE
EXTERN FUNCTION TString( nSeconds ) NATIVE
EXTERN FUNCTION Type( cExpression ) NATIVE RESERVED CHECKED PRIVATES
EXTERN FUNCTION ValType( xValue ) NATIVE RESERVED CHECKED ABBREV 4

// Threads and tasks (xbase functions): the objects they make answer messages (docs/concurrency.md)
EXTERN FUNCTION AtomicInt( [nValue] ) NATIVE CHECKED
EXTERN FUNCTION CancellationTokenSource( ... ) NATIVE CHECKED
EXTERN FUNCTION Channel( [nCapacity] ) NATIVE CHECKED
EXTERN FUNCTION Condition() NATIVE CHECKED
EXTERN FUNCTION Mutex() NATIVE CHECKED
EXTERN FUNCTION Semaphore( nInitial, [nMaximum] ) NATIVE CHECKED
EXTERN FUNCTION TaskCompletionSource() NATIVE CHECKED
EXTERN FUNCTION TaskDelay( nMs, [oToken] ) NATIVE CHECKED
EXTERN FUNCTION TaskFromResult( [xValue] ) NATIVE CHECKED
EXTERN FUNCTION TaskRun( bWork, ... ) NATIVE CHECKED
EXTERN FUNCTION TaskTimeout( oTask, nMs ) NATIVE CHECKED
EXTERN FUNCTION TaskUnobservedBlock( [bNew] ) NATIVE
EXTERN FUNCTION TaskWhenAll( aTasks ) NATIVE CHECKED
EXTERN FUNCTION TaskWhenAny( aTasks ) NATIVE CHECKED
EXTERN FUNCTION TaskWorkers() NATIVE CHECKED
EXTERN FUNCTION TaskYield() NATIVE CHECKED
EXTERN FUNCTION ThreadStart( bWork, ... ) NATIVE CHECKED

// Databases (work areas)
EXTERN FUNCTION Alias( [nArea] ) NATIVE
EXTERN FUNCTION Bof() NATIVE RESERVED CHECKED
EXTERN FUNCTION dbAppend( [lReleaseLocks] ) NATIVE
EXTERN FUNCTION dbClearFilter() NATIVE
EXTERN FUNCTION dbCloseAll() NATIVE
EXTERN FUNCTION dbCloseArea() NATIVE
EXTERN FUNCTION dbCommit() NATIVE
EXTERN FUNCTION dbCommitAll() NATIVE
EXTERN FUNCTION dbClearIndex() NATIVE
EXTERN FUNCTION dbClearRelation() NATIVE
EXTERN FUNCTION dbCreate( cFile, aStruct, [cDriver], [lNewArea], [cAlias] ) NATIVE
EXTERN FUNCTION dbCreateIndex( cFile, cKey, [bKey], [lUnique] ) NATIVE
EXTERN FUNCTION dbDelete() NATIVE
EXTERN FUNCTION dbEval( bBlock, [bFor], [bWhile], [nNext], [nRecord], [lRest] ) NATIVE
EXTERN FUNCTION DBFCDX() NATIVE
EXTERN FUNCTION DBFNTX() NATIVE
EXTERN FUNCTION dbFilter() NATIVE
EXTERN FUNCTION dbGoBottom() NATIVE
EXTERN FUNCTION dbGoTo( nRecord ) NATIVE
EXTERN FUNCTION dbGoTop() NATIVE
EXTERN FUNCTION dbRecall() NATIVE
EXTERN FUNCTION dbReindex() NATIVE
EXTERN FUNCTION dbRelation( [nRelation] ) NATIVE
EXTERN FUNCTION dbRLock( [nRecord] ) NATIVE
EXTERN FUNCTION dbRLockList() NATIVE
EXTERN FUNCTION dbRSelect( [nRelation] ) NATIVE
EXTERN FUNCTION dbRUnlock( [nRecord] ) NATIVE
EXTERN FUNCTION dbSeek( xKey, [lSoftSeek], [lLast] ) NATIVE
EXTERN FUNCTION dbSelectArea( xArea ) NATIVE
EXTERN FUNCTION dbSetFilter( bCondition, [cCondition] ) NATIVE
EXTERN FUNCTION dbSetIndex( cFile ) NATIVE
EXTERN FUNCTION dbSetOrder( nOrder ) NATIVE
EXTERN FUNCTION dbSetRelation( xArea, bExpr, [cExpr], [lScoped] ) NATIVE
EXTERN FUNCTION dbSkip( [nRecords] ) NATIVE
EXTERN FUNCTION dbStruct() NATIVE
EXTERN FUNCTION dbTableExt() NATIVE
EXTERN FUNCTION dbUnlock() NATIVE
EXTERN FUNCTION dbUnlockAll() NATIVE
EXTERN FUNCTION dbUseArea( [lNewArea], [cDriver], [cName], [xAlias], [lShared], [lReadOnly] ) NATIVE
EXTERN FUNCTION Deleted() NATIVE RESERVED CHECKED ABBREV 4
EXTERN FUNCTION Delim() NATIVE
EXTERN FUNCTION Eof() NATIVE RESERVED CHECKED
EXTERN FUNCTION FCount() NATIVE RESERVED CHECKED ABBREV 4
EXTERN FUNCTION FieldGet( nField ) NATIVE
EXTERN FUNCTION FieldName( nField ) NATIVE RESERVED CHECKED ABBREV 4
EXTERN FUNCTION FieldPos( cField ) NATIVE
EXTERN FUNCTION FieldLen( xField ) NATIVE
EXTERN FUNCTION FieldDec( xField ) NATIVE
EXTERN FUNCTION FieldType( xField ) NATIVE
EXTERN FUNCTION FieldPut( nField, xValue ) NATIVE
EXTERN FUNCTION FLock() NATIVE RESERVED CHECKED ABBREV 4
EXTERN FUNCTION Found() NATIVE RESERVED CHECKED ABBREV 4
EXTERN FUNCTION Header() NATIVE
EXTERN FUNCTION IndexExt() NATIVE
EXTERN FUNCTION IndexKey( [nOrder] ) NATIVE
EXTERN FUNCTION IndexOrd() NATIVE
EXTERN FUNCTION LastRec() NATIVE RESERVED CHECKED ABBREV 5
EXTERN FUNCTION Lock() NATIVE RESERVED CHECKED ABBREV 4
EXTERN FUNCTION LUpdate() NATIVE
EXTERN FUNCTION NetErr( [lNewError] ) NATIVE
EXTERN FUNCTION ordBagExt() NATIVE
EXTERN FUNCTION ordBagName( [xOrder] ) NATIVE
EXTERN FUNCTION ordCondSet( [cFor], [bFor], [lAll], [bWhile], [bEval], [nInterval], [nStart], [nNext], [nRecord], ;
   [lRest], [lDescend], [lCompound], [lAdditive], [lCurrent] ) NATIVE
EXTERN FUNCTION ordCount( [cBag] ) NATIVE
EXTERN FUNCTION ordCreate( [cBag], [cOrder], [cKey], [bKey], [lUnique] ) NATIVE
EXTERN FUNCTION ordDescend( [xOrder] ) NATIVE
EXTERN FUNCTION ordDestroy( xOrder, [cBag] ) NATIVE
EXTERN FUNCTION ordFor( [xOrder] ) NATIVE
EXTERN FUNCTION ordIsUnique( [xOrder] ) NATIVE
EXTERN FUNCTION ordKey( [xOrder] ) NATIVE
EXTERN FUNCTION ordKeyCount( [xOrder] ) NATIVE
EXTERN FUNCTION ordKeyGoto( nKeyNo ) NATIVE
EXTERN FUNCTION ordKeyNo( [xOrder] ) NATIVE
EXTERN FUNCTION ordKeyVal() NATIVE
EXTERN FUNCTION ordListAdd( cBag ) NATIVE
EXTERN FUNCTION ordListClear() NATIVE
EXTERN FUNCTION ordListRebuild() NATIVE
EXTERN FUNCTION ordName( [nOrder] ) NATIVE
EXTERN FUNCTION ordNumber( cOrder ) NATIVE
EXTERN FUNCTION ordScope( nScope, [xValue] ) NATIVE
EXTERN FUNCTION ordSetRelation( xArea, bKey, [cKey] ) NATIVE
EXTERN FUNCTION ordSetFocus( [xOrder] ) NATIVE
EXTERN FUNCTION ordSkipUnique( [nDirection] ) NATIVE
EXTERN FUNCTION RddName() NATIVE
EXTERN FUNCTION RddSetDefault( [cDriver] ) NATIVE
EXTERN FUNCTION RecCount() NATIVE RESERVED CHECKED ABBREV 4
EXTERN FUNCTION RecNo() NATIVE RESERVED CHECKED ABBREV 4
EXTERN FUNCTION RecSize() NATIVE
EXTERN FUNCTION RLock() NATIVE RESERVED CHECKED ABBREV 4
EXTERN FUNCTION SDF() NATIVE
EXTERN FUNCTION Select( [xAlias] ) NATIVE RESERVED CHECKED ABBREV 4
EXTERN FUNCTION Used() NATIVE
EXTERN FUNCTION __AbstractMethod( cMember ) NATIVE
EXTERN FUNCTION __AreaEval( xArea, bBlock ) NATIVE
EXTERN FUNCTION __dbApp( cFile, [aFields], [bFor], [bWhile], [nNext], [nRecord], [lRest], [cDriver], [nConnection], ;
   [cCodePage], [xDelimiter] ) NATIVE
EXTERN FUNCTION __dbContinue() NATIVE
EXTERN FUNCTION __dbCopy( cFile, [aFields], [bFor], [bWhile], [nNext], [nRecord], [lRest], [cDriver], [nConnection], ;
   [cCodePage], [xDelimiter] ) NATIVE
EXTERN FUNCTION __dbCopyStruct( cFile, [aFields] ) NATIVE
EXTERN FUNCTION __dbCopyXStruct( cFile ) NATIVE
EXTERN FUNCTION __dbCreate( cFile, [cFrom], [cDriver], [lNew], [cAlias] ) NATIVE
EXTERN FUNCTION __dbDelim( lExport, cFile, [xDelimiter], [aFields], [bFor], [bWhile], [nNext], [nRecord], [lRest], ;
   [cCodePage] ) NATIVE
EXTERN FUNCTION __dbJoin( cAlias, cFile, [aFields], [bFor], [cDriver] ) NATIVE
EXTERN FUNCTION __dbLocate( [bFor], [bWhile], [nNext], [nRecord], [lRest] ) NATIVE
EXTERN FUNCTION __dbPack() NATIVE
EXTERN FUNCTION __dbPageCache( [nBudget] ) NATIVE
EXTERN FUNCTION __dbSDF( lExport, cFile, [aFields], [bFor], [bWhile], [nNext], [nRecord], [lRest], [cCodePage] ) NATIVE
EXTERN FUNCTION __dbSort( cFile, [aKeys], [bFor], [bWhile], [nNext], [nRecord], [lRest], [cDriver] ) NATIVE
EXTERN FUNCTION __dbSortTo( cFile, [cDriver], [aKeys], [bFor], [bWhile], [nNext], [nRecord], [lRest] ) NATIVE
EXTERN FUNCTION __dbTotal( cFile, [xKey], [aFields], [bFor], [bWhile], [nNext], [nRecord], [lRest], [cDriver] ) NATIVE
EXTERN FUNCTION __dbTotalTo( cFile, [cDriver], [bKey], [aSums], [bFor], [bWhile], [nNext], [nRecord], [lRest] ) NATIVE
EXTERN FUNCTION __dbTrans( nTarget, aFields, [bFor], [bWhile], [nNext], [nRecord], [lRest] ) NATIVE
EXTERN FUNCTION __dbTransOpen( cFile, [cDriver], [lCreate], [aStruct], [xDelimiter] ) NATIVE
EXTERN FUNCTION __dbUpdate( cAlias, bKey, [lRandom], [bReplace] ) NATIVE
EXTERN FUNCTION __dbUsed( cOperation ) NATIVE
EXTERN FUNCTION __dbZap() NATIVE

// Files, directories and environment
EXTERN FUNCTION ADir( cSpec, [aNames], [aSizes], [aDates], [aTimes], [aAttributes] ) NATIVE
EXTERN FUNCTION CurDir( [cDrive] ) NATIVE
EXTERN FUNCTION DirChange( cDirectory ) NATIVE
EXTERN FUNCTION Directory( [cSpec], [cAttributes] ) NATIVE
EXTERN FUNCTION DiskSpace( [nDrive] ) NATIVE
EXTERN FUNCTION FClose( nHandle ) NATIVE
EXTERN FUNCTION FCreate( cFile, [nAttribute] ) NATIVE
EXTERN FUNCTION FErase( cFile ) NATIVE
EXTERN FUNCTION FError() NATIVE
EXTERN FUNCTION File( cSpec ) NATIVE
EXTERN FUNCTION FOpen( cFile, [nMode] ) NATIVE
EXTERN FUNCTION FRead( nHandle, @cBuffer, nBytes ) NATIVE
EXTERN FUNCTION FReadStr( nHandle, nBytes ) NATIVE
EXTERN FUNCTION FRename( cOld, cNew ) NATIVE
EXTERN FUNCTION FSeek( nHandle, nOffset, [nOrigin] ) NATIVE
EXTERN FUNCTION FWrite( nHandle, cBuffer, [nBytes] ) NATIVE
EXTERN FUNCTION GetEnv( [cName] ) NATIVE
EXTERN FUNCTION __Run( [cCommand] ) NATIVE
EXTERN FUNCTION MemoRead( cFile ) NATIVE
EXTERN FUNCTION MemoWrit( cFile, cText ) NATIVE

// Console and standard streams
EXTERN FUNCTION OutErr( ... ) NATIVE
EXTERN FUNCTION OutStd( ... ) NATIVE
EXTERN FUNCTION QOut( ... ) NATIVE
EXTERN FUNCTION QQOut( ... ) NATIVE

// Internal functions used by std.ch commands, by tests and by the runtime's xBase code
EXTERN FUNCTION __ArgError( nSubCode, [lSubstitute] ) NATIVE ARGS
EXTERN FUNCTION __AwaitEach( xSource ) NATIVE
EXTERN FUNCTION __AwaitEachAwaits( oEnumerator ) NATIVE
EXTERN FUNCTION __AwaitEachKey( oEnumerator, nPosition ) NATIVE
EXTERN FUNCTION __Break( [xValue] ) NATIVE
EXTERN FUNCTION __CallLines( [oError] ) NATIVE
EXTERN FUNCTION __ConsoleWrite( lNewLine, ... ) NATIVE
EXTERN FUNCTION __DirScan( [cSpec] ) NATIVE
EXTERN FUNCTION __Dispose( xValue ) NATIVE
EXTERN FUNCTION __GcCollect( [lYoung] ) NATIVE
EXTERN FUNCTION __GcMemory() NATIVE
EXTERN FUNCTION __IsCollection( xValue ) NATIVE
EXTERN FUNCTION __LanguageCurrent() NATIVE
EXTERN FUNCTION __LanguageTexts( cId ) NATIVE
EXTERN FUNCTION __MemClear() NATIVE
EXTERN FUNCTION __MemNamesInUse( [lLetGo] ) NATIVE
EXTERN FUNCTION __MemRelease( ... ) NATIVE
EXTERN FUNCTION __MemReleaseAll( cSkeleton, lLike ) NATIVE
EXTERN FUNCTION __MemRestore( cFile, [lAdditive] ) NATIVE PRIVATES
EXTERN FUNCTION __MemSave( cFile, [cSkeleton], [lLike] ) NATIVE
EXTERN FUNCTION __Quit() NATIVE
EXTERN FUNCTION __Split( cText, cSeparator ) NATIVE
EXTERN FUNCTION __Affix( cText, cPart, lEnd ) NATIVE
EXTERN FUNCTION __RangeCount( nRest, [nCount] ) NATIVE
EXTERN FUNCTION __RangeStart( nLen, [nStart] ) NATIVE
EXTERN FUNCTION __ScanValue( xTarget, xSearch, nStart, nCount ) NATIVE
EXTERN FUNCTION __Synchronize( oMutex ) NATIVE
EXTERN FUNCTION __ValueLess( x, y ) NATIVE
EXTERN FUNCTION __ValueMatches( xValue, xSearch ) NATIVE
EXTERN FUNCTION __SetCentury( [lOn] ) NATIVE
EXTERN FUNCTION __StdWrite( nHandle, ... ) NATIVE
EXTERN FUNCTION __TaskFail( [xValue], [cCalls] ) NATIVE
EXTERN FUNCTION __TaskUnobserved( xFailure, oTask ) NATIVE
EXTERN FUNCTION __ErrorCalls( oError ) NATIVE
