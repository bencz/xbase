// Blocks that run while orders are built or navigated: a REINDEX EVAL block that changes the records' keys or moves
// the record (the records taken are the same; the keys are those the records had when taken); one that would open an
// order is "Operation not supported", and the order it was building is closed; a scope codeblock that fails leaves the
// bound as it was; one that closes the area, when set or while SKIP evaluates it again, ends there. The files start
// with "rdd_iev", deleted at the end.
FIELD K, N
PROCEDURE Main()
   LOCAL c, i := 0
   ErrorBlock( {| e | QOut( "ERR", e:subSystem, e:genCode, e:subCode, e:description, "[" + e:operation + "]" ), ;
      Break( e ) } )
   dbCreate( "rdd_iev", { { "K", "C", 2, 0 }, { "N", "N", 4, 0 } } )
   USE rdd_iev NEW
   FOR EACH c IN { "b", "a", "c", "b", "a" }
      APPEND BLANK
      REPLACE K WITH c, N WITH ++i
   NEXT
   INDEX ON K TO rdd_iev_k
   INDEX ON N TO rdd_iev_n ADDITIVE
   SET ORDER TO 1
   REINDEX EVAL {|| FIELD->K := "z", .T. }
   ? "reindex eval changing keys:", IndexOrd(), ordKeyCount( 1 ), ordKeyCount( 2 )
   GO TOP
   ? "first:", RecNo(), K
   REINDEX EVAL {|| dbSkip(), ordKeyCount(), IndexOrd(), .T. }
   ? "reindex eval navigating:", IndexOrd(), ordKeyCount( 1 ), ordKeyCount( 2 )
   BEGIN SEQUENCE
      REINDEX EVAL {|| ordListAdd( "rdd_iev_n" ) }
   END SEQUENCE
   ? "orders after a failed rebuild:", IndexOrd(), ordName( 1 ), ordName( 2 )
   SET INDEX TO rdd_iev_k
   ordScope( 0, "b" )
   BEGIN SEQUENCE
      ordScope( 0, {|| 1 / "x" } )
   END SEQUENCE
   ? "after scope error:", ValType( ordScope( 0 ) ), ordKeyCount()
   ordScope( 0, NIL )
   ? "scope closing the area:", ordScope( 0, {|| dbCloseArea(), "a" } ), Used()
   USE rdd_iev NEW INDEX rdd_iev_k
   ordScope( 0, {|| iif( N == 3, dbCloseArea(), NIL ), "a" } )
   GO TOP
   DO WHILE Used() .AND. ! Eof()
      SKIP
   ENDDO
   ? "closed by a scope block while skipping:", Used()
   CLOSE ALL
   AEval( { "rdd_iev.dbf", "rdd_iev_k.ntx", "rdd_iev_n.ntx" }, {| f | FErase( f ) } )
RETURN
