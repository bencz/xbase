// Orders and the blocks they run: a key that fails while REINDEX builds the order again closes that order (the area
// goes on in record order); a FOR block that closes the area while a record changes stops the change; a key block
// that would close or open orders while it runs is "Operation not supported"; the handler of an error of a shared
// order's file closes every area. The reference fails or reads freed memory in these cases (docs/rdd.md). The files
// start with "rdd_index_blocks", deleted at the end.
FIELD K
STATIC lFail := .F., lClose := .F., lClear := .F.

PROCEDURE Main()
   LOCAL c
   ErrorBlock( {| e | QOut( "ERR", e:subSystem, e:genCode, e:subCode, e:description, "[" + e:operation + "]" ), ;
      Break( e ) } )
   dbCreate( "rdd_index_blocks", { { "K", "C", 4, 0 } } )
   USE rdd_index_blocks NEW
   FOR EACH c IN { "d", "b", "a", "c" }
      APPEND BLANK
      REPLACE K WITH c
   NEXT
   INDEX ON KeyOf() TO rdd_index_blocks_k
   INDEX ON K TO rdd_index_blocks_f FOR Keep() ADDITIVE
   SET ORDER TO 1
   lFail := .T.
   BEGIN SEQUENCE
      REINDEX
   END SEQUENCE
   lFail := .F.
   ? "after a failed reindex:", IndexOrd(), ordName( 1 ), ordName( 2 )
   Walk()
   SET ORDER TO 1
   GO 2
   lClose := .T.
   BEGIN SEQUENCE
      REPLACE K WITH "z"
   END SEQUENCE
   lClose := .F.
   ? "closed by a FOR block:", Used(), Select()
   USE rdd_index_blocks NEW
   SET INDEX TO rdd_index_blocks_k
   GO 1
   lClear := .T.
   BEGIN SEQUENCE
      REPLACE K WITH "y"
   END SEQUENCE
   lClear := .F.
   ? "orders kept:", IndexOrd(), ordName( 1 )
   CLOSE
   FErase( "rdd_index_blocks.dbf" )
   FErase( "rdd_index_blocks_k.ntx" )
   FErase( "rdd_index_blocks_f.ntx" )
   Damaged()
RETURN

// A shared order whose file another program damaged: the change of a key fails, and the error's handler, which runs
// once the file's lock is let go, may close every area.
STATIC PROCEDURE Damaged()
   LOCAL h
   dbCreate( "rdd_index_blocks_d", { { "K", "C", 4, 0 } } )
   USE rdd_index_blocks_d EXCLUSIVE NEW
   APPEND BLANK
   REPLACE K WITH "a"
   INDEX ON K TO rdd_index_blocks_d
   CLOSE
   USE rdd_index_blocks_d SHARED NEW INDEX rdd_index_blocks_d
   RLock()
   REPLACE K WITH "b"
   dbCommit()
   h := FOpen( "rdd_index_blocks_d.ntx", 2 )
   FSeek( h, 2 )
   FWrite( h, Chr( 99 ) + Chr( 0 ) + Chr( 3 ) + Chr( 0 ) + Chr( 0 ) + Chr( 0 ) ) // a new counter; a root that is none
   FClose( h )
   BEGIN SEQUENCE WITH {| e | QOut( "handler:", e:subCode ), dbCloseAll(), Break( e ) }
      REPLACE K WITH "c"
      dbCommit()
   END SEQUENCE
   ? "after:", Used()
   FErase( "rdd_index_blocks_d.dbf" )
   FErase( "rdd_index_blocks_d.ntx" )
RETURN

FUNCTION KeyOf()
   IF lFail
      Break( "failed" )
   ENDIF
   IF lClear
      ordListClear()
   ENDIF
RETURN K

FUNCTION Keep()
   IF lClose
      dbCloseArea()
   ENDIF
RETURN .T.

// The records in the controlling order.
STATIC PROCEDURE Walk()
   GO TOP
   DO WHILE ! Eof()
      ?? " " + LTrim( Str( RecNo() ) ) + Trim( K )
      SKIP
   ENDDO
RETURN
