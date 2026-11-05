// Day numbers far from the calendar years: decoding past 9999, DoW() and CDoW() outside the calendar and in year 0.
PROCEDURE Main()
   LOCAL d := CToD( "" ), n
   FOR EACH n IN { 5373485, 5373850, 9999999, 2147438064, 2147483647 }
      ? n, DToS( d + n ), Year( d + n ), Month( d + n ), Day( d + n ), DoW( d + n ), CDoW( d + n ), DToC( d + n )
   NEXT
   FOR EACH n IN { 1, 1000, 1721059, 1721060, 1721061, 1721424, 1721425, 1721426, -1, -3, -10, 0 }
      ? n, DToS( d + n ), Year( d + n ), Month( d + n ), Day( d + n ), DoW( d + n ), "[" + CDoW( d + n ) + "]"
   NEXT
   FOR n := 0 TO 13
      ?? CDoW( SToD( "20240301" ) + n ), DoW( SToD( "20240301" ) + n ), ""
   NEXT
RETURN
