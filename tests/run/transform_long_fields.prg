// Transform() with fields longer than the text it builds in place (128 characters): number templates of 150 to 300
// placeholders, with and without decimals, against Str() of that width; @L, @E and @( on them; a long @R template
// on a string; and the short ones again after them.
PROCEDURE Main()
   LOCAL c150 := Replicate( "9", 150 ), c300 := Replicate( "9", 300 ), x := -1234567.891, c

   c := Transform( x, c150 + ".99" )
   ? "150.2:", Len( c ), c == Str( x, 153, 2 ), AllTrim( c )
   c := Transform( 42, c300 )
   ? "300:", Len( c ), c == Str( 42, 300 ), AllTrim( c )
   c := Transform( 42, "@L " + c150 )
   ? "@L:", Len( c ), Left( c, 3 ), Right( c, 4 ), Len( c ) - Len( StrTran( c, "0" ) )
   c := Transform( x, "@E " + c150 + ".999" )
   ? "@E:", Len( c ), Right( c, 13 )
   c := Transform( x, "@( " + c150 + ".99" )
   ? "@(:", Len( c ), Left( c, 1 ), Right( c, 13 )
   c := Transform( Replicate( "ab", 100 ), "@R " + Replicate( "X-", 100 ) )
   ? "@R:", Len( c ), Left( c, 8 ), Right( c, 4 )
   ? "short:", Transform( x, "9,999,999.99" ), Transform( 7, "@Z 9" ), Transform( 0, "@Z 9" ) + "|"
RETURN
