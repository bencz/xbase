// A table open shared: another program changes the header's flags (byte 28: the production file's) and language byte
// (29) while it is open here; APPEND BLANK then writes the record count and the date only, so both survive. The file
// starts with "rdd_append_header", deleted at the end.
#include "fileio.ch"

PROCEDURE Main()
   LOCAL h, c := Space( 32 )
   dbCreate( "rdd_append_header", { { "N", "N", 5, 0 } } )
   USE rdd_append_header SHARED
   APPEND BLANK
   h := FOpen( "rdd_append_header.dbf", FO_READWRITE + FO_SHARED )
   FSeek( h, 28 )
   FWrite( h, Chr( 1 ) + Chr( 3 ) )
   FClose( h )
   APPEND BLANK
   APPEND BLANK
   ? LastRec()
   CLOSE
   h := FOpen( "rdd_append_header.dbf", FO_READ + FO_SHARED )
   FRead( h, @c, 32 )
   FClose( h )
   ? Asc( SubStr( c, 29, 1 ) ), Asc( SubStr( c, 30, 1 ) ), Bin2L( SubStr( c, 5, 4 ) )
   FErase( "rdd_append_header.dbf" )
   RETURN
