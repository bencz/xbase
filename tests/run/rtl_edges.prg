// Edge cases of the number, string and date functions (narrow Str() fields, Stuff()/StrTran() argument rules,
// day numbers outside the calendar, results outside the reals).
PROCEDURE Main()
   LOCAL d := CToD( "" )
   ? Round( 0.4999999999, 0 ), Round( 1.4999999999, 0 ), Round( 0.0049999999999, 2 )
   ? Round( 1.005, 2 ), Round( 2.675, 2 ), Round( 0.285, 2 ), Round( -2.5, 0 ), Round( 0.045, 2 )
   ? Round( 1.0049999999999999, 2 ), Round( 1234567.125, 2 ), Round( 0.49999999999999994, 0 )
   ? Round( 1.15, 1 ), Round( 1.25, 1 ), Round( 1.35, 1 ), Round( -1.15, 1 ), Round( 123.455, 2 )
   ? Round( 1000000000000000.5, 0 ), Round( 4503599627370495.5, 0 ), Round( 1.5, -1 ), Round( 15, -1 ), Round( 149.99999999, -2 )
   ? Round( 5.0000000001, 0 ), Round( 2.4999999999999, 0 ), Round( 2.49999999999999, 0 ), Round( 2.499999999999999, 0 )
   ? Round( 0.15, 1 ), Round( 0.25, 1 ), Round( 0.35, 1 ), Round( 1.45, 1 ), Round( 8.345, 2 ), Round( 1.0000005, 6 )
   ? Round( -0.5, 0 ), Round( -1.005, 2 ), Round( 0, 3 ), Round( 12345, -2 ), Round( -12355, -1 ), Round( 7, 0 )
   ? Int( 0.99999999999999989 ), Int( 2.9999999999999996 ), Int( -2.9999999999999996 ), Int( 7.5 ), Int( -7.5 )
   ? Sqrt( -1 ), Exp( 1000 ), Exp( -1000 ), Log( -1 ), Log( 0 ), Sqrt( 16 ), Log( 1 )
   ? Len( Str( Sqrt( -1 ) ) ), Len( Str( Log( 0 ) ) ), Len( Str( Log( -1 ) ) ), Len( Str( Exp( 1000 ) ) )
   ? Len( Str( 1.5, 0, 100000 ) ), Str( 1.5, 0, 2 ), Str( 1.5, 3, 3 ), Str( 1.5, 4, 3 ), Str( 1.5, 5, 3 ), Str( 1.5, 2, 1 ), Str( 1.5, 1, 0 )
   ? Str( 1.5, -3, 1 ), Str( 1.5, 4, -1 ), StrZero( 1.5, 3, 3 ), StrZero( -1.5, 5, 1 ), Len( Str( 1, 5, 99 ) ), Str( 3.14159, 5, 5 )
   ? Str( 0.5, 4, 3 ), Str( 0.5, 3, 2 ), Str( -0.5, 4, 3 ), Str( 12.5, 4, 2 ), Str( 1, 3, 2 ), Str( 1, 2, 2 ), Str( 0, 2, 1 )
   ? "[" + Stuff( "abc", 2, 1 ) + "]", "[" + Stuff( "abc", NIL, 1, "x" ) + "]", "[" + Stuff( "abc", 2, 1, 5 ) + "]"
   ? Stuff( "abcdef", -2, 1, "X" ), Stuff( "abcdef", 2, -3, "X" ), Stuff( "abcdef", 0, 1, "X" ), Stuff( "abcdef", 10, 1, "X" )
   ? Stuff( "abcdef", 2, 100, "X" ), Stuff( "abcdef", 2, 0, "XY" ), Stuff( "", 1, 1, "X" ), Stuff( "abcdef", 7, 1, "X" )
   ? "[" + StrTran( "abcbb", "b", "x", 0, 1 ) + "]", "[" + StrTran( "abcbb", "b", "x", 1, 0 ) + "]", StrTran( "abcbb", "b", "x", -1, 1 )
   ? StrTran( "abcbb", "b", "x", NIL, 2 ), StrTran( "abcbb", "b", "x", 2, NIL ), StrTran( "abcbb", "b", "x", 2.7, 1.9 ), StrTran( "abcbb", "b", "x", 1, -1 )
   ? "[" + StrTran( "abc", "", "x", 0 ) + "]", StrTran( "abc", "", "x" ), StrTran( "abc", "b" ), StrTran( "abcbb", "b", "x", 4 )
   ? DToS( d - 1 ), DToC( d - 1 ), Year( d - 1 ), DoW( d - 1 ), Empty( d - 1 ), d - 1 == d
   ? DToS( SToD( "00010101" ) ), DToS( SToD( "00010101" ) - 1 ), DToS( SToD( "99991231" ) )
   ? SToD( "00010101" ) - d, SToD( "99991231" ) - d
   ? DToS( d + 1 ), DToC( d + 1 ), Year( d + 1 ), DoW( d + 1 ), CMonth( d + 1 ), Day( d + 1 ), DoW( d ), CDoW( d )
   ? DToS( d + 1721425 ), DToS( d + 1721426 ), DToS( d + 5373484 )
   ? ( d - 5 ) - ( d - 7 ), ( d + 100000000000000 ) - d, DToS( d + 100000000000000 ) , DToC( d + 3000000000000000000 )
   ? DoW( SToD( "20240315" ) ), CDoW( SToD( "20240315" ) ), DoW( SToD( "00000101" ) ), CDoW( SToD( "00010101" ) )
   ? Replicate( "ab", 3 ), Len( Replicate( "ab", 0 ) ), Len( Replicate( "", 4611686018427387904 ) ), Len( Replicate( "ab", -1 ) )
   ? Val( Chr( 11 ) + "5" ), Val( Chr( 12 ) + "5" ), Val( Chr( 9 ) + "5" )
   SET DATE FORMAT TO "MM/DD"
   SET CENTURY ON
   ? Set( _SET_DATEFORMAT )
   SET CENTURY OFF
   ? Set( _SET_DATEFORMAT )
   SET DATE FORMAT TO "dd.mm.yy"
   SET CENTURY ON
   ? Set( _SET_DATEFORMAT )
   SET DATE FORMAT TO "yy/mm/dd yy"
   SET CENTURY ON
   ? Set( _SET_DATEFORMAT )
   SET DATE FORMAT TO "dd/mm/yyyy -------------------------------------------------------------------------- end"
   ? Set( _SET_DATEFORMAT ), Len( Set( _SET_DATEFORMAT ) ), SToD( "20240315" )
   SET CENTURY OFF
   ? Set( _SET_DATEFORMAT )
RETURN
