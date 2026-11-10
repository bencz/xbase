// Upper() and Lower() change ASCII letters only, eight bytes at a time and then one by one: strings of every length
// around eight, the bytes next to the letters (@ [ ` {) and bytes above 127 kept. At() and RAt() find the first or last
// occurrence: one byte or several, at the start, the end, overlapping, with NUL bytes, a first byte that matches where
// the rest does not, a needle longer than the string.
PROCEDURE Main()
   LOCAL s := "@AZ[`az{" + Chr( 0 ) + "Mixed " + Chr( 193 ) + Chr( 225 ) + "Case!~", i, t := ""
   ? Upper( s ) == "@AZ[`AZ{" + Chr( 0 ) + "MIXED " + Chr( 193 ) + Chr( 225 ) + "CASE!~"
   ? Lower( s ) == "@az[`az{" + Chr( 0 ) + "mixed " + Chr( 193 ) + Chr( 225 ) + "case!~"
   FOR i := 0 TO 17
      t += Upper( Left( "abcdefghijklmnopq", i ) ) + Lower( Left( "ABCDEFGHIJKLMNOPQ", i ) ) + "|"
   NEXT
   ? Len( t ), Right( t, 40 )
   ? At( "a", "banana" ), RAt( "a", "banana" ), At( "ana", "banana" ), RAt( "ana", "banana" ), At( "b", "banana" )
   ? At( "nab", "banana" ), At( "bananas", "banana" ), RAt( "x", "banana" ), At( "anan", "banana" )
   t := "a" + Chr( 0 ) + "b" + Chr( 0 ) + "b"
   ? At( Chr( 0 ) + "b", t ), RAt( Chr( 0 ) + "b", t )
   ? At( "aab", "aaaab" ), RAt( "aa", "aaaab" ), At( "b", "b" ), RAt( "b", "b" ), At( "ab", "b" )
RETURN
