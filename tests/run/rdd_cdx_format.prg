// CDX files against the reference's bytes. A production file of two tags the reference built (below: its bytes in
// hexadecimal, numbers for runs of zero bytes; a character tag with a blank key, and a numeric one DESCENDING with a
// FOR condition) is read back in each order, and the same tags built here are the same bytes. Then the tags of a
// table of a few thousand records whose keys change at random (character keys of 1 to 40 bytes, repeated and blank
// ones, numbers, dates, logicals; unique, descending and conditioned tags), in an exclusive and then a shared table:
// the files' sizes and checksums are the reference's, whose pages split, merge, borrow from neighbours and are freed
// and taken again the same way. The files start with "rdd_cdx_format", deleted at the end.
REQUEST DBFCDX
FIELD NAME, AMT, D, L

STATIC s_nSeed

PROCEDURE Main()
   LOCAL aRef, i, c, a := { "SMITH", "JONES", "BROWN", "ADAMS", "JONES", "CLARK", "MILLER", "BAKER", "", "ZED" }
   aRef := { "0004", 10, "0A00E0010004000252434842", 480, "01000100000001", 513, ;
      "03000200FFFFFFFFFFFFFFFFDB01FFFF00000F0F10040403000C70000660", 475, "4E414D45414D54000A", 10, ;
      "0A00600100040002", 484, "05000100000005004E414D45", 508, ;
      "03000A00FFFFFFFFFFFFFFFFA401FFFF00000F0F100404030900A0040050080050030051060050020050050055070040" + ;
      "0100500A0070", ;
      420, "5A4544534D4954484D494C4C45524A4F4E4553434C41524B524F574E42414B45524144414D530010", 10, ;
      "0800680100040002", 482, "01000400080000000400414D5400414D54203E2030", 501, ;
      "03000700FFFFFFFFFFFFFFFFC601FFFF00000F0F100404030400600500510600510700510800610900510A0051", 454, ;
      "554052204E47C041803680C024" }
   rddSetDefault( "DBFCDX" )
   AEval( Directory( "rdd_cdx_format*.*" ), {| a | FErase( a[ 1 ] ) } ) // left by a run that failed
   dbCreate( "rdd_cdx_format", { { "NAME", "C", 10, 0 }, { "AMT", "N", 8, 2 } } )
   USE rdd_cdx_format EXCLUSIVE
   FOR i := 1 TO Len( a )
      APPEND BLANK
      REPLACE NAME WITH a[ i ], AMT WITH i * 12.5 - 40
   NEXT
   CLOSE
   Put( "rdd_cdx_format.cdx", Bytes( aRef ) )
   USE rdd_cdx_format EXCLUSIVE
   FOR i := 1 TO ordCount()
      ordSetFocus( i )
      c := ""
      GO TOP
      DO WHILE ! Eof()
         c += LTrim( Str( RecNo() ) ) + " "
         SKIP
      ENDDO
      ? "read:", ordName(), ordKey(), ordFor(), ordDescend(), c
   NEXT
   CLOSE
   FErase( "rdd_cdx_format.cdx" )
   USE rdd_cdx_format EXCLUSIVE
   INDEX ON NAME TAG NAME
   INDEX ON AMT TAG AMT DESCENDING FOR AMT > 0
   CLOSE
   ? "same bytes:", Get( "rdd_cdx_format.cdx" ) == Bytes( aRef )
   Changes( .F. )
   Changes( .T. )
   FErase( "rdd_cdx_format.dbf" )
   FErase( "rdd_cdx_format.cdx" )
RETURN

// The tags of a table whose records change at random (the same changes each run).
STATIC PROCEDURE Changes( lShared )
   LOCAL i
   s_nSeed := 7
   dbCreate( "rdd_cdx_format", { { "NAME", "C", 40, 0 }, { "AMT", "N", 10, 2 }, { "D", "D", 8, 0 }, ;
      { "L", "L", 1, 0 } } )
   USE rdd_cdx_format EXCLUSIVE
   FOR i := 1 TO 2000
      APPEND BLANK
      REPLACE NAME WITH RandomWord(), AMT WITH ( Rand( 20001 ) - 10000 ) / 100, ;
         D WITH SToD( "20240101" ) + Rand( 400 ), L WITH Rand( 2 ) == 1
   NEXT
   INDEX ON NAME TAG NAME
   INDEX ON AMT TAG AMT
   INDEX ON D TAG D DESCENDING
   INDEX ON L TAG L
   INDEX ON NAME + Str( AMT ) TAG MIXED FOR ! Empty( NAME )
   INDEX ON Left( NAME, 3 ) TAG FIRST UNIQUE
   CLOSE
   IF lShared
      USE rdd_cdx_format SHARED
   ELSE
      USE rdd_cdx_format EXCLUSIVE
   ENDIF
   FOR i := 1 TO 3000
      IF Rand( 10 ) == 0
         APPEND BLANK
      ELSE
         GO 1 + Rand( LastRec() )
         RLock()
      ENDIF
      SWITCH Rand( 5 )
      CASE 0
         REPLACE NAME WITH RandomWord()
         EXIT
      CASE 1
         REPLACE AMT WITH ( Rand( 20001 ) - 10000 ) / 100
         EXIT
      CASE 2
         REPLACE D WITH SToD( "20240101" ) + Rand( 400 )
         EXIT
      CASE 3
         REPLACE NAME WITH ""
         EXIT
      OTHERWISE
         REPLACE L WITH ! L, NAME WITH RandomWord()
      ENDSWITCH
   NEXT
   ? iif( lShared, "shared:", "exclusive:" ), LastRec(), ordCount()
   CLOSE
   ? "  cdx:", Len( Get( "rdd_cdx_format.cdx" ) ), Sum( Get( "rdd_cdx_format.cdx" ) )
RETURN

STATIC FUNCTION Rand( n )
   s_nSeed := Int( ( s_nSeed * 75 + 74 ) % 65537 )
RETURN Int( s_nSeed % n )

STATIC FUNCTION RandomWord()
   IF Rand( 7 ) == 0
      RETURN ""
   ENDIF
RETURN Replicate( Chr( 65 + Rand( 4 ) ), 1 + Rand( 30 ) ) + LTrim( Str( Rand( 300 ) ) )

STATIC FUNCTION Sum( c )
   LOCAL n := 0, i
   FOR i := 1 TO Len( c )
      n := Int( ( n * 31 + Asc( SubStr( c, i, 1 ) ) ) % 1000000007 )
   NEXT
RETURN n

STATIC PROCEDURE Put( cFile, cBytes )
   LOCAL h := FCreate( cFile )
   FWrite( h, cBytes )
   FClose( h )
RETURN

// The file's bytes.
STATIC FUNCTION Get( cFile )
   LOCAL h := FOpen( cFile ), n := FSeek( h, 0, 2 ), c := Space( n )
   FSeek( h, 0 )
   FRead( h, @c, n )
   FClose( h )
RETURN c

// Bytes from pieces: hexadecimal text, or a count of zero bytes.
STATIC FUNCTION Bytes( aPieces )
   LOCAL c := "", x, i
   FOR EACH x IN aPieces
      IF ValType( x ) == "N"
         c += Replicate( Chr( 0 ), x )
      ELSE
         FOR i := 1 TO Len( x ) STEP 2
            c += Chr( Digit( SubStr( x, i, 1 ) ) * 16 + Digit( SubStr( x, i + 1, 1 ) ) )
         NEXT
      ENDIF
   NEXT
RETURN c

STATIC FUNCTION Digit( c )
RETURN At( c, "0123456789ABCDEF" ) - 1
