// SET INDEX names an index file that is not there: the error handler gives the file its name, closes the area and
// asks to retry; the file then opens, but the area is closed, so no order takes it: it is closed again (it stayed
// open, held by nobody, and kept the file from being opened exclusively). The files start with
// "rdd_order_open_closed", deleted at the end.
#include "fileio.ch"
FIELD K

PROCEDURE Main()
   LOCAL h, b, nTries := 0
   dbCreate( "rdd_order_open_closed", { { "K", "N", 5, 0 } } )
   USE rdd_order_open_closed EXCLUSIVE
   APPEND BLANK
   INDEX ON K TO rdd_order_open_closed_x
   CLOSE
   USE rdd_order_open_closed SHARED
   b := ErrorBlock( {| e | nTries++, FRename( "rdd_order_open_closed_x.ntx", "rdd_order_open_closed.ntx" ), ;
                         dbCloseArea(), .T. } )
   SET INDEX TO rdd_order_open_closed
   ErrorBlock( b )
   ? nTries, Used()
   h := FOpen( "rdd_order_open_closed.ntx", FO_READWRITE + FO_EXCLUSIVE )
   ? h >= 0, FError()
   FClose( h )
   FErase( "rdd_order_open_closed.ntx" )
   FErase( "rdd_order_open_closed.dbf" )
   RETURN
