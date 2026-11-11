// Clipper's time sample functions: times of day as "hh:mm:ss" text and as seconds.

// Secs( cTime ): the seconds of "hh:mm:ss" (each field read as Val() reads it, the minutes from the 4th character, the
// seconds from the 7th; a shorter text has none of them).
FUNCTION Secs( cTime )
   LOCAL nSecs := 0
   IF ValType( cTime ) == "C"
      nSecs := Int( Val( cTime ) ) * 3600
      IF Len( cTime ) >= 4
         nSecs += Int( Val( SubStr( cTime, 4 ) ) ) * 60
      ENDIF
      IF Len( cTime ) >= 7
         nSecs += Int( Val( SubStr( cTime, 7 ) ) )
      ENDIF
   ENDIF
RETURN nSecs

// TString( nSecs ): the seconds (truncated) as "hh:mm:ss", the hours taken modulo 24.
FUNCTION TString( nSecs )
   LOCAL n := IIF( ValType( nSecs ) == "N", Int( nSecs ), 0 )
RETURN Digits2( Int( n / 3600 ) % 24 ) + ":" + Digits2( Int( n / 60 ) % 60 ) + ":" + Digits2( n % 60 )

// Days( nSecs ): the whole days in a number of seconds.
FUNCTION Days( nSecs )
RETURN IIF( ValType( nSecs ) == "N", Int( Int( nSecs ) / 86400 ), 0 )

// ElapTime( cStart, cEnd ): the time from cStart to cEnd as "hh:mm:ss", past midnight when cEnd is earlier.
FUNCTION ElapTime( cStart, cEnd )
   LOCAL nStart := Secs( cStart ), nEnd := Secs( cEnd )
RETURN TString( IIF( nEnd < nStart, 86400, 0 ) + nEnd - nStart )

// AMPM( cTime ): "hh:mm:ss" on a 12-hour clock with " am" or " pm": hour 0 (or 24) is 12 am, hours past 12 take 12
// off (a blank for a leading zero), the rest of the text stays.
FUNCTION AMPM( cTime )
   LOCAL nHour, cRest
   IF ValType( cTime ) != "C"
      cTime := ""
   ENDIF
   nHour := Int( Val( cTime ) )
   cRest := SubStr( cTime, 3 )
   IF nHour == 0 .OR. nHour == 24
      RETURN "12" + cRest + " am"
   ELSEIF nHour > 12
      RETURN Str( nHour - 12, 2 ) + cRest + " pm"
   ENDIF
RETURN cTime + IIF( nHour == 12, " pm", " am" )

// LenNum( nNumber ): the length of the number's text as Str() gives it without its leading blanks; 0 for anything
// else.
FUNCTION LenNum( nNumber )
RETURN IIF( ValType( nNumber ) == "N", Len( LTrim( Str( nNumber ) ) ), 0 )

STATIC FUNCTION Digits2( n )
RETURN StrZero( n, 2 )
