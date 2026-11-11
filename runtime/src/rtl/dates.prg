// The date functions that speak a language: the names of a date's day and month in the language SET LANGUAGE
// selected (rtl/languages.prg).
#include "language.ch"

// CDoW( dDate ): the name of the day of the week; "" for the empty date. Anything else is BASE/1117 (the handler may
// give the result). The day comes from the date's year, month and day as Clipper computes it, so it can differ from
// DoW() in year 0 and for day numbers outside the calendar (decoded as 0/0/0): January and February are months 13 and
// 14 of the year before, the other months count one more, month 0 counts as 13, divisions truncate, 1 is Sunday.
FUNCTION CDoW( dDate )
   LOCAL nYear, nMonth
   IF ValType( dDate ) != "D"
      RETURN __ArgError( 1117, .T. )
   ENDIF
   IF Empty( dDate )
      RETURN ""
   ENDIF
   nMonth := Month( dDate )
   nYear := IIF( nMonth < 3, Year( dDate ) - 1, Year( dDate ) )
   nMonth := IIF( nMonth < 3, nMonth + 13, nMonth + 1 )
RETURN __LanguageCurrent()[ LANG_DAYS ][ ( Day( dDate ) + Int( 26 * nMonth / 10 ) + nYear + Int( nYear / 4 ) - ;
   Int( nYear / 100 ) + Int( nYear / 400 ) + 6 ) % 7 + 1 ]

// CMonth( dDate ): the name of the month; "" for the empty date and a day number outside the calendar (month 0).
// Anything else is BASE/1116 (the handler may give the result).
FUNCTION CMonth( dDate )
   LOCAL nMonth
   IF ValType( dDate ) != "D"
      RETURN __ArgError( 1116, .T. )
   ENDIF
   nMonth := Month( dDate )
RETURN IIF( nMonth == 0, "", __LanguageCurrent()[ LANG_MONTHS ][ nMonth ] )
