// Scopes: ordScope() per order, giving the bound before; SET SCOPE, SCOPETOP and SCOPEBOTTOM; navigation, SEEK,
// ordKeyCount(), ordKeyNo() and ordKeyGoto() within the scope (the filter and SET DELETED do not count); bounds that
// compare as prefixes, cross, have another type than the key's (none) or are codeblocks (evaluated again before
// each use: ordKeyCount() follows the block's value, where the reference keeps a count it computed before);
// descending orders; ordKeyVal() and ordSkipUnique(); REINDEX EVAL. The second table has enough records for trees
// of several levels, whose counts and positions are checked against a walk. The files start with
// "rdd_index_scope", deleted at the end.
FIELD K, N, B

PROCEDURE Main()
   Small()
   Large()
   Seeks()
   Numbers()
RETURN

// SEEK with a scope: SOFTSEEK before the scope gives its first record, past it the end; LAST too.
STATIC PROCEDURE Seeks()
   LOCAL c
   dbCreate( "rdd_index_scope_s", { { "K", "C", 3, 0 } } )
   USE rdd_index_scope_s NEW
   FOR EACH c IN { "b1", "a1", "c1", "b2", "d1", "b3", "a2", "e1", "c2" }
      APPEND BLANK
      REPLACE K WITH c
   NEXT
   INDEX ON K TO rdd_index_scope_k
   SET SCOPE TO "b", "c"
   Try( "a", .T., .F. )
   Try( "a", .T., .T. )
   Try( "a", .F., .F. )
   Try( "a", .F., .T. )
   Try( "d", .T., .F. )
   Try( "d", .T., .T. )
   Try( "c", .T., .T. )
   Try( "bz", .T., .F. )
   Try( "bz", .T., .T. )
   Try( "c3", .T., .F. )
   Try( "c3", .T., .T. )
   Try( "b0", .T., .T. )
   Try( "", .T., .T. )
   INDEX ON K TO rdd_index_scope_d DESCENDING
   SET SCOPE TO "c", "b"
   Try( "a", .T., .F. )
   Try( "a", .T., .T. )
   Try( "d", .T., .F. )
   Try( "d", .T., .T. )
   Try( "bz", .T., .F. )
   Try( "c3", .T., .T. )
   CLOSE
   FErase( "rdd_index_scope_s.dbf" )
   AEval( { "k", "d" }, {| c | FErase( "rdd_index_scope_" + c + ".ntx" ) } )
RETURN

// ordKeyVal() of numbers: 19 digits kept whole, negative ones.
STATIC PROCEDURE Numbers()
   LOCAL c, i := 0
   dbCreate( "rdd_index_scope_x", { { "K", "C", 2, 0 }, { "N", "N", 4, 0 }, { "B", "N", 19, 0 } } )
   USE rdd_index_scope_x NEW
   FOR EACH c IN { "b", "a", "c", "b", "a" }
      APPEND BLANK
      REPLACE K WITH c, N WITH ++i, B WITH 1234567890123456789 - i
   NEXT
   INDEX ON B TO rdd_index_scope_b
   GO 3
   ? "big key:", ordKeyVal(), ValType( ordKeyVal() ), Len( Str( ordKeyVal() ) )
   INDEX ON N - 3 TO rdd_index_scope_neg
   GO 1
   ? "neg key:", ordKeyVal(), Str( ordKeyVal() )
   INDEX ON K TO rdd_index_scope_k
   SET SCOPE TO "b", "c"
   ? "soft a:", dbSeek( "a", .T. ), RecNo(), K
   ? "soft d:", dbSeek( "d", .T. ), RecNo(), Eof()
   ? "hard a:", dbSeek( "a" ), RecNo(), Eof()
   SET SCOPE TO
   CLOSE
   FErase( "rdd_index_scope_x.dbf" )
   AEval( { "b", "neg", "k" }, {| c | FErase( "rdd_index_scope_" + c + ".ntx" ) } )
RETURN

STATIC PROCEDURE Try( x, lSoft, lLast )
   ? x, lSoft, lLast, dbSeek( x, lSoft, lLast ), RecNo(), Eof(), Bof(), K
RETURN

STATIC PROCEDURE Small()
   LOCAL c, i := 0
   ErrorBlock( {| e | QOut( "ERR", e:subSystem, e:genCode, e:subCode, e:description, "[" + e:operation + "]" ), ;
      Break( e ) } )
   dbCreate( "rdd_index_scope", { { "K", "C", 3, 0 }, { "N", "N", 3, 0 } } )
   USE rdd_index_scope NEW
   FOR EACH c IN { "b1", "a1", "c1", "b2", "d1", "b3", "a2", "e1", "c2" }
      APPEND BLANK
      REPLACE K WITH c, N WITH ++i
   NEXT
   INDEX ON K TO rdd_index_scope_k
   ? "count:", ordKeyCount(), "no:", ordKeyNo(), RecNo(), "val:", ordKeyVal()
   GO 5
   ? "rec 5 no:", ordKeyNo(), ordKeyVal()
   ? "scope:", ordScope( 0, "b" ), ordScope( 1, "c" ), ordScope( 0 ), ordScope( 1 )
   ? "after scope set:", RecNo()
   Walk( "scoped:" )
   ? "count scoped:", ordKeyCount(), "no:", ordKeyNo()
   GO BOTTOM
   ? "bottom:", RecNo(), K, ordKeyNo()
   SKIP
   ? "past bottom:", RecNo(), Eof()
   GO TOP
   SKIP -1
   ? "before top:", RecNo(), Bof()
   ? "seek a1:", dbSeek( "a1" ), RecNo(), Eof()
   ? "seek c2:", dbSeek( "c2" ), RecNo()
   ? "seek d:", dbSeek( "d", .T. ), RecNo(), Eof()
   GO 1
   ? "goto out of scope 5:", ordKeyNo(), RecNo()
   GO 5
   SKIP
   ? "skip from out of scope:", RecNo(), Eof(), K
   ? "keygoto 2:", ordKeyGoto( 2 ), RecNo(), K
   ? "keygoto 9:", ordKeyGoto( 9 ), RecNo(), Eof()
   ordScope( 0, NIL )
   ordScope( 1, NIL )
   Walk( "unscoped:" )
   ? "keygoto 3:", ordKeyGoto( 3 ), RecNo(), K
   GO TOP
   ? "skipunique:", ordSkipUnique(), RecNo(), K
   ? "skipunique:", ordSkipUnique(), RecNo(), K
   ? "skipunique back:", ordSkipUnique( -1 ), RecNo(), K
   SET SCOPETOP TO "c"
   SET SCOPEBOTTOM TO "d"
   Walk( "set scope:" )
   SET SCOPE TO
   Walk( "set scope to:" )
   SET SCOPE TO "a1", "b2"
   Walk( "set scope a1 b2:" )
   SET SCOPE TO "b"
   Walk( "set scope b:" )
   SET SCOPE TO
   SET DELETED ON
   GO 4
   DELETE
   ? "count deleted:", ordKeyCount()
   SET FILTER TO N > 3
   ? "count filter:", ordKeyCount()
   GO 9
   ? "no filter:", ordKeyNo()
   SET FILTER TO
   SET DELETED OFF
   SET ORDER TO 0
   ? "order 0:", ordKeyCount(), ordKeyNo(), ordKeyVal()
   ? "by name:", ordKeyCount( "rdd_index_" ), ordKeyNo( "Rdd_Index_" ), ordKeyCount( "none" )
   CLOSE
   FErase( "rdd_index_scope.dbf" )
   FErase( "rdd_index_scope_k.ntx" )
RETURN

STATIC PROCEDURE Large()
   LOCAL i, nCalls := 0, cTop := "c"
   dbCreate( "rdd_index_scope_b", { { "K", "C", 4, 0 }, { "N", "N", 6, 0 } } )
   USE rdd_index_scope_b NEW
   FOR i := 1 TO 6000
      APPEND BLANK
      REPLACE K WITH Chr( 97 + ( i * 7 ) % 13 ) + Str( ( i * 31 ) % 97, 3 ), N WITH i
   NEXT
   INDEX ON K TO rdd_index_scope_k
   Check( "none" )
   SET SCOPE TO "c", "f"
   Check( "c-f" )
   SET SCOPE TO "d"
   Check( "d" )
   SET SCOPE TO "d 50", "e  3"
   Check( "d50-e3" )
   SET SCOPE TO "x"
   Check( "x" )
   SET SCOPE TO "f", "c"
   Check( "crossed" )
   SET SCOPE TO , "b"
   Check( "bottom b" )
   SET SCOPETOP TO "l"
   SET SCOPEBOTTOM TO
   Check( "top l" )
   SET SCOPE TO
   ordScope( 0, {|| cTop } )
   Check( "block c" )
   cTop := "k"
   Check( "block k" )
   ? "scope val:", ValType( ordScope( 0 ) ), ordScope( 1 )
   ? "scope 2:", ordScope( 2, "g" ), ordScope( 0 ), ordScope( 1 )
   Check( "scope 2 g" )
   ? "scope 2 clear:", ordScope( 2 ), ordScope( 0 ), ordScope( 1 )
   ? "wrong type:", ordScope( 0, 5 ), ordScope( 0 )
   Check( "after wrong type" )
   ? "keygoto 0:", ordKeyGoto( 0 ), RecNo()
   INDEX ON K TO rdd_index_scope_d DESCENDING
   Check( "desc none" )
   SET SCOPE TO "f", "c"
   Check( "desc f-c" )
   SET SCOPE TO "c", "f"
   Check( "desc c-f" )
   SET SCOPE TO
   INDEX ON N TO rdd_index_scope_n FOR N % 3 == 0
   SET SCOPE TO 100, 400
   Check( "num 100-400" )
   GO 101
   ? "not in order:", ordKeyNo(), ordKeyVal()
   SET SCOPE TO
   SET INDEX TO rdd_index_scope_k, rdd_index_scope_d, rdd_index_scope_n
   REINDEX EVAL {|| nCalls++, .T. } EVERY 1000
   ? "reindex eval:", nCalls, ordKeyCount( 1 ), ordKeyCount( 2 ), ordKeyCount( 3 )
   SET ORDER TO 1
   GO TOP
   i := 0
   DO WHILE ! Eof()
      i++
      ordSkipUnique()
   ENDDO
   ? "unique keys:", i
   GO BOTTOM
   i := 0
   DO WHILE ! Bof()
      i++
      ordSkipUnique( -1 )
   ENDDO
   ? "unique keys back:", i, RecNo()
   SET SCOPE TO "c", "d"
   GO TOP
   i := 0
   DO WHILE ! Eof()
      i++
      ordSkipUnique()
   ENDDO
   ? "unique keys scoped:", i
   SET DELETED ON
   GO TOP
   DELETE
   SKIP
   DELETE
   SET FILTER TO N > 10
   ? "count with deleted:", ordKeyCount(), ordKeyNo()
   ? "keygoto 1:", ordKeyGoto( 1 ), RecNo(), ordKeyNo()
   CLOSE
   FErase( "rdd_index_scope_b.dbf" )
   AEval( { "k", "d", "n" }, {| c | FErase( "rdd_index_scope_" + c + ".ntx" ) } )
RETURN

STATIC PROCEDURE Walk( c )
   GO TOP
   ? c
   DO WHILE ! Eof()
      ?? " " + LTrim( Str( RecNo() ) ) + Trim( K )
      SKIP
   ENDDO
RETURN

STATIC PROCEDURE Check( c )
   LOCAL nKeys := 0, nSum := 0, nBad := 0, k, nRec
   GO TOP
   DO WHILE ! Eof()
      nKeys++
      IF ordKeyNo() != nKeys
         nBad++
      ENDIF
      nSum += RecNo() * ( nKeys % 7 + 1 )
      SKIP
   ENDDO
   FOR k := 1 TO nKeys + 1 STEP Max( 1, Int( nKeys / 37 ) )
      ordKeyGoto( k )
      nRec := iif( Eof(), 0, RecNo() )
      IF k <= nKeys .AND. ordKeyNo() != k
         nBad++
      ENDIF
      nSum += nRec % 1000
   NEXT
   ordKeyGoto( nKeys + 1 )
   ? c, nKeys, ordKeyCount(), nSum, nBad, Eof()
RETURN
