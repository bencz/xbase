// INDEX ON's clauses: WHILE, NEXT and REST start at the current record (ordCondSet()'s start record when it gives one)
// and RECORD takes one record; USECURRENT starts at the controlling order's first record in its scope; the records are
// walked in the controlling order (the record order without one), raw: the filter and SET DELETED do not apply, and a
// record the order has no entry for ends the walk. EVAL runs after the first record and then after every EVERY more,
// and stops the build, keeping its records, when it returns anything but .T.. ADDITIVE keeps the orders open; without
// it they close after the build. Equal keys keep the order of the walk. ordKeyVal() gives the key with the key's type,
// width and decimals; ordSkipUnique() moves by keys. The files start with "rdd_icl", deleted at the end.
FIELD K, N, D

PROCEDURE Main()
   LOCAL c, i, nC

   i := 0
   nC := 0
   ? "-- functions"
   dbCreate( "rdd_icl", { { "K", "C", 2, 0 }, { "N", "N", 6, 2 }, { "D", "D", 8, 0 } } )
   USE rdd_icl NEW
   FOR EACH c IN { "b", "a", "c", "b", "a", "b", "c", "d" }
      APPEND BLANK
      REPLACE K WITH c, N WITH ( ++i ) * 1.5 - 5, D WITH SToD( "2024010" + Str( i, 1 ) )
   NEXT
   INDEX ON K TO rdd_icl_k
   Walk( "k:" )
   GO TOP
   ?? Chr( 10 ) + "unique fwd:"
   DO WHILE ! Eof()
      ?? " " + LTrim( Str( RecNo() ) )
      ordSkipUnique()
   ENDDO
   GO BOTTOM
   ?? Chr( 10 ) + "unique back:"
   DO WHILE ! Bof()
      ?? " " + LTrim( Str( RecNo() ) )
      ordSkipUnique( -1 )
   ENDDO
   ? "at bof:", RecNo(), Bof()
   INDEX ON N TO rdd_icl_n
   GO 3
   ? "keyval n:", ordKeyVal(), Str( ordKeyVal() ), ValType( ordKeyVal() )
   INDEX ON D TO rdd_icl_d
   GO 3
   ? "keyval d:", DToS( ordKeyVal() ), ValType( ordKeyVal() )
   INDEX ON K TO rdd_icl_f FOR K == "a"
   GO 1
   ? "keyval out:", ordKeyVal(), ordKeyNo()
   GO 4
   INDEX ON K TO rdd_icl_w WHILE N < 5
   Walk( "while from 4:" )
   GO 3
   INDEX ON K TO rdd_icl_nx NEXT 3
   Walk( "next 3 from 3:" )
   GO 6
   INDEX ON K TO rdd_icl_r REST
   Walk( "rest from 6:" )
   INDEX ON K TO rdd_icl_rec RECORD 5
   Walk( "record 5:" )
   INDEX ON K TO rdd_icl_e EVAL {|| nC++, .T. } EVERY 3
   ? "eval calls:", nC
   nC := 0
   INDEX ON K TO rdd_icl_e2 EVAL {|| nC++, nC < 2 } EVERY 2
   ? "eval stop:", nC
   Walk( "stopped:" )
   SET INDEX TO rdd_icl_n
   GO 5
   INDEX ON K TO rdd_icl_uc FOR N > 0 USECURRENT
   Walk( "usecurrent:" )
   SET INDEX TO rdd_icl_n
   GO 4
   INDEX ON K TO rdd_icl_ucw WHILE N < 7 USECURRENT
   Walk( "usecurrent while from 4:" )
   CLOSE
   Cleanup()
   i := 0
   ? "-- walks"
   dbCreate( "rdd_icl", { { "K", "C", 2, 0 }, { "N", "N", 6, 2 } } )
   USE rdd_icl NEW
   FOR EACH c IN { "b", "a", "c", "b", "a", "b", "c", "d" }
      APPEND BLANK
      REPLACE K WITH c, N WITH ( ++i ) * 1.5 - 5
   NEXT
   GO 4
   INDEX ON K TO rdd_icl_w WHILE N < 5
   Walk( "while from 4:" )
   SET INDEX TO
   GO 3
   INDEX ON K TO rdd_icl_nx NEXT 3
   Walk( "next 3 from 3:" )
   SET INDEX TO
   GO 6
   INDEX ON K TO rdd_icl_r REST
   Walk( "rest from 6:" )
   SET INDEX TO
   GO 6
   INDEX ON K TO rdd_icl_rec RECORD 5
   Walk( "record 5:" )
   SET INDEX TO
   GO 6
   INDEX ON K TO rdd_icl_all ALL
   Walk( "all from 6:" )
   SET INDEX TO rdd_icl_all
   GO 4
   INDEX ON K TO rdd_icl_ctl NEXT 3
   Walk( "next 3 from 4 with order:" )
   SET INDEX TO rdd_icl_all
   GO 4
   INDEX ON K TO rdd_icl_ctl2 NEXT 3 USECURRENT
   Walk( "next 3 from 4 usecurrent:" )
   CLOSE
   Cleanup()
   i := 0
   nC := 0
   ? "-- raw and eval"
   dbCreate( "rdd_icl", { { "K", "C", 2, 0 }, { "N", "N", 4, 0 } } )
   USE rdd_icl NEW
   FOR EACH c IN { "b", "a", "c", "b", "a", "b", "c", "d", "e", "a" }
      APPEND BLANK
      REPLACE K WITH c, N WITH ++i
   NEXT
   SET DELETED ON
   GO 3
   DELETE
   SET FILTER TO N != 5
   GO 1
   INDEX ON K TO rdd_icl_raw WHILE N < 8
   SET FILTER TO
   Walk( "while raw:" )
   SET INDEX TO
   INDEX ON K TO rdd_icl_e EVAL {|| nC++, NIL }
   ? "eval nil:", nC
   Walk( "eval nil:" )
   SET INDEX TO
   INDEX ON K TO rdd_icl_rb RECORD 50
   Walk( "record 50:" )
   ? "count:", ordKeyCount(), LastRec()
   SET INDEX TO
   GO 2
   INDEX ON K TO rdd_icl_n0 NEXT 0
   Walk( "next 0:" )
   INDEX ON N TO rdd_icl_nn
   SET SCOPE TO 3, 7
   INDEX ON K TO rdd_icl_uc USECURRENT ADDITIVE
   Walk( "usecurrent scoped:" )
   ? "orders:", ordName( 1 ), ordName( 2 ), ordName( 3 ), IndexOrd()
   SET ORDER TO 2
   ? "scope kept:", ordScope( 0 ), ordScope( 1 )
   GO 9
   INDEX ON K TO rdd_icl_r2 REST USECURRENT
   Walk( "rest usecurrent from 9:" )
   ? "orders:", ordName( 1 ), ordName( 2 ), IndexOrd()
   SET INDEX TO
   GO 4
   ordCondSet( , , , , , , 6, 2 )
   ordCreate( "rdd_icl_cs", , "K" )
   Walk( "condset start 6 next 2:" )
   nC := 0
   INDEX ON K TO rdd_icl_ev EVAL {|| nC++, .T. } EVERY 4 NEXT 9
   ? "eval next:", nC
   nC := 0
   GO TOP
   INDEX ON K TO rdd_icl_ev2 EVAL {|| nC++, .T. } EVERY 0
   ? "every 0:", nC
   nC := 0
   INDEX ON K TO rdd_icl_ev3 EVAL {|| nC++, .T. } EVERY -2
   ? "every -2:", nC
   SET INDEX TO
   nC := 0
   GO 7
   INDEX ON K TO rdd_icl_ev4 EVAL {|| nC++, .T. } WHILE K != "e"
   ? "eval while:", nC
   Walk( "eval while:" )
   CLOSE
   Cleanup()
   i := 0
   ? "-- usecurrent"
   dbCreate( "rdd_icl", { { "K", "C", 2, 0 }, { "N", "N", 4, 0 } } )
   USE rdd_icl NEW
   FOR EACH c IN { "b", "a", "c", "b", "a", "b", "c", "d", "e", "a" }
      APPEND BLANK
      REPLACE K WITH c, N WITH ++i
   NEXT
   INDEX ON N TO rdd_icl_nn
   ? "keys:", ordKeyVal(), ordKeyCount()
   SET SCOPE TO 3, 7
   ? "scoped count:", ordKeyCount()
   GO TOP
   ? "top:", RecNo(), Eof()
   INDEX ON K TO rdd_icl_uc USECURRENT ADDITIVE
   ? "uc count:", ordKeyCount()
   SET ORDER TO 1
   SET SCOPE TO 1
   ? "scoped 1:", ordKeyCount()
   INDEX ON K TO rdd_icl_uc2 USECURRENT ADDITIVE
   ? "uc2 count:", ordKeyCount()
   SET ORDER TO 1
   SET SCOPE TO 50
   ? "empty scope:", ordKeyCount(), RecNo()
   GO 4
   INDEX ON K TO rdd_icl_uc3 USECURRENT ADDITIVE
   ? "uc3 count:", ordKeyCount(), RecNo()
   CLOSE
   Cleanup()
   Ties()
   Cleanup()
RETURN

STATIC PROCEDURE Cleanup()
   LOCAL a
   FErase( "rdd_icl.dbf" )
   FOR EACH a IN Directory( "rdd_icl_*.ntx" )
      FErase( a[ 1 ] )
   NEXT
RETURN

STATIC PROCEDURE Walk( c )
   GO TOP
   ? c
   DO WHILE ! Eof()
      ?? " " + LTrim( Str( RecNo() ) ) + Trim( K )
      SKIP
   ENDDO
RETURN

// Equal keys of a build that walks another order keep the order it reaches them in (UNIQUE keeps the first).
STATIC PROCEDURE Ties()
   LOCAL c, i := 0
   ? "-- ties"
   dbCreate( "rdd_icl", { { "K", "C", 1, 0 }, { "N", "N", 2, 0 } } )
   USE rdd_icl NEW
   FOR EACH c IN { "B", "A", "B", "A", "B", "A", "B" }
      APPEND BLANK
      REPLACE K WITH c, N WITH ++i
   NEXT
   INDEX ON -N TO rdd_icl_n
   INDEX ON K TO rdd_icl_uc USECURRENT ADDITIVE
   WalkK( "usecurrent:" )
   SET ORDER TO 1
   INDEX ON K TO rdd_icl_ud USECURRENT DESCENDING ADDITIVE
   WalkK( "descending:" )
   SET ORDER TO 1
   INDEX ON K TO rdd_icl_uu USECURRENT UNIQUE ADDITIVE
   WalkK( "unique:" )
   SET ORDER TO 1
   GO 5
   INDEX ON K TO rdd_icl_ur REST ADDITIVE
   WalkK( "rest from 5:" )
   CLOSE
RETURN

STATIC PROCEDURE WalkK( c )
   GO TOP
   ? c
   DO WHILE ! Eof()
      ?? " " + K + LTrim( Str( RecNo() ) )
      SKIP
   ENDDO
RETURN
