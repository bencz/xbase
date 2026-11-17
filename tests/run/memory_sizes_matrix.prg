// Every function that takes a size from the program, given sizes past any address space (2^62, 10^17, the largest
// integer, 2^63 as a double, 10^300): the error raised names the function (its operation) and is either BASE/9032
// Memory low (EG_MEM) or BASE/1234 String overflow (EG_STROVERFLOW), the code and the subcode agreeing (which of the
// two depends on the target's largest string: 32-bit targets overflow first). Handled, the program goes on each time.
#include "error.ch"

PROCEDURE Main()
   LOCAL aSize := { 2 ^ 62, 100000000000000000, 9223372036854775807, 2 ^ 63, 10 ^ 300 }, n, f, c
   LOCAL aFn := { { "SPACE", {| n | Space( n ) } }, { "REPLICATE", {| n | Replicate( "ab", n ) } }, ;
                  { "PADL", {| n | PadL( "x", n ) } }, { "PADR", {| n | PadR( "x", n ) } }, ;
                  { "PADC", {| n | PadC( "x", n ) } }, { "ARRAY", {| n | Array( n ) } }, ;
                  { "ARRAY", {| n | Array( 10, n ) } }, { "ASIZE", {| n | ASize( { 1 }, n ) } }, ;
                  { "HALLOCATE", {| n | HAllocate( { => }, n ) } }, { "MEMOLINE", {| n | MemoLine( "abc", n ) } }, ;
                  { "STR", {| n | Str( 1, n ) } }, { "STRZERO", {| n | StrZero( 1, n ) } }, ;
                  { "STR", {| n | Str( 1.5, n, 2 ) } } }
   FOR EACH f IN aFn
      c := ""
      FOR EACH n IN aSize
         c += " " + Try( f[ 1 ], f[ 2 ], n )
      NEXT
      ? PadR( f[ 1 ], 10 ) + c
   NEXT
   ? "goes on:", Len( Space( 1000 ) ), Len( Array( 1000 ) )
RETURN

// "ok" for an error of `cOp` that is Memory low or String overflow with its code and subcode agreeing, else what came.
STATIC FUNCTION Try( cOp, b, n )
   LOCAL x
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      x := Eval( b, n )
      x := "made " + ValType( x )
   RECOVER USING x
      IF x:operation == cOp .AND. x:subsystem == "BASE" .AND. ;
         ( ( x:genCode == EG_MEM .AND. x:subCode == 9032 ) .OR. ;
           ( x:genCode == EG_STROVERFLOW .AND. x:subCode == 1234 ) )
         x := "ok"
      ELSE
         x := x:subsystem + "/" + LTrim( Str( x:subCode ) ) + " " + x:operation
      ENDIF
   END SEQUENCE
RETURN x
