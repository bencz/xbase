// A module compiled as part of the runtime (runtime_module.rt/, xbc --runtime): its public function is a runtime
// function (declared NATIVE, xbf_ name), shown on the call stack with line 0, as the C ones are; its STATIC function
// and its codeblock do not show, though PCount() and PValue() read their own frames. __ArgError() raises its argument
// error.
EXTERN FUNCTION RtWalk( bBlock, ... ) NATIVE

PROCEDURE Main()
   LOCAL e
   ? RtWalk( {|| Stack() }, "x", "y" )
   ErrorBlock( {| err | Break( err ) } )
   BEGIN SEQUENCE
      RtWalk( 5, "w" )
   RECOVER USING e
      ? "error", e:subCode, e:operation, Len( e:args ), e:args[ 1 ], e:args[ 2 ], e:canSubstitute
   END
RETURN

FUNCTION Stack()
   LOCAL n := 0, s := ""
   DO WHILE !Empty( ProcName( n ) )
      s += ProcName( n ) + "(" + LTrim( Str( ProcLine( n ) ) ) + ") "
      n++
   ENDDO
RETURN s
