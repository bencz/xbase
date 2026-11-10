// A program's own ErrorSys() replaces the runtime's: the startup calls it by name after the STATIC initializations
// and before the INIT procedures, and the block it installs handles the errors.
STATIC s_cName := "mine"

INIT PROCEDURE First()
   ? "init", ValType( ErrorBlock() )
RETURN

PROCEDURE Main()
   ? "main", SubStr( 1, 1 ), 10 / 0
RETURN

PROCEDURE ErrorSys()
   ? "errorsys", s_cName
   ErrorBlock( {| e | "<" + e:operation + ">" } )
RETURN
