// Log processing: 300,000 lines of an access log (time, level, [module], user, duration, status) made from a fixed
// seed as one text, then timed: split into lines with Split, each line parsed (SubStr at fixed columns, At for the
// module and the key=value fields, Val), lines counted per level in a hash, durations summed per module, the slowest
// request found, server errors counted per hour, distinct users counted in a hash.
STATIC nSeed := 9001

PROCEDURE Main()
   LOCAL aLv := { "INFO ", "WARN ", "ERROR", "DEBUG" }, aMod := { "auth", "db", "http", "cache", "queue", "mail" }
   LOCAL aSt := { 200, 200, 200, 201, 204, 301, 404, 500, 503, 200 }, n := 300000, i, nSec, l, c := "", t, x, ok
   LOCAL aLines := Array( n ), hLv := { => }, hMod := { => }, hUsers := { => }, aHourErr := AFill( Array( 24 ), 0 )
   LOCAL nMax := -1, nMaxLine := 0, nErr5 := 0, nMs, nSt, nHour, cMod, nB
   FOR i := 1 TO n
      nSec := Int( i * 86400 / n )
      x := Rand( 1000 )
      l := Rand( 10 )
      l := IIf( l < 6, 1, IIf( l < 8, 4, IIf( l < 9, 2, 3 ) ) )
      aLines[ i ] := "2026-11-19 " + StrZero( Int( nSec / 3600 ), 2 ) + ":" + StrZero( Int( nSec / 60 ) % 60, 2 ) + ;
                     ":" + StrZero( nSec % 60, 2 ) + "." + StrZero( x, 3 ) + " " + aLv[ l ] + " [" + ;
                     aMod[ Rand( 6 ) + 1 ] + "] request done user=u" + LTrim( Str( Rand( 5000 ), 10, 0 ) ) + ;
                     " ms=" + LTrim( Str( Rand( 2000 ), 10, 0 ) ) + " status=" + Str( aSt[ Rand( 10 ) + 1 ], 3 )
   NEXT
   FOR EACH x IN aLines
      c += x + Chr( 10 )
   NEXT
   aLines := NIL
   t := ElapsedMs()
   i := 0
   FOR EACH x IN c:Split( Chr( 10 ) )
      IF Empty( x )
         LOOP
      ENDIF
      i++
      l := RTrim( SubStr( x, 25, 5 ) )
      hLv[ l ] := HGetDef( hLv, l, 0 ) + 1
      nB := At( "[", x )
      cMod := SubStr( x, nB + 1, At( "]", x ) - nB - 1 )
      nMs := Val( SubStr( x, At( " ms=", x ) + 4 ) )
      hMod[ cMod ] := HGetDef( hMod, cMod, 0 ) + nMs
      IF nMs > nMax
         nMax := nMs
         nMaxLine := i
      ENDIF
      nSt := Val( SubStr( x, At( " status=", x ) + 8 ) )
      IF nSt >= 500
         nErr5++
         nHour := Val( SubStr( x, 12, 2 ) )
         aHourErr[ nHour + 1 ]++
      ENDIF
      hUsers[ SubStr( x, At( " user=", x ) + 6, At( " ms=", x ) - At( " user=", x ) - 6 ) ] := .T.
   NEXT
   ok := Len( c ) == 23765927 .AND. i == n .AND. hLv[ "INFO" ] == 179990 .AND. hLv[ "WARN" ] == 29916 .AND. ;
         hLv[ "ERROR" ] == 30173 .AND. hLv[ "DEBUG" ] == 59921 .AND. hMod[ "auth" ] == 50005329 .AND. ;
         hMod[ "db" ] == 50599071 .AND. hMod[ "http" ] == 50094170 .AND. hMod[ "cache" ] == 49851623 .AND. ;
         hMod[ "queue" ] == 49898190 .AND. hMod[ "mail" ] == 49855051 .AND. nMax == 1999 .AND. nMaxLine == 1062 .AND. ;
         nErr5 == 59808 .AND. aHourErr[ 14 ] == 2503 .AND. Len( hUsers ) == 5000
   ? "log_processing", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
RETURN

STATIC FUNCTION Rand( n )
   nSeed := nSeed * 16807 % 2147483647
RETURN nSeed % n
