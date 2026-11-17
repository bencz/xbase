// Sizes a program takes from its input that no memory holds (here past any address space) raise an error the program
// handles, rather than ending it: Space, Replicate, PadL/PadR/PadC, Array (one and several dimensions), ASize,
// HAllocate, MemoLine and Str with a width: BASE/9032 Memory low (or the string overflow, where the size is past the
// largest string first, as on 32-bit targets); the program goes on, and memory is as usable as before.
#include "error.ch"

PROCEDURE Main()
   LOCAL b, n := 0
   LOCAL nHuge := 100000000000000000 // past any address space
   FOR EACH b IN { {|| Space( nHuge ) }, {|| Replicate( "ab", nHuge ) }, {|| PadL( "x", nHuge ) }, ;
                   {|| PadR( "x", nHuge ) }, {|| PadC( "x", nHuge ) }, {|| Array( nHuge ) }, {|| Array( 10, nHuge ) }, ;
                   {|| ASize( { 1, 2 }, nHuge ) }, {|| HAllocate( { => }, nHuge ) }, {|| MemoLine( "abc", nHuge ) } }
      ? ++n, Try( b )
   NEXT
   FOR EACH n IN Array( 1000 )
      b := Space( 100000 )
   NEXT
   ? "goes on:", Len( b ), Len( Array( 1000 ) ), Len( Replicate( "ab", 50000 ) )
RETURN

STATIC FUNCTION Try( b )
   LOCAL x
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      x := Eval( b )
      x := "made " + ValType( x )
   RECOVER USING x
      x := x:subsystem + IIf( x:genCode == EG_MEM .OR. x:genCode == EG_STROVERFLOW, " memory or overflow: handled", ;
         " OTHER " + LTrim( Str( x:genCode ) ) )
   END SEQUENCE
RETURN x
