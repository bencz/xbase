// The copies between tables and text files (docs/rdd.md#copies-between-files), written over the primitives of
// rdd/transfer.c: COPY TO, APPEND FROM (with SDF and DELIMITED), COPY STRUCTURE [EXTENDED], CREATE [FROM], SORT,
// TOTAL, UPDATE and JOIN.

#define XSTRUCT { { "FIELD_NAME", "C", 10, 0 }, { "FIELD_TYPE", "C", 1, 0 }, { "FIELD_LEN", "N", 3, 0 }, ;
   { "FIELD_DEC", "N", 3, 0 } }
#define LEN_BYTE 256 // a character field longer than this keeps len % 256 and len / 256 in an extended structure

// __dbCopy( cFile, [aFields], [bFor], [bWhile], [nNext], [nRecord], [lRest], [cDriver], , , [xDelimiter] ): copies
// the current area's records of the scope to a new file (cDriver's, the default driver's without it) of the fields
// aFields names (every field without it), the area current when it ends; .F. when nothing could be copied.
FUNCTION __dbCopy( cFile, aFields, bFor, bWhile, nNext, nRecord, lRest, cDriver, nConnection, cCodePage, xDelimiter )
   LOCAL aStruct, nTarget, oError, lFailed := .F.
   IF ! __dbUsed( "COPY TO" ) .OR. Empty( aStruct := Chosen( dbStruct(), aFields ) )
      RETURN .F.
   ENDIF
   IF ( nTarget := __dbTransOpen( cFile, cDriver, .T., aStruct, xDelimiter ) ) == 0
      RETURN .F.
   ENDIF
   BEGIN SEQUENCE
      __dbTrans( nTarget, aStruct, bFor, bWhile, nNext, nRecord, lRest )
   RECOVER USING oError
      lFailed := .T.
   END SEQUENCE
   CloseOwn( nTarget )
   IF lFailed
      Break( oError )
   ENDIF
RETURN .T.

// __dbApp( cFile, [aFields], [bFor], [bWhile], [nNext], [nRecord], [lRest], [cDriver], , , [xDelimiter] ): appends to
// the current area the records of the scope of a file (a table opened shared and read-only, or a text file with the
// fields of the current area aFields names), its fields of the same names; the scope's blocks run with the file's
// area current. The area stays current, on the last record appended.
FUNCTION __dbApp( cFile, aFields, bFor, bWhile, nNext, nRecord, lRest, cDriver, nConnection, cCodePage, xDelimiter )
   LOCAL aStruct, nTarget := Select(), nSource, oError, lFailed := .F.
   IF ! __dbUsed( "APPEND FROM" ) .OR. Empty( aStruct := Chosen( dbStruct(), aFields ) )
      RETURN .F.
   ENDIF
   IF ( nSource := __dbTransOpen( cFile, cDriver, .F., aStruct, xDelimiter ) ) == 0
      RETURN .F.
   ENDIF
   BEGIN SEQUENCE
      dbSelectArea( nSource )
      __dbTrans( nTarget, aStruct, bFor, bWhile, nNext, nRecord, lRest )
   RECOVER USING oError
      lFailed := .T.
   END SEQUENCE
   CloseOwn( nSource )
   dbSelectArea( nTarget )
   IF lFailed
      Break( oError )
   ENDIF
RETURN .T.

// __dbSDF( lExport, cFile, [aFields], [bFor], [bWhile], [nNext], [nRecord], [lRest] ): COPY TO and APPEND FROM a
// text file of fixed-width fields.
FUNCTION __dbSDF( lExport, cFile, aFields, bFor, bWhile, nNext, nRecord, lRest, cCodePage )
   IF lExport
      RETURN __dbCopy( cFile, aFields, bFor, bWhile, nNext, nRecord, lRest, "SDF", , cCodePage )
   ENDIF
RETURN __dbApp( cFile, aFields, bFor, bWhile, nNext, nRecord, lRest, "SDF", , cCodePage )

// __dbDelim( lExport, cFile, [xDelimiter], [aFields], [bFor], [bWhile], [nNext], [nRecord], [lRest] ): COPY TO and
// APPEND FROM a text file of delimited fields (xDelimiter: "BLANK", "TAB", "PIPE", a character, or
// { cDelimiter, cSeparator }).
FUNCTION __dbDelim( lExport, cFile, xDelimiter, aFields, bFor, bWhile, nNext, nRecord, lRest, cCodePage )
   IF lExport
      RETURN __dbCopy( cFile, aFields, bFor, bWhile, nNext, nRecord, lRest, "DELIM", , cCodePage, xDelimiter )
   ENDIF
RETURN __dbApp( cFile, aFields, bFor, bWhile, nNext, nRecord, lRest, "DELIM", , cCodePage, xDelimiter )

// __dbCopyStruct( cFile, [aFields] ): a new table, empty, of the current area's fields aFields names (all of them
// without it).
FUNCTION __dbCopyStruct( cFile, aFields )
   dbCreate( cFile, Chosen( dbStruct(), aFields ) )
RETURN .T.

// __dbCopyXStruct( cFile ): a table of the current area's structure, a record for each field: its name, type, length
// and decimals (a character field longer than 255 bytes: the length's low byte and its high byte as decimals);
// .F. without a table.
FUNCTION __dbCopyXStruct( cFile )
   LOCAL aStruct, aField, nTarget
   IF ! Used()
      RETURN .F.
   ENDIF
   aStruct := dbStruct()
   IF ( nTarget := __dbTransOpen( cFile, , .T., XSTRUCT ) ) == 0
      RETURN .F.
   ENDIF
   FOR EACH aField IN aStruct
      ( nTarget )->( dbAppend() )
      ( nTarget )->( FieldPut( 1, aField[ 1 ] ) )
      ( nTarget )->( FieldPut( 2, aField[ 2 ] ) )
      IF aField[ 2 ] == "C" .AND. aField[ 3 ] >= LEN_BYTE
         ( nTarget )->( FieldPut( 3, aField[ 3 ] % LEN_BYTE ) )
         ( nTarget )->( FieldPut( 4, Int( aField[ 3 ] / LEN_BYTE ) ) )
      ELSE
         ( nTarget )->( FieldPut( 3, aField[ 3 ] ) )
         ( nTarget )->( FieldPut( 4, aField[ 4 ] ) )
      ENDIF
   NEXT
   ( nTarget )->( dbCloseArea() )
RETURN .T.

// __dbCreate( cFile, [cFrom], [cDriver], [lNew], [cAlias] ): creates and opens (in a new area with lNew, else in the
// current one) a table of the structure the extended structure table cFrom describes (its records that show), or,
// without cFrom, an extended structure table; whether it is open.
FUNCTION __dbCreate( cFile, cFrom, cDriver, lNew, cAlias )
   LOCAL aStruct := {}, nFrom, aField
   IF Empty( cFrom )
      aStruct := XSTRUCT
   ELSE
      IF ( nFrom := __dbTransOpen( cFrom, cDriver, .F. ) ) == 0
         RETURN .F.
      ENDIF
      ( nFrom )->( dbEval( {|| AAdd( aStruct, { FieldGet( 1 ), FieldGet( 2 ), FieldGet( 3 ), FieldGet( 4 ) } ) } ) )
      ( nFrom )->( dbCloseArea() )
      FOR EACH aField IN aStruct
         IF Upper( Left( aField[ 2 ], 1 ) ) == "C" .AND. aField[ 4 ] != 0
            aField[ 3 ] += aField[ 4 ] * LEN_BYTE
            aField[ 4 ] := 0
         ENDIF
      NEXT
   ENDIF
   dbCreate( cFile, aStruct, cDriver, lNew == .T., cAlias )
RETURN Used()

// __dbSort( cFile, [aKeys], [bFor], [bWhile], [nNext], [nRecord], [lRest], [cDriver] ): a new file of the current
// area's records of the scope ordered by the keys: field names, each followed by "/" and flags (D descending unless
// an A comes before it, C upper and lower case letters the same, for character fields); memo fields and names of no
// field are not keys. FOR and WHILE may be text, compiled. .F. without a table or when the file could not be made.
FUNCTION __dbSort( cFile, aKeys, bFor, bWhile, nNext, nRecord, lRest, cDriver )
   LOCAL aSpec := {}, aStruct, xKey, cItem, cFlags, nAt, nField
   IF ! Used()
      RETURN .F.
   ENDIF
   aStruct := dbStruct()
   FOR EACH xKey IN iif( ValType( aKeys ) == "A", aKeys, {} )
      IF ValType( xKey ) == "C"
         cItem := Upper( xKey )
         cFlags := ""
         IF ( nAt := At( "/", cItem ) ) > 0
            cFlags := SubStr( cItem, nAt + 1 )
            cItem := Left( cItem, nAt - 1 )
         ENDIF
         nField := FieldPos( NameOf( cItem ) )
         IF nField > 0 .AND. aStruct[ nField, 2 ] != "M"
            AAdd( aSpec, { nField, Descending( cFlags ), "C" $ cFlags .AND. aStruct[ nField, 2 ] == "C" } )
         ENDIF
      ENDIF
   NEXT
RETURN __dbSortTo( cFile, cDriver, aSpec, Block( bFor ), Block( bWhile ), nNext, nRecord, lRest )

// __dbTotal( cFile, [xKey], [aFields], [bFor], [bWhile], [nNext], [nRecord], [lRest], [cDriver] ): a new file with
// a record for each run of the scope's records of the same key (a block, a text compiled, or the controlling order's
// key; none: one run), the fields aFields names summed. .F. without fields to write or when the file could not be
// made.
FUNCTION __dbTotal( cFile, xKey, aFields, bFor, bWhile, nNext, nRecord, lRest, cDriver )
   LOCAL aSums := {}, xName
   IF ! __dbUsed( "TOTAL" )
      RETURN .F.
   ENDIF
   IF ValType( xKey ) != "B"
      xKey := iif( ValType( xKey ) == "C" .AND. ! Empty( xKey ), xKey, IndexKey( 0 ) )
      xKey := iif( Empty( xKey ), NIL, Block( xKey ) )
   ENDIF
   FOR EACH xName IN iif( ValType( aFields ) == "A", aFields, {} )
      IF ValType( xName ) == "C"
         AAdd( aSums, NameOf( xName ) )
      ENDIF
   NEXT
RETURN __dbTotalTo( cFile, cDriver, xKey, aSums, Block( bFor ), Block( bWhile ), nNext, nRecord, lRest )

// __dbUpdate( cAlias, bKey, [lRandom], [bReplace] ): for each record of the area cAlias names, evaluates bReplace with
// the current area current on its record of the same key (bKey's value): with lRandom the one a SEEK finds, else,
// both tables in the order of the key, the next one not before it (a record of a key before the current one's, or of
// none, updates nothing).
FUNCTION __dbUpdate( cAlias, bKey, lRandom, bReplace )
   LOCAL nTarget := Select(), nSource, xKey, oError, lFailed := .F.
   IF ! __dbUsed( "UPDATE" )
      RETURN .F.
   ENDIF
   dbGoTop()
   BEGIN SEQUENCE
      dbSelectArea( cAlias )
      nSource := Select()
      dbGoTop()
      DO WHILE ! Eof()
         xKey := Eval( bKey )
         dbSelectArea( nTarget )
         IF lRandom == .T.
            IF dbSeek( xKey )
               Eval( bReplace )
            ENDIF
         ELSE
            DO WHILE ! Eof() .AND. Eval( bKey ) < xKey
               dbSkip()
            ENDDO
            IF ! Eof() .AND. Eval( bKey ) == xKey
               Eval( bReplace )
            ENDIF
         ENDIF
         dbSelectArea( nSource )
         dbSkip()
      ENDDO
   RECOVER USING oError
      lFailed := .T.
   END SEQUENCE
   dbSelectArea( nTarget )
   IF lFailed
      Break( oError )
   ENDIF
RETURN .T.

// __dbJoin( cAlias, cFile, [aFields], [bFor], [cDriver] ): a new file with a record for each pair of a record of the
// current area and one of the area cAlias names that bFor (a block or a text compiled; evaluated with the current area
// current; none: every pair) takes, in the
// order of the current area, then of the other. Its fields: the current area's that aFields names (all of them
// without aFields), in its order, then the other's it names as "alias->name", in its order (a name the current area's
// fields have already is left out); each field takes the value of the table it is from. .F. without fields or when
// the file could not be made.
FUNCTION __dbJoin( cAlias, cFile, aFields, bFor, cDriver )
   LOCAL nMaster := Select(), nDetail, cDetail, aStruct := {}, aFrom := {}, aField, aItems := {}, xName, nTarget, i
   LOCAL oError, lFailed := .F.
   IF ! __dbUsed( "JOIN" )
      RETURN .F.
   ENDIF
   IF ( nDetail := Select( cAlias ) ) == 0
      dbSelectArea( cAlias ) // "Alias does not exist", which may be retried
      nDetail := Select()
      dbSelectArea( nMaster )
   ENDIF
   cDetail := Upper( ( nDetail )->( Alias() ) )
   FOR EACH xName IN iif( ValType( aFields ) == "A", aFields, {} )
      IF ValType( xName ) == "C"
         AAdd( aItems, Upper( AllTrim( xName ) ) )
      ENDIF
   NEXT
   aFields := dbStruct()
   FOR i := 1 TO Len( aFields )
      aField := aFields[ i ]
      IF Empty( aItems ) .OR. AScan( aItems, {| c | NameOf( c ) == aField[ 1 ] } ) > 0
         AAdd( aStruct, aField )
         AAdd( aFrom, { nMaster, i } )
      ENDIF
   NEXT
   aFields := ( nDetail )->( dbStruct() )
   FOR i := 1 TO Len( aFields )
      aField := aFields[ i ]
      IF AScan( aItems, {| c | DetailName( c, cDetail ) == aField[ 1 ] } ) > 0 .AND. ;
            AScan( aStruct, {| a | a[ 1 ] == aField[ 1 ] } ) == 0
         AAdd( aStruct, aField )
         AAdd( aFrom, { nDetail, i } )
      ENDIF
   NEXT
   IF Empty( aStruct ) .OR. ( nTarget := __dbTransOpen( cFile, cDriver, .T., aStruct ) ) == 0
      RETURN .F.
   ENDIF
   bFor := Block( bFor )
   bFor := iif( ValType( bFor ) == "B", bFor, {|| .T. } )
   BEGIN SEQUENCE
      dbGoTop()
      DO WHILE ! Eof()
         ( nDetail )->( dbGoTop() )
         DO WHILE ! ( nDetail )->( Eof() )
            IF Eval( bFor )
               ( nTarget )->( dbAppend() )
               FOR i := 1 TO Len( aFrom )
                  ( nTarget )->( FieldPut( i, ( aFrom[ i, 1 ] )->( FieldGet( aFrom[ i, 2 ] ) ) ) )
               NEXT
            ENDIF
            ( nDetail )->( dbSkip() )
         ENDDO
         dbSkip()
      ENDDO
   RECOVER USING oError
      lFailed := .T.
   END SEQUENCE
   CloseOwn( nTarget )
   dbSelectArea( nMaster )
   IF lFailed
      Break( oError )
   ENDIF
RETURN .T.

// The rows of a structure the names of aFields choose, in their order (all rows without names): names as NameOf()
// reads them; names of no field and names given again are left out.
STATIC FUNCTION Chosen( aStruct, aFields )
   LOCAL aResult := {}, xName, cName, nAt
   IF Empty( aFields )
      RETURN aStruct
   ENDIF
   FOR EACH xName IN aFields
      IF ValType( xName ) == "C"
         cName := NameOf( xName )
         nAt := AScan( aStruct, {| a | a[ 1 ] == cName } )
         IF nAt > 0 .AND. AScan( aResult, {| a | a[ 1 ] == cName } ) == 0
            AAdd( aResult, aStruct[ nAt ] )
         ENDIF
      ENDIF
   NEXT
RETURN aResult

// The field a name of a list stands for, in upper case, the blanks around it and "alias->", "FIELD->" or "_FIELD->"
// prefixes of the current area left out; "" for a name after another area's alias.
STATIC FUNCTION NameOf( cItem )
   LOCAL cName := Upper( AllTrim( cItem ) ), nAt
   DO WHILE ( nAt := At( "->", cName ) ) > 0
      IF ! Prefix( RTrim( Left( cName, nAt - 1 ) ) )
         RETURN ""
      ENDIF
      cName := LTrim( SubStr( cName, nAt + 2 ) )
   ENDDO
RETURN cName

// The field of the area of alias cAlias a name "alias->name" of a list stands for (the alias in any letter case,
// blanks around both parts), "" for another name.
STATIC FUNCTION DetailName( cItem, cAlias )
   LOCAL nAt := At( "->", cItem )
   IF nAt > 0 .AND. AllTrim( Left( cItem, nAt - 1 ) ) == cAlias
      RETURN AllTrim( SubStr( cItem, nAt + 2 ) )
   ENDIF
RETURN ""

// Whether a name before "->" may come before a field's: the area's alias, FIELD or _FIELD (four letters of FIELD at
// least).
STATIC FUNCTION Prefix( cWord )
   IF cWord == Upper( Alias() )
      RETURN .T.
   ENDIF
   IF Left( cWord, 1 ) == "_"
      cWord := SubStr( cWord, 2 )
   ENDIF
RETURN Len( cWord ) >= 4 .AND. Left( "FIELD", Len( cWord ) ) == cWord

// Closes the area of a copy's other file, unless a block put another table there (the copy's has no alias).
STATIC PROCEDURE CloseOwn( nArea )
   IF ( nArea )->( Used() ) .AND. Empty( ( nArea )->( Alias() ) )
      ( nArea )->( dbCloseArea() )
   ENDIF
RETURN

// Whether a SORT key's flags make it descending: a D with no A before it.
STATIC FUNCTION Descending( cFlags )
   LOCAL nD := At( "D", cFlags ), nA := At( "A", cFlags )
RETURN nD > 0 .AND. ( nA == 0 .OR. nD > nA )

// A FOR or WHILE condition given as text, compiled to a block; another value as it is.
STATIC FUNCTION Block( xCondition )
   IF ValType( xCondition ) == "C" .AND. ! Empty( xCondition )
      RETURN &( "{||" + xCondition + "}" )
   ENDIF
RETURN xCondition
