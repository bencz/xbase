// bench/pivot_crosstab.prg in Harbour (its hash functions where xbase has its own names, positions where it has FOR
// EACH with keys).
// A pivot (crosstab) of sales: 2,000,000 records { seller, date, amount } made from a fixed seed (60 sellers, the days
// of 2025), then timed: the amounts summed per seller and month (Month() of the date) in a hash of rows, and per
// weekday (DoW()) and month; row and column totals, the sellers sorted, and the table rendered as text (PadR, Str)
// with a totals line.
STATIC nSeed := 16180

PROCEDURE Main()
   LOCAL aSales := Array( 2000000 ), t, i, r, hRows := { => }, aCol := Array( 12 ), a, k, cTab := "", cLine, nRow
   LOCAL aDow := Array( 7 ), j, nMonth, nSat := 0, nGrand := 0, cBest, nBest := 0, cS07, ok, dBase := SToD( "20250101" )
   FOR i := 1 TO Len( aSales )
      aSales[ i ] := { "SELLER" + StrZero( Rand( 60 ) + 1, 3 ), dBase + Rand( 365 ), Rand( 50000 ) + 1 }
   NEXT
   t := ElapsedMs()
   AFill( aCol, 0 )
   FOR i := 1 TO 7
      aDow[ i ] := AFill( Array( 12 ), 0 )
   NEXT
   FOR EACH r IN aSales
      a := hb_HGetDef( hRows, r[ 1 ] )
      IF a == NIL
         a := AFill( Array( 12 ), 0 )
         hRows[ r[ 1 ] ] := a
      ENDIF
      nMonth := Month( r[ 2 ] )
      a[ nMonth ] += r[ 3 ]
      aDow[ DoW( r[ 2 ] ) ][ nMonth ] += r[ 3 ]
   NEXT
   hb_HSort( hRows )
   FOR j := 1 TO Len( hRows )
      k := hb_HKeyAt( hRows, j )
      a := hb_HValueAt( hRows, j )
      cLine := PadR( k, 10 )
      nRow := 0
      FOR i := 1 TO 12
         cLine += Str( a[ i ], 11 )
         nRow += a[ i ]
         aCol[ i ] += a[ i ]
      NEXT
      cLine += Str( nRow, 13 )
      IF nRow > nBest
         cBest := k
         nBest := nRow
      ENDIF
      IF k == "SELLER007"
         cS07 := cLine
      ENDIF
      cTab += cLine + Chr( 10 )
   NEXT
   cLine := PadR( "TOTAL", 10 )
   FOR i := 1 TO 12
      cLine += Str( aCol[ i ], 11 )
      nGrand += aCol[ i ]
   NEXT
   cTab += cLine + Str( nGrand, 13 ) + Chr( 10 )
   AEval( aDow[ 7 ], {| x | nSat += x } )
   ok := nGrand == 50028051852 .AND. cBest == "SELLER015" .AND. nBest == 844241139 .AND. ;
         aCol[ 1 ] == 4275964399 .AND. aCol[ 2 ] == 3837132322 .AND. aCol[ 12 ] == 4246237533 .AND. ;
         aDow[ 1 ][ 1 ] == 547728957 .AND. nSat == 7111132032 .AND. ;
         Len( cTab ) == 61 * ( 10 + 12 * 11 + 13 + 1 ) .AND. cS07 == "SELLER007 " + ;
         "   71382096   62515370   68284183   68152167   70863968   69201960" + ;
         "   71447433   71672784   67917922   68878748   65718083   68477906    824512620"
   ? "pivot_crosstab", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
RETURN

STATIC FUNCTION Rand( n )
   nSeed := nSeed * 16807 % 2147483647
RETURN nSeed % n
