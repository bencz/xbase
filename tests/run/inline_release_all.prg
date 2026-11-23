// RELEASE ALL acts on the PRIVATEs the activation calling it created, also in a function that could be inlined (it
// never is: its frame is the activation): the caller's PRIVATEs stay, those of the function go. In a codeblock alike.
PROCEDURE Main()
   PRIVATE x := 1, y := 2
   Clear()
   ? "after Clear:", Type( "x" ), x
   Mine()
   ? "after Mine:", Type( "y" ), y
   Eval( {|| __MemReleaseAll( "*", .T. ) } )
   ? "after the block:", Type( "x" ), x, Type( "y" ), y
RETURN

STATIC PROCEDURE Clear()
   RELEASE ALL
RETURN

STATIC PROCEDURE Mine()
   PRIVATE z := 3
   RELEASE ALL LIKE z
   ? "z released:", z == NIL, Type( "y" )
RETURN
