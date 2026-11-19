// Dates and times: 200,000 dates between 1900 and 2099 and times of day from a fixed seed. For each: Year, Month, Day
// against DToS; DoW and CDoW (Sundays, Fridays the 13th); DToC and back with CToD (SET DATE BRITISH, SET CENTURY ON);
// 13 months added (the day kept within the month, through SToD and date arithmetic); the days to the year's end; a
// time of day through TString and back with Secs.
STATIC nSeed := 1999

PROCEDURE Main()
   LOCAL t, dBase := SToD( "19000101" ), i, d, y, m, nDay, n, s, nYmd := 0, nSun := 0, nFri13 := 0, nAdd := 0
   LOCAL nToEnd := 0, nSec := 0, nBad := 0
   SET DATE BRITISH
   SET CENTURY ON
   t := ElapsedMs()
   FOR i := 1 TO 200000
      d := dBase + Rand( 73000 )
      y := Year( d )
      m := Month( d )
      nDay := Day( d )
      n := y * 10000 + m * 100 + nDay
      nYmd += n
      IF Val( DToS( d ) ) != n .OR. CToD( DToC( d ) ) != d
         nBad++
      ENDIF
      IF DoW( d ) == 1
         nSun++
      ENDIF
      IF nDay == 13 .AND. CDoW( d ) == "Friday"
         nFri13++
      ENDIF
      nAdd += Val( DToS( AddMonths( y, m, nDay, 13 ) ) )
      nToEnd += SToD( Str( y, 4 ) + "1231" ) - d
      s := Rand( 86400 )
      IF Secs( TString( s ) ) != s
         nBad++
      ENDIF
      nSec += s
   NEXT
   ? "date_time_ops", ElapsedMs() - t, ;
     IIf( nBad == 0 .AND. nYmd == 3998853334023 .AND. nSun == 28567 .AND. nFri13 == 951 .AND. ;
          nAdd == 4001021793716 .AND. nToEnd == 36531947 .AND. nSec == 8632784536, "ok", "WRONG" )
RETURN

// The date n months after y-m-d, its day made the month's last when the month is shorter.
STATIC FUNCTION AddMonths( y, m, nDay, n )
   LOCAL nM := m + n, nY, dFirst, dNext
   nY := y + Int( ( nM - 1 ) / 12 )
   nM := ( nM - 1 ) % 12 + 1
   dFirst := SToD( StrZero( nY, 4 ) + StrZero( nM, 2 ) + "01" )
   dNext := dFirst + 31
RETURN dFirst + Min( nDay, Day( dNext - Day( dNext ) ) ) - 1

STATIC FUNCTION Rand( n )
   nSeed := nSeed * 16807 % 2147483647
RETURN nSeed % n
