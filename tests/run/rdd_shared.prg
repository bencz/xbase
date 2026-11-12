// Shared tables: two areas on one table lock each other out as two programs do (record and file locks, the file lock
// releasing the area's record locks first); RLock() keeps one lock, dbRLock() adds; dbRLockList() in the order taken
// (empty for an exclusive table); a change is seen by the other area once written (unlock); APPEND BLANK locks the
// new record; "Lock required", "Exclusive required" (PACK, ZAP); a sharing that the other users do not allow sets
// NetErr() under the default error handler; SET EXCLUSIVE; an index shared by two areas, changed by one and read by
// the other. The files start with "rdd_shared", deleted at the end.
FIELD K, N, F1, F2

PROCEDURE Main()
   Areas()
   Locks()
   Index()
   Fresh()
   AEval( Directory( "rdd_shared*.*" ), {| a | FErase( a[ 1 ] ) } )
RETURN

// What another area wrote is what a lock gives: the record read again once locked (no lost update), records read
// ahead dropped when the file lock goes, an index's cursor placed from the record's key as it is now (read again); a
// table opened and closed many times while another area keeps it open; locks of a shared READONLY table; dbRLock()
// with no record.
STATIC PROCEDURE Fresh()
   LOCAL i, c
   dbCreate( "rdd_shared_r", { { "F1", "C", 3, 0 }, { "F2", "C", 3, 0 } } )
   USE rdd_shared_r EXCLUSIVE NEW
   FOR i := 1 TO 5
      APPEND BLANK
      REPLACE F1 WITH "o" + Str( i, 1 ), F2 WITH "p" + Str( i, 1 )
   NEXT
   CLOSE
   USE rdd_shared_r SHARED ALIAS xa NEW
   USE rdd_shared_r SHARED ALIAS xb NEW
   xa->( dbGoTo( 1 ) )
   ? "xa reads:", xa->F1, xa->F2
   xb->( dbGoTo( 1 ) )
   xb->( RLock() )
   REPLACE xb->F2 WITH "B"
   xb->( dbUnlock() )
   xa->( RLock() )
   REPLACE xa->F1 WITH "A"
   xa->( dbUnlock() )
   xb->( dbGoTo( 1 ) )
   ? "no lost update:", "[" + xb->F1 + "][" + xb->F2 + "]"
   xa->( FLock() )
   xa->( dbGoTop() )
   c := ""
   DO WHILE ! xa->( Eof() )
      c += xa->F1
      xa->( dbSkip() )
   ENDDO
   xa->( dbUnlock() )
   xb->( dbGoTo( 3 ) )
   xb->( RLock() )
   REPLACE xb->F1 WITH "new"
   xb->( dbUnlock() )
   xa->( dbGoTo( 3 ) )
   ? "after the file lock went:", xa->F1
   xa->( dbGoTo( 1 ) )
   ? "dbRLock():", xa->( RLock() ), Len( xa->( dbRLockList() ) ), xa->( dbRLock() ), Len( xa->( dbRLockList() ) ), ;
      xa->( dbRLock( 3 ) ), xa->( dbRLock( 4 ) ), Len( xa->( dbRLockList() ) )
   xa->( dbUnlock() )
   FOR i := 1 TO 3000
      USE rdd_shared_r SHARED ALIAS xc NEW
      IF ! Used()
         EXIT
      ENDIF
      CLOSE xc
   NEXT
   ? "openings while another area keeps it open:", i
   CLOSE ALL
   USE rdd_shared_r SHARED READONLY ALIAS xr NEW
   ? "readonly shared:", RLock(), FLock(), Len( dbRLockList() )
   CLOSE ALL
   dbCreate( "rdd_shared_k", { { "K", "C", 3, 0 } } )
   USE rdd_shared_k EXCLUSIVE NEW
   FOR i := 1 TO 10
      APPEND BLANK
      REPLACE K WITH "A" + StrZero( i, 2 )
   NEXT
   INDEX ON K TO rdd_shared_kk
   CLOSE
   USE rdd_shared_k SHARED ALIAS ka NEW INDEX rdd_shared_kk
   USE rdd_shared_k SHARED ALIAS kb NEW INDEX rdd_shared_kk
   ka->( dbGoTo( 5 ) )
   kb->( dbGoTo( 5 ) )
   kb->( RLock() )
   REPLACE kb->K WITH "Z05"
   kb->( dbUnlock() )
   ka->( dbSkip() )
   ? "skip after the key moved:", ka->( RecNo() ), ka->( Eof() )
   ka->( dbGoTo( 5 ) )
   ka->( dbSkip( -1 ) )
   ? "skip back after the key moved:", ka->( RecNo() )
   kb->( dbGoTo( 3 ) )
   kb->( RLock() )
   REPLACE kb->K WITH "Z03"
   kb->( dbUnlock() )
   ka->( dbGoTo( 3 ) )
   ? "ordKeyNo after the key moved:", ka->( ordKeyNo() )
   ka->( dbGoTo( 7 ) )
   ? "read before the key moved:", ka->K
   kb->( dbGoTo( 7 ) )
   kb->( RLock() )
   REPLACE kb->K WITH "Z07"
   kb->( dbUnlock() )
   ka->( dbSkip() )
   ? "skip from a record read before:", ka->( RecNo() ), ka->( Eof() )
   CLOSE ALL
   AEval( Directory( "rdd_shared_*.*" ), {| a | FErase( a[ 1 ] ) } )
RETURN

STATIC PROCEDURE Areas()
   dbCreate( "rdd_shared", { { "K", "C", 10, 0 }, { "M", "M", 10, 0 } } )
   USE rdd_shared EXCLUSIVE NEW
   APPEND BLANK
   APPEND BLANK
   CLOSE
   USE rdd_shared SHARED ALIAS xa NEW
   USE rdd_shared SHARED ALIAS xb NEW
   ? "xa rlock:", xa->( RLock() ), "xb rlock same:", xb->( RLock() ), "xb rlock 2:", xb->( dbRLock( 2 ) )
   ? "xa flock:", xa->( FLock() ), "xa list:", Len( xa->( dbRLockList() ) )
   xa->( dbUnlock() )
   ? "xb flock after:", xb->( FLock() ), xa->( RLock() ), xa->( FLock() )
   xb->( dbUnlock() )
   xa->( dbGoTo( 1 ) )
   ? "xa rlock 1:", xa->( RLock() )
   REPLACE xa->K WITH "one", xa->M WITH Replicate( "m", 600 )
   ? "xb sees before unlock:", xb->( dbGoTo( 1 ) ), "[" + xb->K + "]"
   xa->( dbUnlock() )
   xb->( dbGoTo( 1 ) )
   ? "xb sees after unlock:", "[" + xb->K + "]", Len( xb->M )
   ? "xb lastrec:", xb->( LastRec() )
   xa->( dbAppend() )
   ? "xa append:", xa->( RecNo() ), xa->( LastRec() ), xb->( LastRec() ), NetErr()
   ? "xb rlock 3:", xb->( dbRLock( 3 ) ), xa->( dbRLockList() )[ 1 ]
   xa->( dbUnlock() )
   USE rdd_shared EXCLUSIVE ALIAS xc NEW
   ? "exclusive while shared:", NetErr(), Used(), Alias()
   CLOSE xa
   CLOSE xb
   USE rdd_shared EXCLUSIVE ALIAS xc NEW
   ? "exclusive alone:", NetErr(), Used(), RLock(), Len( dbRLockList() )
   USE rdd_shared SHARED ALIAS xd NEW
   ? "shared while exclusive:", NetErr(), Used()
   CLOSE ALL
   SET EXCLUSIVE OFF
   USE rdd_shared NEW
   USE rdd_shared ALIAS second NEW
   ? "set exclusive off:", NetErr(), Used(), Set( _SET_EXCLUSIVE )
   CLOSE ALL
RETURN

STATIC PROCEDURE Locks()
   ErrorBlock( {| e | QOut( "ERR", e:subSystem, e:genCode, e:subCode, e:description, "[" + e:operation + "]", ;
      e:canDefault ), Break( e ) } )
   dbCreate( "rdd_shared_o", { { "K", "C", 10, 0 } } )
   USE rdd_shared_o EXCLUSIVE NEW
   APPEND BLANK
   APPEND BLANK
   ? "excl rlock:", RLock(), dbRLock( 2 ), Len( dbRLockList() ), FLock(), Len( dbRLockList() )
   CLOSE
   USE rdd_shared_o SHARED NEW
   ? "list:", Len( dbRLockList() )
   GO 2
   ? "rlock 2:", RLock(), dbRLock( 1 ), Len( dbRLockList() ), dbRLockList()[ 1 ], dbRLockList()[ 2 ]
   ? "rlock again:", RLock(), Len( dbRLockList() )
   dbRUnlock( 2 )
   ? "after runlock 2:", Len( dbRLockList() )
   GO BOTTOM
   SKIP
   ? "eof rlock:", RecNo(), RLock(), Len( dbRLockList() )
   ? "rlock 99:", dbRLock( 99 ), Len( dbRLockList() )
   ? "flock:", FLock(), Len( dbRLockList() ), RLock(), dbRLock( 1 ), Len( dbRLockList() )
   QOut( "--replace with flock" )
   GO 1
   REPLACE K WITH "a"
   QOut( "--unlock" )
   UNLOCK
   QOut( "--pack" )
   BEGIN SEQUENCE
      PACK
   END SEQUENCE
   BEGIN SEQUENCE
      ZAP
   END SEQUENCE
   QOut( "--append" )
   APPEND BLANK
   ? "append:", RecNo(), Len( dbRLockList() ), NetErr()
   QOut( "--close" )
   CLOSE
RETURN

STATIC PROCEDURE Index()
   LOCAL i
   dbCreate( "rdd_shared_i", { { "K", "C", 4, 0 }, { "N", "N", 4, 0 } } )
   USE rdd_shared_i EXCLUSIVE NEW
   FOR i := 1 TO 50
      APPEND BLANK
      REPLACE K WITH StrZero( i * 7 % 100, 4 ), N WITH i
   NEXT
   INDEX ON K TO rdd_shared_ik
   CLOSE
   USE rdd_shared_i SHARED ALIAS ia NEW INDEX rdd_shared_ik
   USE rdd_shared_i SHARED ALIAS ib NEW INDEX rdd_shared_ik
   ia->( dbGoTo( 10 ) )
   ia->( RLock() )
   REPLACE ia->K WITH "ZZZZ"
   ? "ib seek before unlock:", ib->( dbSeek( "ZZZZ" ) )
   ia->( dbUnlock() )
   ? "ib seek after unlock:", ib->( dbSeek( "ZZZZ" ) ), ib->( RecNo() ), ib->( ordKeyNo() ), ib->( ordKeyCount() )
   ib->( dbGoTop() )
   ? "ib top:", ib->K, ib->( RecNo() )
   ia->( dbAppend() )
   REPLACE ia->K WITH "0000", ia->N WITH 99
   ia->( dbUnlock() )
   ib->( dbSkip( 2 ) )
   ? "ib skip after other append:", ib->K, ib->( RecNo() )
   ib->( dbGoTop() )
   ? "ib top now:", ib->K, ib->( RecNo() ), ib->( ordKeyCount() ), ib->( LastRec() )
   ? "reindex shared:"
   BEGIN SEQUENCE
      ib->( ordListRebuild() )
   END SEQUENCE
   CLOSE ALL
RETURN
