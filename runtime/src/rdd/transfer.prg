// The copies between tables and text files (docs/rdd.md#copies-between-files), written over the primitives of
// rdd/transfer.c: COPY TO, APPEND FROM (with SDF and DELIMITED), COPY STRUCTURE [EXTENDED] and CREATE [FROM].

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
   ( nTarget )->( dbCloseArea() )
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
   ( nSource )->( dbCloseArea() )
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

// The rows of a structure the names of aFields choose, in their order (all rows without names): a name in any letter
// case, blanks around it, after "alias->", "FIELD->" or "_FIELD->" prefixes; names of no field and names given again
// are left out.
STATIC FUNCTION Chosen( aStruct, aFields )
   LOCAL aResult := {}, xName, cName, nAt
   IF Empty( aFields )
      RETURN aStruct
   ENDIF
   FOR EACH xName IN aFields
      IF ValType( xName ) == "C"
         cName := Upper( AllTrim( xName ) )
         DO WHILE ( nAt := At( "->", cName ) ) > 0 .AND. Prefix( RTrim( Left( cName, nAt - 1 ) ) )
            cName := LTrim( SubStr( cName, nAt + 2 ) )
         ENDDO
         nAt := AScan( aStruct, {| a | a[ 1 ] == cName } )
         IF nAt > 0 .AND. AScan( aResult, {| a | a[ 1 ] == cName } ) == 0
            AAdd( aResult, aStruct[ nAt ] )
         ENDIF
      ENDIF
   NEXT
RETURN aResult

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
