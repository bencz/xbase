// Relations (SET RELATION, dbSetRelation, ordSetRelation): a child follows its parent by a SEEK in its controlling
// order (no soft seek; Found() tells) or, without an order, by record number; at the parent's end it is at its end.
// It moves lazily: the expression runs once, at the child's next use (a filter of the parent naming the child, a
// grandchild read first), with the parent's record as it is then; GO, GO TOP, GO BOTTOM, SEEK and APPEND in the child
// drop the pending move, a SKIP starts from it, clearing the relation or closing the parent makes it. ADDITIVE, a
// chain, two parents into one child, SCOPED (the child's scope is the key, cleared with the relation), SET DELETED
// in the child, a key of another type; dbRelation(), dbRSelect(); closing the child drops the relation; the errors;
// expressions that undo what the move stands on.
// Deliberate differences: dbRelation() of a relation set without text is "" (the reference gives NIL); an error in
// the expression leaves the area current before it current (the reference leaves the parent's); a relation that
// leads back to its parent is DBCMD/1013 (the reference recurses until it crashes); one set from or into a table
// being closed is DBCMD/1004; the hostile expressions do not crash xbase as they do the reference. The files start
// with "rdd_relation_", deleted at the end.
REQUEST DBFCDX
FIELD ID, NAME, CID, AMT, CODE, NOTE, K

STATIC s_nEval := 0, s_lHook := .F.

PROCEDURE Main()
   AEval( Directory( "rdd_relation_*.*" ), {| a | FErase( a[ 1 ] ) } ) // left by a run that failed
   Basic()
   More()
   Hostile()
   AEval( Directory( "rdd_relation_*.*" ), {| a | FErase( a[ 1 ] ) } )
RETURN

STATIC PROCEDURE Basic()
   LOCAL i, c
   dbCreate( "rdd_relation_cust", { { "CID", "N", 3, 0 }, { "NAME", "C", 8, 0 }, { "CODE", "C", 2, 0 } } )
   USE rdd_relation_cust EXCLUSIVE NEW ALIAS cust
   FOR i := 1 TO 5
      APPEND BLANK
      REPLACE CID WITH i * 10, NAME WITH "cust" + Str( i, 1 ), CODE WITH Chr( 64 + i ) + "x"
   NEXT
   INDEX ON CID TO rdd_relation_cust
   CLOSE
   dbCreate( "rdd_relation_ord", { { "ID", "N", 3, 0 }, { "CID", "N", 3, 0 }, { "AMT", "N", 6, 2 } } )
   USE rdd_relation_ord EXCLUSIVE NEW ALIAS ord
   FOR i := 1 TO 8
      APPEND BLANK
      REPLACE ID WITH i, CID WITH { 10, 20, 10, 60, 30, 50, 20, 10 }[ i ], AMT WITH i * 1.5
   NEXT
   INDEX ON CID TO rdd_relation_ordc
   CLOSE
   dbCreate( "rdd_relation_note", { { "CODE", "C", 2, 0 }, { "NOTE", "C", 6, 0 } } )
   USE rdd_relation_note EXCLUSIVE NEW ALIAS note
   FOR i := 1 TO 3
      APPEND BLANK
      REPLACE CODE WITH Chr( 64 + i * 2 - 1 ) + "x", NOTE WITH "note" + Str( i, 1 )
   NEXT
   INDEX ON CODE TO rdd_relation_note
   CLOSE

   USE rdd_relation_cust NEW INDEX rdd_relation_cust ALIAS cust
   USE rdd_relation_note NEW INDEX rdd_relation_note ALIAS note
   USE rdd_relation_ord NEW ALIAS ord
   SET RELATION TO CID INTO cust
   ? "rel:", dbRelation(), dbRelation( 1 ), "[" + dbRelation( 2 ) + "]", dbRSelect(), dbRSelect( 1 ), dbRSelect( 2 ), ;
      dbRSelect( 0 ), Select( "cust" )
   GO TOP
   DO WHILE ! Eof()
      ? ID, CID, cust->NAME, cust->( RecNo() ), cust->( Found() ), cust->( Eof() )
      SKIP
   ENDDO
   ? "eof parent:", Eof(), cust->( RecNo() ), cust->( Eof() ), cust->( Bof() ), cust->( Found() )
   // chain: ord -> cust -> note
   SELECT cust
   SET RELATION TO CODE INTO note
   SELECT ord
   GO TOP
   DO WHILE ! Eof()
      ?? " " + note->NOTE + "/" + LTrim( Str( note->( RecNo() ) ) )
      SKIP
   ENDDO
   ? "grandchild direct:"
   GO 5
   ?? note->( RecNo() ), note->NOTE, cust->NAME
   // additive and record-number relation (no order in child)
   SELECT note
   SET INDEX TO
   SELECT ord
   SET RELATION TO ID INTO note ADDITIVE
   ? "two:", dbRelation( 1 ), dbRelation( 2 ), dbRSelect( 2 )
   GO 2
   ? "by recno:", note->( RecNo() ), note->NOTE, note->( Found() ), note->( Eof() )
   GO 6
   ? "by recno past:", note->( RecNo() ), note->( Found() ), note->( Eof() )
   SET RELATION TO
   ? "cleared:", "[" + dbRelation() + "]", dbRSelect()
   // filter on the parent naming the child
   SET RELATION TO CID INTO cust
   SET FILTER TO cust->CODE >= "C"
   c := ""
   GO TOP
   DO WHILE ! Eof()
      c += LTrim( Str( ID ) ) + " "
      SKIP
   ENDDO
   ? "filter through child:", c
   SET FILTER TO
   // lazy: how often the relation runs
   SET RELATION TO Counted( CID ) INTO cust
   s_nEval := 0
   GO TOP
   SKIP 3
   ? "evals without reading the child:", s_nEval
   ? cust->NAME, s_nEval, cust->NAME, s_nEval
   GO BOTTOM
   ? "after go bottom:", s_nEval, cust->( RecNo() ), s_nEval
   // the child moved by hand, then the parent moves
   GO 1
   cust->( dbGoBottom() )
   ? "child moved:", cust->( RecNo() ), s_nEval
   SKIP
   ? "parent moved:", cust->( RecNo() ), s_nEval
   // a change of the parent's key before the child is read
   GO 3
   REPLACE CID WITH 40
   ? "key changed:", cust->NAME, s_nEval
   REPLACE CID WITH 10
   ? "key changed again:", cust->NAME
   APPEND BLANK
   ? "append:", RecNo(), cust->( RecNo() ), cust->( Eof() ), s_nEval
   REPLACE CID WITH 30
   ? "after replace:", cust->NAME
   dbDelete()
   SET RELATION TO
   // scoped
   SELECT cust
   SET RELATION TO
   SELECT ord
   SET INDEX TO rdd_relation_ordc
   SELECT cust
   SET RELATION TO CID INTO ord SCOPED
   ? "scoped:", dbRelation(), dbRSelect()
   GO TOP
   DO WHILE ! Eof()
      c := ""
      SELECT ord
      DO WHILE ! Eof()
         c += LTrim( Str( ID ) ) + " "
         SKIP
      ENDDO
      SELECT cust
      ? CID, c, ord->( ordScope( 0 ) ), ord->( ordScope( 1 ) )
      SKIP
   ENDDO
   SET RELATION TO
   ? "scope after clear:", ord->( ordScope( 0 ) ), ord->( ordScope( 1 ) )
   ordSetRelation( "ord", {|| CID }, "CID" )
   GO 2
   ? "ordSetRelation:", dbRelation(), ord->( ordScope( 0 ) ), ord->ID
   // closing the child drops the relation
   CLOSE ord
   ? "child closed:", "[" + dbRelation() + "]", dbRSelect()
   // errors
   ErrorBlock( {| e | QOut( "ERR", e:subSystem, e:genCode, e:subCode, e:description, e:canRetry, e:canDefault ), ;
      Break( e ) } )
   BEGIN SEQUENCE
      dbSetRelation( "cust", {|| CID }, "CID" )
   END SEQUENCE
   BEGIN SEQUENCE
      dbSetRelation( "note" )
   END SEQUENCE
   BEGIN SEQUENCE
      dbSetRelation( 99, {|| 1 }, "1" )
   END SEQUENCE
   BEGIN SEQUENCE
      dbSetRelation( "nosuch", {|| 1 }, "1" )
   END SEQUENCE
   BEGIN SEQUENCE
      dbSetRelation( 2, {|| 1 } )
      ? "no text:", "[" + dbRelation() + "]", dbRSelect()
   END SEQUENCE
   SET RELATION TO
   SET RELATION TO CODE INTO note
   BEGIN SEQUENCE
      SET RELATION TO 1 + "a" INTO note
      ? "set"
      ? note->( RecNo() )
   END SEQUENCE
   ? "after error:", dbRelation(), dbRSelect()
   SELECT 9
   ? "unused:", "[" + dbRelation() + "]", dbRSelect()
   BEGIN SEQUENCE
      dbSetRelation( "note", {|| 1 }, "1" )
   END SEQUENCE
   BEGIN SEQUENCE
      dbClearRelation()
      ? "clear unused ok"
   END SEQUENCE
   CLOSE ALL
RETURN

STATIC PROCEDURE More()
   LOCAL i, c, a
   dbCreate( "rdd_relation_mc", { { "CID", "N", 3, 0 }, { "NAME", "C", 6, 0 } }, "DBFCDX" )
   USE rdd_relation_mc EXCLUSIVE VIA "DBFCDX" NEW ALIAS mc
   FOR i := 1 TO 6
      APPEND BLANK
      REPLACE CID WITH i, NAME WITH "c" + Str( i, 1 )
   NEXT
   INDEX ON CID TAG CID
   GO 2
   DELETE
   CLOSE
   dbCreate( "rdd_relation_mp", { { "ID", "N", 3, 0 }, { "CID", "N", 3, 0 }, { "K", "C", 3, 0 } } )
   USE rdd_relation_mp EXCLUSIVE NEW ALIAS mp
   FOR i := 1 TO 6
      APPEND BLANK
      REPLACE ID WITH i, CID WITH 7 - i, K WITH Str( i, 3 )
   NEXT
   CLOSE
   USE rdd_relation_mc VIA "DBFCDX" SHARED NEW ALIAS mc
   USE rdd_relation_mp SHARED NEW ALIAS mp
   USE rdd_relation_mp SHARED NEW ALIAS p2
   SELECT mp
   mc->( ordSetFocus( "CID" ) )
   SET DELETED ON
   SET RELATION TO CID INTO mc
   c := ""
   dbEval( {|| c += mc->NAME + iif( mc->( Found() ), "+", "-" ) + " " } )
   ? "deleted skipped:", c
   SET DELETED OFF
   GO 5
   ? "deleted shown:", mc->NAME, mc->( Deleted() )
   // the clear makes the pending move
   GO 3
   SET RELATION TO
   ? "cleared with a move pending:", mc->( RecNo() )
   // a key of another type
   SET RELATION TO K INTO mc
   GO 1
   ? "other type:", mc->( RecNo() ), mc->( Eof() ), mc->( Found() )
   // two parents into one child: the last that moved
   SET RELATION TO CID INTO mc
   SELECT p2
   SET RELATION TO ID INTO mc
   GO 2
   SELECT mp
   GO 1
   ? "two parents:", mc->( RecNo() )
   p2->( dbGoTo( 4 ) )
   ? "two parents:", mc->( RecNo() )
   // skip in the child starts where the relation put it
   GO 2
   mc->( dbSkip() )
   ? "child skip:", mc->( RecNo() )
   // a write in the child goes to the related record
   GO 6
   mc->( RLock() )
   mc->NAME := "first"
   ? "child write:", mc->( RecNo() ), mc->NAME
   // a cycle is refused
   ErrorBlock( {| e | QOut( "ERR", e:subSystem, e:genCode, e:subCode, e:description ), Break( e ) } )
   SELECT mc
   BEGIN SEQUENCE
      SET RELATION TO CID INTO p2
      ? "cycle accepted"
   END SEQUENCE
   ? "relations:", dbRSelect(), p2->( dbRSelect() )
   // garbage made while relations hold their blocks
   SELECT mp
   SET RELATION TO Counted( CID ) INTO mc
   FOR i := 1 TO 200000
      a := { i, Str( i ) }
   NEXT
   GO 4
   ? "after collecting:", mc->NAME, s_nEval
   // closing the parent makes the pending move; closing the child drops the relation
   GO 5
   CLOSE mp
   ? "parent closed:", mc->( RecNo() ), s_nEval, Select( "mp" )
   SELECT p2
   CLOSE mc
   ? "child closed:", dbRSelect(), "[" + dbRelation() + "]"
   CLOSE ALL
RETURN

// The child's order changes before its move is made (the move uses the order it has then; Used(), Alias() and
// IndexOrd() do not make it); a SEEK of a value of another type drops the pending move. Then expressions that undo
// what the move stands on: a grandparent's that clears its child's relations, a scoped one that drops the child's
// orders while it SEEKs, one that moves its own parent, one that removes its own scoped relation, an order's key that
// sets a relation from or into a table being closed.
STATIC PROCEDURE Hostile()
   LOCAL i
   dbCreate( "rdd_relation_hb", { { "ID", "N", 3, 0 }, { "K", "N", 3, 0 }, { "NAME", "C", 4, 0 } } )
   USE rdd_relation_hb EXCLUSIVE NEW ALIAS hb
   FOR i := 1 TO 4
      APPEND BLANK
      REPLACE ID WITH i, K WITH 5 - i, NAME WITH Chr( 64 + i )
   NEXT
   INDEX ON ID TO rdd_relation_h1
   INDEX ON K TO rdd_relation_h2
   CLOSE
   USE rdd_relation_hb NEW ALIAS bp SHARED
   USE rdd_relation_hb NEW ALIAS bc INDEX rdd_relation_h1, rdd_relation_h2 SHARED
   SELECT bp
   SET RELATION TO K INTO bc
   GO 2
   SELECT bc
   ? "used:", Used(), Alias(), IndexOrd()
   SET ORDER TO 2
   ? "order changed first:", RecNo(), Found(), NAME
   SELECT bp
   GO 1
   bc->( dbSeek( "x" ) )
   ? "seek of another type:", bc->( Eof() ), bc->( RecNo() )
   CLOSE ALL
   // G -> P -> C, G's expression clears P's relations
   USE rdd_relation_hb NEW ALIAS gp SHARED
   USE rdd_relation_hb NEW ALIAS pp SHARED
   USE rdd_relation_hb NEW ALIAS pc SHARED
   SELECT pp
   SET RELATION TO ID INTO pc
   SELECT gp
   SET RELATION TO ClearChild() INTO pp
   GO 2
   ? "relation cleared on the way:", pc->ID, pp->( dbRSelect() ), pp->( RecNo() )
   CLOSE ALL
   // a scoped expression that drops the child's orders, while the child SEEKs
   USE rdd_relation_hb NEW ALIAS fp SHARED
   USE rdd_relation_hb NEW ALIAS fc INDEX rdd_relation_h1 SHARED
   SELECT fp
   SET RELATION TO DropOrders() INTO fc SCOPED
   GO 1
   BEGIN SEQUENCE
      fc->( dbSeek( 2 ) )
   END SEQUENCE
   ? "orders dropped while seeking:", fc->( IndexOrd() ), fc->( RecNo() )
   // an expression that moves its own parent; one that removes its own scoped relation
   SET RELATION TO ( dbSkip( 0 ), ID ) INTO fc SCOPED
   fc->( dbSetIndex( "rdd_relation_h1" ) )
   GO 3
   ? "parent moved by the expression:", fc->( RecNo() ), fc->( ordScope( 0 ) )
   SET RELATION TO ( dbClearRelation(), ID ) INTO fc SCOPED
   GO 2
   ? "scoped relation removed by its expression:", fc->( RecNo() ), fc->( ordScope( 0 ) ), dbRSelect()
   CLOSE ALL
   // an order's key that sets relations from and into tables being closed
   USE rdd_relation_hb NEW ALIAS kc SHARED
   USE rdd_relation_hb NEW ALIAS kp SHARED
   INDEX ON Hook() TO rdd_relation_h3
   RLock()
   ErrorBlock( {| e | QOut( "ERR", e:subSystem, e:genCode, e:subCode, e:description ), Break( e ) } )
   s_lHook := .T.
   REPLACE ID WITH 9
   CLOSE kp
   s_lHook := .F.
   ? "closed:", Select( "kp" ), kc->( RecNo() ), kc->( dbRSelect() )
   CLOSE ALL
RETURN

STATIC FUNCTION ClearChild()
   pp->( dbClearRelation() )
RETURN ID

STATIC FUNCTION DropOrders()
   fc->( dbClearIndex() )
RETURN ID

STATIC FUNCTION Hook()
   IF s_lHook
      BEGIN SEQUENCE
         dbSetRelation( "kc", {|| ID }, "ID" )
      END SEQUENCE
      BEGIN SEQUENCE
         kc->( dbSetRelation( "kp", {|| ID }, "ID" ) )
      END SEQUENCE
   ENDIF
RETURN ID

STATIC FUNCTION Counted( x )
   ++s_nEval
RETURN x
