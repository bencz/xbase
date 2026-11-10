// RtWalk( bBlock, ... ): the stack seen from bBlock, run by a STATIC function through a codeblock, with the counts the
// hidden frames see.
FUNCTION RtWalk( bBlock, ... )
   IF ValType( bBlock ) != "B"
      RETURN __ArgError( 3012 )
   ENDIF
RETURN Walk( bBlock, ... ) + "/" + LTrim( Str( PCount() ) )

STATIC FUNCTION Walk( bBlock, ... )
RETURN Eval( {| a | Eval( bBlock ) + LTrim( Str( PCount() ) ) + PValue( 1 ) }, "z" ) + LTrim( Str( PCount() ) )
