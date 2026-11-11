// The memo-line functions (MemoLine, MLCount, MLPos, MLCToPos, MPosToLC, HardCR, MemoTran), compared with the
// reference (with CR LF as its line end, Clipper's). Lines are shown between brackets, tabs and soft returns as \t
// and ~. A deliberate difference: an empty replacement in MemoTran() is the default one (the reference writes a zero
// byte).
#define CRLF Chr( 13 ) + Chr( 10 )
#define SOFT Chr( 141 ) + Chr( 10 )

PROCEDURE Main()
   LOCAL cText := "The quick brown fox jumps over the lazy dog" + CRLF + "second" + Chr( 9 ) + "line" + SOFT + ;
                  "soft" + CRLF + CRLF + "averyveryverylongwordthatdoesnotfit end"
   LOCAL n, i, a

   ? "count:", MLCount( cText, 10 ), MLCount( cText, 10, 4, .F. ), MLCount( cText ), MLCount( "" ), ;
      MLCount( "abc", 0 ), MLCount( 5 )
   FOR EACH n IN { 10, 7 }
      FOR i := 1 TO MLCount( cText, n ) + 1
         ? "line", n, i, Show( MemoLine( cText, n, i ) ), Show( MemoLine( cText, n, i, 4, .F. ) ), ;
            MLPos( cText, n, i ), MLPos( cText, n, i, 4, .F. )
      NEXT
   NEXT
   ? "tabs:", Show( MemoLine( Chr( 9 ) + "a" + Chr( 9 ) + "b", 10, 1, 3 ) ), ;
      Show( MemoLine( "ab" + Chr( 9 ) + "c", 10, 1, 4 ) ), Show( MemoLine( Chr( 9 ) + "x", 4, 1, 8 ) )
   ? "edges:", Show( MemoLine( cText, 10, 0 ) ), Show( MemoLine( "abc", 5, 3 ) ), Show( MemoLine( "abc" ) ), ;
      MLPos( "abc", 5, 0 ), MLPos( "abc", 5, 2 ), MLPos( "abc", 5, 3 ), MLPos( "a b c d e f", 3, 9 )
   FOR i := 1 TO 3
      ? "position:", i, MLCToPos( cText, 10, i, 0 ), MLCToPos( cText, 10, i, 3 ), MLCToPos( cText, 10, i, 20 ), ;
         MLCToPos( cText, 4, i, 2 ), MLCToPos( cText, 10, i )
   NEXT
   FOR EACH n IN { 1, 5, 11, 12, 44, 45, 46, 52, 60, 61, 70, Len( cText ), Len( cText ) + 1, Len( cText ) + 2, 0 }
      a := MPosToLC( cText, 10, n )
      ?? " " + LTrim( Str( n ) ) + "->" + LTrim( Str( a[ 1 ] ) ) + "," + LTrim( Str( a[ 2 ] ) )
   NEXT
   ? "hard:", Show( HardCR( "a" + SOFT + "b" + CRLF + "c" ) ), Show( HardCR( 5 ) )
   ? "tran:", Show( MemoTran( "a" + CRLF + "b" + SOFT + "c" ) ), ;
      Show( MemoTran( "a" + CRLF + "b" + SOFT + "c", "|", "_" ) ), Show( MemoTran( "a" + CRLF + "b", "" ) ), ;
      Show( MemoTran( NIL ) )
RETURN

// The text with its control characters visible.
STATIC FUNCTION Show( c )
   c := StrTran( c, Chr( 13 ), "\r" )
   c := StrTran( c, Chr( 10 ), "\n" )
   c := StrTran( c, Chr( 9 ), "\t" )
   c := StrTran( c, Chr( 141 ), "~" )
RETURN "[" + c + "]"
