// The file functions on the files of tables that areas have open: FOpen(), FRead() and FClose() of a table's file,
// and MemoRead() of it, again and again, keep working and keep the area's locks (a second area still finds the record
// locked; tests/platform/shared_plain.c checks the system's locks and the handle kept); a second FClose() of a handle
// is FError() 6, and so are FRead(), FWrite() and FSeek() of it. Safer than the reference: FCreate() of a table's
// file, which would empty it, and an FOpen() that would keep the areas from it (FO_EXCLUSIVE, FO_DENYREAD,
// FO_DENYWRITE while an area writes) are refused with 32. The files start with "files_tables", deleted at the end.
#include "fileio.ch"

FIELD N

PROCEDURE Main()
   LOCAL h, k, nFailed := 0, c

   dbCreate( "files_tables", { { "N", "N", 5, 0 } } )
   USE files_tables EXCLUSIVE NEW
   APPEND BLANK
   CLOSE
   USE files_tables SHARED ALIAS ta NEW
   ? "locked:", RLock()

   FOR k := 1 TO 3000
      h := FOpen( "files_tables.dbf" )
      IF h == -1
         nFailed++
      ELSE
         c := Space( 1 )
         FRead( h, @c, 1 )
         FClose( h )
      ENDIF
   NEXT
   ? "opened 3000 times, failed:", nFailed, "first byte:", Asc( c )
   FOR k := 1 TO 3000
      IF Len( MemoRead( "files_tables.dbf" ) ) == 0
         nFailed++
      ENDIF
   NEXT
   ? "read 3000 times, failed:", nFailed
   h := FOpen( "files_tables.dbf", 2 )
   ? "closes:", FClose( h ), FError(), FClose( h ), FError()
   c := Space( 4 )
   ? "after the close:", FRead( h, @c, 4 ), FError(), FWrite( h, "x" ), FError(), FSeek( h, 0, FS_END ), FError()
   ? "create:", FCreate( "files_tables.dbf" ), FError(), "exclusive:", FOpen( "files_tables.dbf", FO_EXCLUSIVE ), ;
     FError(), "deny read:", FOpen( "files_tables.dbf", FO_DENYREAD ), FError(), "deny write:", ;
     FOpen( "files_tables.dbf", FO_DENYWRITE ), FError()
   h := FOpen( "files_tables.dbf", FO_DENYNONE )
   ? "deny none:", h != -1, FClose( h ), "table kept:", LastRec(), N

   USE files_tables SHARED ALIAS tb NEW
   ? "other area locks the record:", RLock(), "the file:", FLock()
   CLOSE tb
   SELECT ta
   ? "own lock kept:", AScan( dbRLockList(), 1 ) > 0
   CLOSE ta
   FErase( "files_tables.dbf" )
RETURN
