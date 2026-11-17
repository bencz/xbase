// The memo-line functions given a width or a column past what 32 bits hold (2^32 + 3, 2^32 + 4, 2^62, the largest
// integer, 10^300): as wide as no line, on every target (32-bit hosts too: 2^32 + 3 is no width of 3), next to the
// widths that do cut the text; MemoLine() with such a width is the error of a string past the largest one or past
// memory (its line is padded to the width), widths no memory holds on any target only.
#include "error.ch"

PROCEDURE Main()
   LOCAL c := "abc def ghi", w
   FOR EACH w IN { 3, 4, 7, 2 ^ 32 + 3, 2 ^ 32 + 4, 2 ^ 62, 9223372036854775807, 10 ^ 300 }
      ? PadL( iif( w < 1000, LTrim( Str( w ) ), "huge" ), 5 ), MLCount( c, w ), MLPos( c, w, 2 ), ;
        MLCToPos( c, w, 1, 9 ), ArrText( MPosToLC( c, w, 9 ) ), "[" + MemoLine( c, Min( w, 20 ), 1 ) + "]"
   NEXT
   FOR EACH w IN { 2 ^ 62, 9223372036854775807, 10 ^ 300 }
      ? "MemoLine huge:", Try( c, w )
   NEXT
RETURN

STATIC FUNCTION ArrText( a )
RETURN "{" + LTrim( Str( a[ 1 ] ) ) + "," + LTrim( Str( a[ 2 ] ) ) + "}"

STATIC FUNCTION Try( c, w )
   LOCAL x
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      x := MemoLine( c, w, 1 )
      x := iif( Len( x ) == w, "made, padded", "made " + LTrim( Str( Len( x ) ) ) )
   RECOVER USING x
      x := iif( x:operation == "MEMOLINE" .AND. ( x:genCode == EG_MEM .OR. x:genCode == EG_STROVERFLOW ), "error", ;
                x:operation + " " + LTrim( Str( x:subCode ) ) )
   END SEQUENCE
RETURN x
