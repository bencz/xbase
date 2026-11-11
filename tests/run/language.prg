// SET LANGUAGE: English until another is selected; Portuguese and Spanish name the days and months (CDoW, CMonth) and
// describe the runtime's errors in their language; Set( _SET_LANGUAGE ) gives the language before the call, takes a
// name in any letter case, and leaves an unknown one as it is. A subcode's own description (a scope violation) comes
// in the language too. CDoW and CMonth of something that is no date are errors the handler may give the result of.
#include "set.ch"

PROCEDURE Main()
   LOCAL d := SToD( "20241225" )
   ? Set( _SET_LANGUAGE ), CDoW( d ), CMonth( d ), Description()
   SET LANGUAGE TO PT
   ? Set( _SET_LANGUAGE ), CDoW( d ), CMonth( d ), Description(), CDoW( d + 4 ), CMonth( d + 10 )
   ? Description( {|| TSecret():New():cHidden } ), Description( {|| TSecret():New():nProtected := 1 } )
   ? Set( _SET_LANGUAGE, "es" ), Set( _SET_LANGUAGE ), CDoW( d ), CMonth( d ), Description()
   ? Set( _SET_LANGUAGE, "XX" ), Set( _SET_LANGUAGE ), "[" + CDoW( CToD( "" ) ) + "]", "[" + CMonth( CToD( "" ) ) + "]"
   SET LANGUAGE TO ( "En" )
   ErrorBlock( {| e | IIF( e:canSubstitute, e:operation + " " + LTrim( Str( e:subCode ) ), Break( e ) ) } )
   ? Set( _SET_LANGUAGE ), CDoW( 1 ), CMonth( "x" ), Description()
RETURN

STATIC FUNCTION Description( bFail )
   LOCAL e, b := ErrorBlock( {| o | Break( o ) } )
   BEGIN SEQUENCE
      e := IIF( bFail == NIL, 1 + "x", Eval( bFail ) )
   RECOVER USING e
      e := e:description
   END SEQUENCE
   ErrorBlock( b )
RETURN e

CLASS TSecret
   HIDDEN:
   VAR cHidden INIT "h"
   PROTECTED:
   VAR nProtected INIT 0
ENDCLASS
