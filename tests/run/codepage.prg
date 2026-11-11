// SET CODEPAGE: UTF-8 by default, where Upper, Lower, IsAlpha, IsUpper and IsLower read the letters of every script
// and never change a string's length (ß and ı keep their case, whose other form takes another number of bytes);
// Len and SubStr still count bytes, the Char*() functions characters (an invalid byte is one, U+FFFD); CPConvert
// between UTF-8 and the single-byte codepages ("?" for a character the target has not); under CP850 the case
// functions read its bytes; Set( _SET_CODEPAGE ) takes the other names of a codepage and leaves an unknown one; the
// Char*() argument errors have xbase's own codes. Also: Greek with iota subscript, invalid UTF-8 (truncated and
// overlong sequences, a match inside a character, which CharAt does not take), the edges of the Char*() ranges, a
// round trip of a single-byte codepage's upper half through UTF-8, and the bytes CP1252 leaves undefined.
#include "set.ch"

PROCEDURE Main()
   LOCAL s := "ação Ñandú ÿ ß ı Ωμέγα Жук", c
   ? Set( _SET_CODEPAGE ), Upper( s ), Len( Upper( s ) ) == Len( s )
   ? Lower( "AÇÃO ÑANDÚ Ÿ ΩΜΈΓΑ ЖУК" )
   ? IsAlpha( "ç" ), IsUpper( "Ç" ), IsLower( "ç" ), IsUpper( "ç" ), IsAlpha( "€" ), IsAlpha( "日本" ), IsLower( "日" )
   ? Len( "ação" ), CharLen( "ação" ), CharSubStr( "ação", 2, 2 ), CharSubStr( "ação", -2 ), CharLeft( "ação", 3 ), ;
      CharRight( "ação", 2 ), CharAt( "ão", "ação" ), At( "ão", "ação" )
   ? CharAsc( "ç" ), CharAsc( "€uro" ), CharAsc( "" ), CharChr( 8364 ) == "€", Len( CharChr( 8364 ) ), ;
      "[" + CharChr( 55296 ) + "]", CharLen( Chr( 255 ) + "a" ), CharAsc( Chr( 255 ) )
   c := CPConvert( "ação", "UTF8", "CP850" )
   ? Len( c ), Asc( SubStr( c, 2, 1 ) ), Asc( SubStr( c, 3, 1 ) ), CPConvert( c, "IBM850", "UTF-8" ) == "ação", ;
      CPConvert( CPConvert( "ação €", "UTF8", "CP437" ), "CP437", "UTF8" ), ;
      Asc( CPConvert( "€", "UTF8", "WINDOWS-1252" ) ), CPConvert( "x", "UTF8", "UTF8" )
   ? Upper( "ᾳ ῃ ῳ" ) == "ᾼ ῌ ῼ", CharAt( Chr( 167 ), "ç" ), CharAt( Chr( 167 ) + "X", "çX" ), CharAt( "X", "çX" ), ;
      CharLen( Chr( 195 ) ), CharLen( Chr( 224 ) + Chr( 128 ) ), CharLen( Chr( 192 ) + Chr( 128 ) ), ;
      CharLen( Chr( 237 ) + Chr( 160 ) + Chr( 128 ) ), CharLen( Chr( 244 ) + Chr( 144 ) + Chr( 128 ) + Chr( 128 ) )
   ? CharSubStr( "ação", 0, 2 ), "[" + CharSubStr( "ação", 5 ) + "]", "[" + CharSubStr( "ação", 2, -1 ) + "]", ;
      CharSubStr( "ação", -9, 2 ), "[" + CharLeft( "ação", 0 ) + "]", CharLeft( "ação", 9 ), CharRight( "ação", 9 ), ;
      "[" + CharRight( "", 2 ) + "]", CharLen( "" ), CharAt( "", "x" )
   c := ""
   AEval( Array( 128 ), {| x, i | c += Chr( 127 + i ) } )
   ? CPConvert( CPConvert( c, "CP850", "UTF8" ), "UTF8", "CP850" ) == c, ;
      CPConvert( CPConvert( c, "CP437", "UTF8" ), "UTF8", "CP437" ) == c, ;
      Len( CPConvert( Chr( 129 ), "CP1252", "UTF8" ) ), CPConvert( CPConvert( Chr( 157 ), "CP1252", "UTF8" ), "UTF8", ;
      "CP1252" ) == Chr( 157 )
   c := CPConvert( "ação", "UTF8", "CP850" )
   SET CODEPAGE TO CP850
   ? Set( _SET_CODEPAGE ), CPConvert( Upper( c ), "CP850", "UTF8" ), IsAlpha( Chr( 135 ) ), IsUpper( Chr( 128 ) ), ;
      CharLen( c ), CharAsc( Chr( 135 ) )
   ? Set( _SET_CODEPAGE, "latin1" ), Set( _SET_CODEPAGE, "nope" ), Set( _SET_CODEPAGE )
   SET CODEPAGE TO UTF8
   ErrorBlock( {| e | IIF( e:canSubstitute, e:operation + " " + LTrim( Str( e:subCode ) ), Break( e ) ) } )
   ? CharLen( 1 ), CharSubStr( "x" ), CharLeft( 1, 1 ), "[" + CharRight( 1, 1 ) + "]", CharAt( 1, "x" ), ;
      CharAsc( 1 ), CharChr( "x" ), CPConvert( "x", "NOPE", "UTF8" )
RETURN
