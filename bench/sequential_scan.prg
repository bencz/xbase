// Sequential scans of a table: 200,000 sales records (region, amount, quantity, customer, date) made from a fixed seed
// in "bench_seqscan.dbf", then timed, 4 rounds of six passes over every record: a DO WHILE ! Eof() loop with SKIP, SUM,
// COUNT FOR, dbEval with a FOR block, LOCATE FOR with CONTINUE, and SUM FOR a date range. The file is deleted at the
// end.
FIELD REGION, AMOUNT, QTY, CUST, SALEDATE
STATIC nSeed := 31337

PROCEDURE Main()
   LOCAL i, t, dBase := SToD( "20260101" ), ok := .T.
   dbCreate( "bench_seqscan", { { "REGION", "N", 1, 0 }, { "AMOUNT", "N", 10, 0 }, { "QTY", "N", 3, 0 }, ;
                                { "CUST", "C", 10, 0 }, { "SALEDATE", "D", 8, 0 } } )
   USE bench_seqscan NEW EXCLUSIVE
   FOR i := 1 TO 200000
      APPEND BLANK
      REPLACE REGION WITH Rand( 9 ) + 1, AMOUNT WITH Rand( 1000000 ), QTY WITH Rand( 50 ) + 1, ;
              SALEDATE WITH dBase + Rand( 365 )
      REPLACE CUST WITH "CUST" + StrZero( Rand( 50000 ), 6 )
   NEXT
   t := ElapsedMs()
   FOR i := 1 TO 4
      ok := ok .AND. Scans()
   NEXT
   ? "sequential_scan", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   CLOSE
   FErase( "bench_seqscan.dbf" )
RETURN

STATIC FUNCTION Scans()
   LOCAL nSumR3 := 0, nCntR3 := 0, nAll := 0, nBig := 0, nQty := 0, nLoc := 0, nLocIds := 0, nJan := 0
   GO TOP
   DO WHILE ! Eof()
      IF REGION == 3
         nSumR3 += AMOUNT
         nCntR3++
      ENDIF
      SKIP
   ENDDO
   SUM AMOUNT TO nAll
   COUNT TO nBig FOR AMOUNT > 900000
   dbEval( {|| nQty += QTY }, {|| REGION <= 2 } )
   LOCATE FOR Left( CUST, 8 ) == "CUST0001"
   DO WHILE Found()
      nLoc++
      nLocIds += RecNo()
      CONTINUE
   ENDDO
   SUM AMOUNT TO nJan FOR SALEDATE < SToD( "20260201" )
RETURN nSumR3 == 11099768125 .AND. nCntR3 == 22094 .AND. nAll == 99898922935 .AND. nBig == 19973 .AND. ;
       nQty == 1135702 .AND. nLoc == 396 .AND. nLocIds == 40072280 .AND. nJan == 8435099403

STATIC FUNCTION Rand( n )
   nSeed := nSeed * 16807 % 2147483647
RETURN nSeed % n
