// Orders (NTX): a table of 200,000 records with random keys; an order built on them (INDEX ON), 100,000 random SEEKs,
// a SKIP through the order, 20,000 records appended with the order kept up to date. Its files ("bench_index*") are
// deleted at the end.
FIELD K, N
STATIC nSeed := 7

PROCEDURE Main()
   LOCAL t := ElapsedMs(), nRecs := 200000, i, nHits := 0, nCount := 0, cPrev := "", ok := .T.
   dbCreate( "bench_index", { { "K", "C", 10, 0 }, { "N", "N", 10, 0 } } )
   USE bench_index NEW
   FOR i := 1 TO nRecs
      APPEND BLANK
      REPLACE K WITH StrZero( Rand( 100000000 ), 10 ), N WITH i
   NEXT
   INDEX ON K TO bench_index_k
   FOR i := 1 TO 100000
      IF dbSeek( StrZero( Rand( 100000000 ), 10 ), .T. )
         nHits++
      ENDIF
   NEXT
   GO TOP
   DO WHILE ! Eof()
      ok := ok .AND. K >= cPrev
      cPrev := K
      nCount++
      SKIP
   ENDDO
   ok := ok .AND. nCount == nRecs
   FOR i := 1 TO 20000
      APPEND BLANK
      REPLACE K WITH StrZero( Rand( 100000000 ), 10 ), N WITH nRecs + i
   NEXT
   nCount := 0
   GO TOP
   DO WHILE ! Eof()
      nCount++
      SKIP
   ENDDO
   ok := ok .AND. nCount == nRecs + 20000 .AND. nHits >= 0
   CLOSE
   FErase( "bench_index.dbf" )
   FErase( "bench_index_k.ntx" )
   ? "index", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
RETURN

STATIC FUNCTION Rand( n )
   nSeed := ( nSeed * 1103515245 + 12345 ) % 2147483648
RETURN Int( nSeed / 65536 ) % n
