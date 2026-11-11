// FPT memos (DBFCDX) against the reference's bytes: after each step the file's length and a checksum of its bytes
// (the writer's signature, bytes 8-15, left out) and the memos read back are the reference's. Memos added, grown and
// shrunk at the end, rewritten in the middle (the old blocks freed, the smallest free area that fits taken), emptied;
// a memo field 4 bytes wide; PACK; more free areas than the list's page holds; ZAP; two areas writing memos of one
// shared table; an FPT table opened by DBFNTX and a DBT table by DBFCDX (the memo format comes from the table's
// version byte); a file without the FlexFile mark. The files start with "rdd_fpt", deleted at the end.
REQUEST DBFCDX
FIELD M, M4
PROCEDURE Main()
   LOCAL i
   rddSetDefault( "DBFCDX" )
   dbCreate( "rdd_fpt", { { "ID", "N", 3, 0 }, { "M", "M", 10, 0 }, { "M4", "M", 4, 0 } } )
   USE rdd_fpt EXCLUSIVE NEW
   ? "struct:", dbStruct()[ 2, 3 ], dbStruct()[ 3, 3 ]
   ? "rdd:", RddName(), Header(), RecSize()
   FOR i := 1 TO 5
      APPEND BLANK
   NEXT
   Put( 1, Replicate( "A", 10 ) )
   Put( 2, Replicate( "B", 100 ) )
   Put( 3, Replicate( "C", 56 ) )
   Put( 4, Replicate( "D", 200 ) )
   Put( 5, Replicate( "E", 30 ) )
   Show( "five" )
   Put( 5, Replicate( "F", 150 ) )
   Show( "grow at end" )
   Put( 5, Replicate( "G", 5 ) )
   Show( "shrink at end" )
   Put( 2, Replicate( "H", 300 ) )
   Show( "rewrite middle" )
   Put( 1, "" )
   Show( "empty 1" )
   Put( 3, Replicate( "I", 20 ) )
   Show( "reuse" )
   GO 4
   REPLACE M4 WITH "four"
   Show( "width 4" )
   Put( 1, Replicate( "J", 40 ) )
   Show( "best fit" )
   FOR i := 1 TO 5
      Put( i, "" )
   NEXT
   GO 4
   REPLACE M4 WITH ""
   Show( "all empty" )
   FOR i := 1 TO 5
      Put( i, Replicate( Chr( 64 + i ), i * 37 ) )
   NEXT
   Show( "again" )
   Put( 4, Replicate( "x", 180 ) )
   Put( 5, Replicate( "y", 10 ) )
   Put( 2, "" )
   Put( 4, Replicate( "z", 10 ) )
   Show( "exact fit marks" )
   DELETE FOR RecNo() % 2 == 0
   PACK
   Show( "pack" )
   FOR i := 1 TO 300
      APPEND BLANK
      REPLACE M WITH Replicate( "p", 20 )
   NEXT
   FOR i := 4 TO 303 STEP 2
      GO i
      REPLACE M WITH ""
   NEXT
   Show( "many free" )
   ZAP
   Show( "zap" )
   CLOSE
   USE rdd_fpt SHARED ALIAS sa NEW
   USE rdd_fpt SHARED ALIAS sb NEW
   sa->( dbAppend() )
   REPLACE sa->M WITH Replicate( "s", 70 )
   sa->( dbUnlock() )
   sb->( dbAppend() )
   REPLACE sb->M WITH Replicate( "t", 30 )
   sb->( dbUnlock() )
   sa->( dbGoTo( 1 ) )
   sa->( RLock() )
   REPLACE sa->M WITH Replicate( "u", 10 )
   sa->( dbUnlock() )
   sb->( dbGoTo( 1 ) )
   ? "shared:", Len( sb->M ), Left( sb->M, 1 ), sb->( LastRec() )
   CLOSE ALL
   USE rdd_fpt EXCLUSIVE NEW
   Show( "after shared" )
   CLOSE
   USE rdd_fpt VIA "DBFNTX" NEW
   ? "fpt by DBFNTX:", RddName(), Len( M ), Left( M, 1 )
   REPLACE M WITH "via ntx"
   CLOSE
   USE rdd_fpt NEW
   Show( "written by DBFNTX" )
   CLOSE
   dbCreate( "rdd_fpt_d", { { "M", "M", 10, 0 } }, "DBFNTX" )
   USE rdd_fpt_d VIA "DBFCDX" NEW
   APPEND BLANK
   REPLACE M WITH "dbt memo"
   ? "dbt by DBFCDX:", RddName(), M, File( "rdd_fpt_d.dbt" ), File( "rdd_fpt_d.fpt" )
   CLOSE
   FErase( "rdd_fpt.dbf" )
   FErase( "rdd_fpt.fpt" )
   FErase( "rdd_fpt_d.dbf" )
   FErase( "rdd_fpt_d.dbt" )
   Plain()
RETURN

// A file without the FlexFile mark (FoxPro's): no free list kept, no 0xAF; PACK and ZAP keep it so, its header
// 512 bytes.
STATIC PROCEDURE Plain()
   LOCAL i, h
   dbCreate( "rdd_fpt_p", { { "M", "M", 10, 0 } } )
   USE rdd_fpt_p EXCLUSIVE NEW
   FOR i := 1 TO 5
      APPEND BLANK
      REPLACE M WITH Replicate( Chr( 64 + i ), i * 30 )
   NEXT
   CLOSE
   h := FOpen( "rdd_fpt_p.fpt", 2 )
   FSeek( h, 512, 0 )
   FWrite( h, Replicate( Chr( 0 ), 10 ) )
   FClose( h )
   USE rdd_fpt_p EXCLUSIVE NEW
   GO 2
   REPLACE M WITH Replicate( "x", 200 )
   ShowP( "bigger" )
   GO 4
   REPLACE M WITH ""
   ShowP( "emptied" )
   GO 3
   REPLACE M WITH Replicate( "y", 20 )
   ShowP( "rewrite" )
   APPEND BLANK
   REPLACE M WITH Replicate( "z", 50 )
   ShowP( "append" )
   PACK
   ShowP( "pack" )
   GO 2
   REPLACE M WITH Replicate( "w", 10 )
   ShowP( "after pack" )
   ZAP
   ShowP( "zap" )
   APPEND BLANK
   REPLACE M WITH "after zap"
   ShowP( "after zap" )
   CLOSE
   FErase( "rdd_fpt_p.dbf" )
   FErase( "rdd_fpt_p.fpt" )
RETURN

STATIC PROCEDURE Put( n, c )
   GO n
   REPLACE M WITH c
RETURN

STATIC PROCEDURE Show( c )
   LOCAL s, i, n := 0, cMemos := ""
   dbCommit()
   s := FileBytes( "rdd_fpt.fpt" )
   s := Left( s, 8 ) + Replicate( Chr( 0 ), 8 ) + SubStr( s, 17 )
   FOR i := 1 TO Len( s )
      n := ( n * 31 + Asc( SubStr( s, i, 1 ) ) ) % 1000000007
   NEXT
   FOR i := 1 TO Min( LastRec(), 5 )
      GO i
      cMemos += " " + LTrim( Str( Len( M ) ) ) + Left( M, 1 )
   NEXT
   ? c, Len( s ), n, cMemos
RETURN

STATIC FUNCTION FileBytes( cFile )
   LOCAL h := FOpen( cFile ), n := FSeek( h, 0, 2 ), s := Space( n )
   FSeek( h, 0, 0 )
   FRead( h, @s, n )
   FClose( h )
RETURN s

STATIC PROCEDURE ShowP( c )
   LOCAL s, i, n := 0, cMemos := "", h, nLen
   dbCommit()
   h := FOpen( "rdd_fpt_p.fpt" )
   nLen := FSeek( h, 0, 2 )
   s := Space( nLen )
   FSeek( h, 0, 0 )
   FRead( h, @s, nLen )
   FClose( h )
   s := Left( s, 8 ) + Replicate( Chr( 0 ), 8 ) + SubStr( s, 17 )
   FOR i := 1 TO Len( s )
      n := ( n * 31 + Asc( SubStr( s, i, 1 ) ) ) % 1000000007
   NEXT
   FOR i := 1 TO Min( LastRec(), 6 )
      GO i
      cMemos += " " + LTrim( Str( Len( M ) ) ) + Left( M, 1 )
   NEXT
   ? c, Len( s ), n, cMemos
RETURN
