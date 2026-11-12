// Number functions and conversions.
PROCEDURE Main()
   LOCAL n := 3.14159, i := 42, neg := -7.5
   ? Str( i ), Str( n ), Str( i, 5 ), Str( n, 8, 2 ), Str( n, 5, 3 ), Str( neg, 6, 1 ), Str( 123456, 3 ), Str( n, ;
      0 ), Str( 1.5, 3, 0 ), Str( 2.5, 3, 0 )
   ? StrZero( i, 6 ), StrZero( neg, 8, 2 ), StrZero( 5, 1 ), StrZero( -5, 4 )
   ? Val( "123" ), Val( "  12.50" ), Val( "-3.7abc" ), Val( "abc" ), Val( "1e3" ), Val( ".5" ), Val( "" )
   ? Int( 3.9 ), Int( -3.9 ), Int( 10 ), Abs( -5 ), Abs( -2.25 ), Abs( 3 )
   ? Round( 3.14159, 2 ), Round( 2.5, 0 ), Round( -2.5, 0 ), Round( 1234.5678, -2 ), Round( 1.005, 2 ), Round( 7, 1 )
   ? Max( 3, 7 ), Min( 3, 7 ), Max( 2.5, 2 ), Min( SToD( "20240101" ), SToD( "20230101" ) )
   ? Sqrt( 16 ), Sqrt( 2 ), Exp( 1 ), Log( 10 )
   RETURN
