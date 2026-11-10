// ProcFile( [nLevel] ): the source file, without directories, of the function at that level of the call stack: a
// codeblock's is its function's; a module compiled in the same run or on its own has its own; a runtime function
// (in C or in xBase) and a level past the stack or below 0 have none.
PROCEDURE Main()
   LOCAL b := {|| ProcFile( 0 ) + "|" + ProcFile( 1 ) }
   ? "[" + ProcFile() + "]", "[" + ProcFile( 0 ) + "]", "[" + ProcFile( 1 ) + "]", "[" + ProcFile( 9 ) + "]"
   ? Eval( b )
   ? InWith(), InOwn(), "[" + ProcFile( -1 ) + "]", ValType( ProcFile( "x" ) )
   AScan( { 1 }, {|| QQOut( " <" + ProcFile( 1 ) + ":" + ProcName( 1 ) + ">" ), .T. } )
   AEval( { 1 }, {|| QQOut( " <" + ProcFile( 1 ) + ":" + ProcName( 1 ) + ">" ) } )
RETURN
