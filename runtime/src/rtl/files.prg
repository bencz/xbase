// The file functions written over the primitives of rtl/files.c: whole files (MemoRead, MemoWrit), strings read up to
// a NUL (FReadStr), and directories (Directory, ADir, File).

#include "fileio.ch"

#define EOF_MARK     Chr( 26 )

// Directory( [cSpec], [cAttributes] ): { cName, nSize, dDate, cTime, cAttributes } for each file that matches cSpec
// (every file of the current directory without it): the files with none of the attributes D, H and S, and those that
// have only attributes cAttributes names ("D" directories, "H" hidden, "S" system); "V" asks for the volume label,
// which no file system here keeps: none. The attributes come as letters in the order RHSDA (read-only, hidden,
// system, directory, archive: __DirScan).
FUNCTION Directory( cSpec, cAttributes )
   LOCAL aResult := {}, aEntry
   cAttributes := IIF( ValType( cAttributes ) == "C", Upper( cAttributes ), "" )
   IF "V" $ cAttributes
      RETURN aResult
   ENDIF
   FOR EACH aEntry IN __DirScan( IIF( ValType( cSpec ) == "C", cSpec, "" ) )
      IF Asked( "D", aEntry[ 5 ], cAttributes ) .AND. Asked( "H", aEntry[ 5 ], cAttributes ) .AND. ;
            Asked( "S", aEntry[ 5 ], cAttributes )
         AAdd( aResult, aEntry )
      ENDIF
   NEXT
RETURN aResult

// Whether a file with attributes cHas may be listed for the attribute cLetter: it has not got it, or it was asked.
STATIC FUNCTION Asked( cLetter, cHas, cAsked )
RETURN ! cLetter $ cHas .OR. cLetter $ cAsked

// ADir( cSpec, [aNames], [aSizes], [aDates], [aTimes], [aAttributes] ): how many files match cSpec (directories,
// hidden and system files too when aAttributes is given), filling the arrays given with their names, sizes, dates,
// times and attributes, as far as each array goes; 0 without cSpec.
FUNCTION ADir( cSpec, aNames, aSizes, aDates, aTimes, aAttributes )
   LOCAL aFiles, i
   IF ValType( cSpec ) != "C"
      RETURN 0
   ENDIF
   aFiles := Directory( cSpec, IIF( ValType( aAttributes ) == "A", "DHS", "" ) )
   FOR i := 1 TO Len( aFiles )
      Fill( aNames, i, aFiles[ i ][ 1 ] )
      Fill( aSizes, i, aFiles[ i ][ 2 ] )
      Fill( aDates, i, aFiles[ i ][ 3 ] )
      Fill( aTimes, i, aFiles[ i ][ 4 ] )
      Fill( aAttributes, i, aFiles[ i ][ 5 ] )
   NEXT
RETURN Len( aFiles )

STATIC PROCEDURE Fill( aTarget, nPos, xValue )
   IF ValType( aTarget ) == "A" .AND. nPos <= Len( aTarget )
      aTarget[ nPos ] := xValue
   ENDIF
RETURN

// File( cSpec ): whether a file (hidden and system ones too, not a directory) matches cSpec.
FUNCTION File( cSpec )
RETURN ValType( cSpec ) == "C" .AND. ! Empty( cSpec ) .AND. Len( Directory( cSpec, "HS" ) ) > 0

// MemoRead( cFile ): the file's contents without the end-of-file mark (Chr( 26 )) that ends it; "" when it cannot be
// read (FError() tells why).
FUNCTION MemoRead( cFile )
   LOCAL nHandle, nSize, cText
   IF ValType( cFile ) != "C" .OR. ( nHandle := FOpen( cFile ) ) == F_ERROR
      RETURN ""
   ENDIF
   nSize := FSeek( nHandle, 0, FS_END )
   FSeek( nHandle, 0, FS_SET )
   cText := Space( nSize )
   nSize := IIF( nSize > 0, FRead( nHandle, @cText, nSize ), 0 )
   FClose( nHandle )
   cText := Left( cText, nSize )
   IF Right( cText, 1 ) == EOF_MARK
      cText := Left( cText, nSize - 1 )
   ENDIF
RETURN cText

// MemoWrit( cFile, cText ): writes cText and an end-of-file mark (Chr( 26 )) as the whole file: whether it did.
FUNCTION MemoWrit( cFile, cText )
   LOCAL nHandle, lWritten
   IF ValType( cFile ) != "C" .OR. ValType( cText ) != "C" .OR. ( nHandle := FCreate( cFile ) ) == F_ERROR
      RETURN .F.
   ENDIF
   lWritten := FWrite( nHandle, cText + EOF_MARK ) == Len( cText ) + 1
RETURN FClose( nHandle ) .AND. lWritten

// FReadStr( nHandle, nBytes ): reads nBytes (fewer at the end of the file) and returns them up to the first NUL.
FUNCTION FReadStr( nHandle, nBytes )
   LOCAL cBuffer, nRead, nNul
   IF ValType( nHandle ) != "N" .OR. ValType( nBytes ) != "N" .OR. nBytes < 1
      RETURN ""
   ENDIF
   cBuffer := Space( nBytes )
   nRead := FRead( nHandle, @cBuffer, nBytes )
   cBuffer := Left( cBuffer, nRead )
   IF ( nNul := At( Chr( 0 ), cBuffer ) ) > 0
      cBuffer := Left( cBuffer, nNul - 1 )
   ENDIF
RETURN cBuffer
