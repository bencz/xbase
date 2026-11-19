// bench/relatorio_agregacao.prg in Harbour (its hash functions where xbase has its own names, positions where it has
// FOR EACH with keys, hb_ATokens() where it has Split).
// Business report: 1,000,000 sales records (region, product, quantity, unit price in cents) made from a fixed seed,
// then timed: aggregated by region and by product in hashes keyed by code, the 500 products ordered by amount with
// ASort and a block (ties by code), and a report built with Transform (@E pictures): a line per region (sales,
// quantity, amount) and the top 20 products.
STATIC nSeed := 12345

PROCEDURE Main()
   LOCAL aSales := Array( 1000000 ), t, i, r, hReg := { => }, hProd := { => }, aProd := {}, cRep := "", nTotal := 0
   LOCAL k, a, x, aLines, ok, j
   FOR i := 1 TO Len( aSales )
      aSales[ i ] := { "R" + StrZero( Rand( 8 ) + 1, 2 ), "P" + StrZero( Rand( 500 ) + 1, 4 ), Rand( 20 ) + 1, ;
                       Rand( 100000 ) + 100 }
   NEXT
   t := ElapsedMs()
   FOR EACH r IN aSales
      x := r[ 3 ] * r[ 4 ]
      IF hb_HHasKey( hReg, r[ 1 ] )
         a := hReg[ r[ 1 ] ]
         a[ 1 ]++
         a[ 2 ] += r[ 3 ]
         a[ 3 ] += x
      ELSE
         hReg[ r[ 1 ] ] := { 1, r[ 3 ], x }
      ENDIF
      hProd[ r[ 2 ] ] := hb_HGetDef( hProd, r[ 2 ], 0 ) + x
      nTotal += x
   NEXT
   FOR j := 1 TO Len( hProd )
      k := hb_HKeyAt( hProd, j )
      x := hb_HValueAt( hProd, j )
      AAdd( aProd, { k, x } )
   NEXT
   ASort( aProd, , , {| p, q | p[ 2 ] > q[ 2 ] .OR. ( p[ 2 ] == q[ 2 ] .AND. p[ 1 ] < q[ 1 ] ) } )
   hb_HSort( hReg )
   FOR j := 1 TO Len( hReg )
      k := hb_HKeyAt( hReg, j )
      a := hb_HValueAt( hReg, j )
      cRep += k + Transform( a[ 1 ], "@E 9,999,999" ) + Transform( a[ 2 ], "@E 99,999,999" ) + ;
              Transform( a[ 3 ] / 100, "@E 999,999,999,999.99" ) + Chr( 10 )
   NEXT
   FOR i := 1 TO 20
      cRep += aProd[ i ][ 1 ] + Transform( aProd[ i ][ 2 ] / 100, "@E 999,999,999,999.99" ) + Chr( 10 )
   NEXT
   aLines := hb_ATokens( cRep, Chr( 10 ) )
   ok := nTotal == 525748851783 .AND. Len( cRep ) == 8 * 41 + 20 * 24 .AND. Len( hProd ) == 500 .AND. ;
         aLines[ 1 ] == "R01  125.691 1.320.222    662.846.057,84" .AND. ;
         aLines[ 8 ] == "R08  124.683 1.310.936    656.013.155,38" .AND. ;
         aLines[ 9 ] == "P0303     11.438.921,18" .AND. aLines[ 28 ] == "P0360     11.021.303,35"
   ? "relatorio_agregacao", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
RETURN

STATIC FUNCTION Rand( n )
   nSeed := nSeed * 16807 % 2147483647
RETURN nSeed % n
