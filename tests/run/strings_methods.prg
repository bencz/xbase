// CHARACTER:Split() over separators of one byte, several, and more than 16 (the two-way search), at the start, the
// end, side by side, absent, longer than the text, the default (a space), empty (the characters), and an empty text;
// a long text split in one pass. StartsWith() and EndsWith() with an empty part, the whole text, a longer part, NUL
// bytes. Upper() and Lower() of texts the case does not change and of texts with one letter
// to change at each place, around eight bytes. Arguments that are no string: Split 9033, StartsWith and EndsWith 9034.
PROCEDURE Main()
   LOCAL cLong := "<separator-20-bytes>", c, i, a, lOk := .T., t
   ? Shown( "a,b,,c":Split( "," ) ), Shown( ",a,":Split( "," ) ), Shown( ",,":Split( "," ) ), ;
      Shown( "abc":Split( "," ) )
   ? Shown( "a::b::::c":Split( "::" ) ), Shown( "ab":Split( "abc" ) ), Shown( "a b  c":Split() ), ;
      Shown( "abc":Split( "" ) )
   ? Shown( "":Split( "," ) ), Shown( ( "x" + cLong + "y" + cLong + cLong + "z" ):Split( cLong ) )
   ? Shown( ( "a" + Chr( 0 ) + "b" ):Split( Chr( 0 ) ) )
   c := Replicate( "word,", 200000 )
   t := Seconds()
   a := c:Split( "," )
   ? "long:", Len( a ), a[ 1 ], a[ 200000 ], a[ 200001 ] == "", Seconds() - t < 5
   ? "abc":StartsWith( "" ), "abc":StartsWith( "abc" ), "abc":StartsWith( "abcd" ), "abc":StartsWith( "ab" ), ;
      "abc":StartsWith( "b" ), "":StartsWith( "" ), ( "a" + Chr( 0 ) + "b" ):StartsWith( "a" + Chr( 0 ) )
   ? "abc":EndsWith( "" ), "abc":EndsWith( "abc" ), "abc":EndsWith( "xabc" ), "abc":EndsWith( "bc" ), ;
      "abc":EndsWith( "b" ), "":EndsWith( "" ), ( "a" + Chr( 0 ) + "b" ):EndsWith( Chr( 0 ) + "b" )
   FOR i := 0 TO 20
      c := Left( "ABCDEFGHIJ0123456789@[", i )
      lOk := lOk .AND. Upper( c ) == c .AND. Lower( Lower( c ) ) == Lower( c ) .AND. Len( Upper( c ) ) == i
      IF i > 0
         lOk := lOk .AND. Upper( Stuff( c, i, 1, "q" ) ) == Stuff( c, i, 1, "Q" ) .AND. ;
            Lower( Stuff( Lower( c ), i, 1, "Q" ) ) == Stuff( Lower( c ), i, 1, "q" )
      ENDIF
   NEXT
   ? "case:", lOk, Upper( "ABC" + Chr( 225 ) ) == "ABC" + Chr( 225 ), Lower( "abc" + Chr( 193 ) ) == "abc" + Chr( 193 )
   ? Tried( {|| "abc":Split( 5 ) } ), Tried( {|| "abc":StartsWith( 5 ) } ), Tried( {|| "abc":EndsWith( {} ) } )
RETURN

STATIC FUNCTION Shown( a )
   LOCAL c := "{", x, cComma := ""
   FOR EACH x IN a
      c += cComma + '"' + StrTran( x, Chr( 0 ), "\0" ) + '"'
      cComma := ","
   NEXT
RETURN c + "}"

STATIC FUNCTION Tried( b )
   LOCAL x
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      x := Eval( b )
   RECOVER USING x
      x := x:subsystem + "/" + LTrim( Str( x:subCode ) ) + " " + x:operation
   END SEQUENCE
RETURN x
