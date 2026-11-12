// String functions, including edge cases.
PROCEDURE Main()
   LOCAL s := "Hello, World", e := ""
   ? SubStr( s, 8 ), SubStr( s, 8, 3 ), SubStr( s, -5 ), SubStr( s, -5, 2 ), "[" + SubStr( s, 20 ) + "]", "[" + SubStr( s, 3, 0 ) + "]", SubStr( s, 0, 2 ), "[" + SubStr( s, 3, -1 ) + "]"
   ? Left( s, 5 ), Right( s, 5 ), "[" + Left( s, 0 ) + "]", Left( s, 99 ), "[" + Right( s, -1 ) + "]"
   ? Upper( s ), Lower( s ), Upper( "ação 123" )
   ? "[" + LTrim( "  ab  " ) + "]", "[" + RTrim( "  ab  " ) + "]", "[" + Trim( "  ab  " ) + "]", "[" + AllTrim( "  ab  " ) + "]"
   ? "[" + AllTrim( Chr( 9 ) + "x" + Chr( 9 ) ) + "]", "[" + LTrim( Chr( 13 ) + Chr( 10 ) + "x" ) + "]", "[" + RTrim( "x" + Chr( 9 ) ) + "]"
   ? "[" + Space( 3 ) + "]", "[" + Space( -1 ) + "]", Replicate( "ab", 3 ), "[" + Replicate( "ab", 0 ) + "]"
   ? Replicate( "xyz", 7 ), Replicate( "q", 5 ), Len( Replicate( "abc", 1000001 ) ), ;
      Right( Replicate( "abc", 1000001 ), 4 ), Replicate( "1234567", 1 )
   ? At( "o", s ), At( "xyz", s ), At( "", s ), RAt( "o", s ), RAt( "", s ), At( "World", s )
   ? Chr( 65 ), Chr( 321 ), Asc( "A" ), Asc( "" ), Asc( "abc" )
   ? StrTran( "a,b,c,b", "b", "X" ), StrTran( "a,b,c,b", "b" ), StrTran( "aaaa", "a", "b", 2 ), StrTran( "aaaa", "a", "b", 2, 1 ), StrTran( "abc", "" )
   ? Stuff( "ABCDEF", 2, 3, "xx" ), Stuff( "ABCDEF", 1, 0, "zz" ), Stuff( "ABC", 5, 1, "!" ), Stuff( "ABC", 2, 99, "" ), "[" + Stuff( 1, 1, 1, "x" ) + "]"
   ? IsAlpha( "a1" ), IsAlpha( "1a" ), IsDigit( "5" ), IsUpper( "Ab" ), IsLower( "Ab" ), IsAlpha( "" )
   ? Empty( "" ), Empty( "  " ), Empty( Chr( 9 ) + Chr( 13 ) ), Empty( "x" ), Empty( 0 ), Empty( 0.0 ), Empty( 1 ), Empty( NIL ), Empty( .F. ), Empty( {} ), Empty( { 1 } ), Empty( CToD( "" ) ), Empty( e )
   RETURN
