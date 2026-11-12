// CDX orders as commands and functions see them. INDEX ON ... TAG without TO adds a tag to the production file (the
// table's name, ".cdx"), which the table's header then tells of and USE opens again with the table (no order
// controlling); a tag of a name already there is replaced, in place in the order list; TO without TAG makes a file
// anew with one tag named as the file. ordListClear() and SET INDEX TO keep the production file's orders. REINDEX
// makes each file anew with its tags in their order; ordDestroy() and DELETE TAG take a tag out, erasing a file left
// without tags (the header no longer tells of a production file then). Navigation, SEEK and positions in a
// descending tag; bounds that cross count nothing; numbers' keys give doubles; a name cut to 10 characters names the
// tag it replaces; a tag built from some records is marked partial; an NTX file goes with its order. Whenever the
// table closes, the files are the reference's (their checksums). A REINDEX whose key fails on a tag closes the orders
// after it in its file, made anew without them (the reference keeps them open on tags its file no longer has). The
// files start with "rdd_cdx", deleted at the end.
REQUEST DBFCDX
FIELD NAME, AMT

STATIC s_lFail := .F.

PROCEDURE Main()
   LOCAL i, a := { "SMITH", "JONES", "BROWN", "ADAMS", "JONES", "CLARK", "MILLER", "BAKER" }
   rddSetDefault( "DBFCDX" )
   AEval( { "rdd_cdx.dbf", "rdd_cdx.cdx", "rdd_cdx_other.cdx" }, {| f | FErase( f ) } ) // left by a run that failed
   dbCreate( "rdd_cdx", { { "NAME", "C", 10, 0 }, { "AMT", "N", 8, 2 } } )
   ? "flag:", Flag(), File( "rdd_cdx.cdx" )
   USE rdd_cdx EXCLUSIVE
   FOR i := 1 TO Len( a )
      APPEND BLANK
      REPLACE NAME WITH a[ i ], AMT WITH i * 12.5 - 40
   NEXT
   INDEX ON NAME TAG NAME
   ? ordCount(), IndexOrd(), ordName( 1 ), ordBagName( 1 )
   INDEX ON AMT TAG AMT
   ? ordCount(), IndexOrd(), ordName( 1 ), ordName( 2 ), ordKey( 2 )
   Walk()
   CLOSE
   ? "closed:", Flag(), Files()
   USE rdd_cdx EXCLUSIVE
   ? "reopened:", ordCount(), IndexOrd(), ordName( 1 ), ordName( 2 )
   ordSetFocus( "AMT" )
   Walk()
   GO TOP
   ? "key value:", ordKeyVal(), ValType( ordKeyVal() )
   APPEND BLANK
   REPLACE NAME WITH "ZED", AMT WITH -100
   GO 2
   REPLACE NAME WITH "AARON"
   INDEX ON NAME + "x" TAG NAME
   ? "replaced:", ordCount(), IndexOrd(), ordName( 1 ), ordName( 2 ), ordKey( 2 )
   INDEX ON Upper( NAME ) TO rdd_cdx_other ADDITIVE
   ? "other:", ordCount(), IndexOrd(), ordName( 3 ), ordBagName( 3 )
   ordListClear()
   ? "cleared:", ordCount(), IndexOrd()
   SET INDEX TO rdd_cdx_other
   ? "set index:", ordCount(), IndexOrd(), ordName( 1 ), ordName( 3 )
   INDEX ON AMT TAG BIG DESCENDING
   ? "descending:", ordCount(), IndexOrd(), ordName( 1 ), ordName( 2 ), ordName( 3 ), ordName( 4 )
   Walk()
   SEEK 47.5
   ? "seek:", Found(), RecNo(), ordKeyNo()
   SET SOFTSEEK ON
   SEEK 0
   ? "soft seek:", Found(), RecNo(), ordKeyNo()
   SET SOFTSEEK OFF
   ordKeyGoto( 3 )
   ? "key goto:", RecNo(), ordKeyNo(), ordKeyCount()
   SET SCOPE TO 50, 10
   ? "scope:", ordKeyCount()
   SET SCOPE TO 10, 50
   ? "crossed:", ordKeyCount()
   SET SCOPE TO
   REINDEX
   ? "reindex:", ordCount(), IndexOrd()
   CLOSE
   ? "closed:", Flag(), Files()
   USE rdd_cdx EXCLUSIVE
   ordDestroy( "AMT" )
   ? "destroyed:", ordCount(), ordName( 1 ), ordName( 2 )
   CLOSE
   ? "closed:", Flag(), Files()
   USE rdd_cdx EXCLUSIVE
   ordDestroy( "NAME" )
   ordDestroy( "BIG" )
   ? "all destroyed:", ordCount(), File( "rdd_cdx.cdx" )
   CLOSE
   ? "closed:", Flag()
   USE rdd_cdx EXCLUSIVE
   INDEX ON NAME TAG N1
   INDEX ON AMT TAG N2
   DELETE TAG N1
   ? "delete tag:", ordCount(), ordName( 1 )
   CLOSE
   ? "closed:", Flag(), Files()
   USE rdd_cdx EXCLUSIVE
   INDEX ON NAME TO rdd_cdx_other
   ? "made anew:", ordCount(), IndexOrd(), ordName( 1 ), ordName( 2 )
   CLOSE
   ? "closed:", Flag(), Files()
   USE rdd_cdx EXCLUSIVE
   INDEX ON NAME TAG ( "abcdefghi xy" )
   INDEX ON AMT TAG ( "abcdefghi xy" ) ADDITIVE
   ? "long name:", ordCount(), ordName( 2 ), ordKey( 2 ), ordSetFocus( "ABCDEFGHI XY" ), IndexOrd()
   INDEX ON NAME TAG PART NEXT 3
   ? "partial:", ordCount(), ordKeyCount()
   CLOSE
   ? "closed:", Flag(), Files()
   USE rdd_cdx VIA "DBFNTX" EXCLUSIVE
   INDEX ON NAME TO rdd_cdx_ntx
   ordDestroy( "RDD_CDX_NTX" )
   ? "ntx destroyed:", ordCount(), File( "rdd_cdx_ntx.ntx" )
   CLOSE
   FErase( "rdd_cdx.cdx" )
   Reindex()
   FErase( "rdd_cdx.dbf" )
   FErase( "rdd_cdx.cdx" )
   FErase( "rdd_cdx_other.cdx" )
RETURN

// REINDEX failing on the second of three tags: the first is made again, the others close.
STATIC PROCEDURE Reindex()
   LOCAL c
   USE rdd_cdx EXCLUSIVE
   INDEX ON NAME TAG A
   INDEX ON KeyOf() TAG B
   INDEX ON AMT TAG C
   s_lFail := .T.
   ErrorBlock( {| e | Break( e ) } )
   BEGIN SEQUENCE
      REINDEX
   RECOVER
      ? "reindex failed"
   END SEQUENCE
   s_lFail := .F.
   ? "after:", ordCount(), IndexOrd(), ordName( 1 )
   GO 2
   REPLACE NAME WITH "ZZ"
   ordSetFocus( 1 )
   c := ""
   GO TOP
   DO WHILE ! Eof()
      c += AllTrim( NAME ) + " "
      SKIP
   ENDDO
   ? "walk:", c
   CLOSE
RETURN

STATIC FUNCTION KeyOf()
   IF s_lFail
      RETURN 1 / Val( "x" ) + "x"
   ENDIF
RETURN NAME

// The table header's flags byte (1: it has a production order file).
STATIC FUNCTION Flag()
   LOCAL h := FOpen( "rdd_cdx.dbf" ), c := Space( 32 )
   FRead( h, @c, 32 )
   FClose( h )
RETURN Asc( SubStr( c, 29, 1 ) )

// The order files' sizes and checksums (read with the table closed: an exclusive file cannot be read meanwhile on
// every system).
STATIC FUNCTION Files()
   LOCAL c := "", f
   FOR EACH f IN { "rdd_cdx.cdx", "rdd_cdx_other.cdx" }
      IF File( f )
         c += " " + LTrim( Str( Len( Get( f ) ) ) ) + "/" + LTrim( Str( Sum( Get( f ) ) ) )
      ENDIF
   NEXT
RETURN c

STATIC PROCEDURE Walk()
   LOCAL c := ""
   GO TOP
   DO WHILE ! Eof()
      c += LTrim( Str( RecNo() ) ) + " "
      SKIP
   ENDDO
   ? "walk:", ordName(), c
RETURN

STATIC FUNCTION Sum( c )
   LOCAL n := 0, i
   FOR i := 1 TO Len( c )
      n := Int( ( n * 31 + Asc( SubStr( c, i, 1 ) ) ) % 1000000007 )
   NEXT
RETURN n

// The file's bytes.
STATIC FUNCTION Get( cFile )
   LOCAL h := FOpen( cFile ), n := FSeek( h, 0, 2 ), c := Space( n )
   FSeek( h, 0 )
   FRead( h, @c, n )
   FClose( h )
RETURN c
