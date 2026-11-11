// Orders and the blocks they run: a key that fails while REINDEX builds the order again closes that order (the area
// goes on in record order); a FOR block that closes the area while a record changes stops the change; a key block
// that would close or open orders while it runs is "Operation not supported". The reference fails or reads freed
// memory in these cases (docs/rdd.md). The files start with "rdd_index_blocks", deleted at the end.
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
