// INIT procedures run after the STATICs are initialized, module by module, each module's from
// its last; EXIT procedures after the start function, module by module, each module's in order.
STATIC s := "static"
PROCEDURE Main()
   ? "main", s
   B()
RETURN

INIT PROCEDURE First()
   ? "init first", s
RETURN

EXIT PROCEDURE Last()
   ? "exit last"
RETURN

INIT PROCEDURE Second2()
   ? "init second"
RETURN

EXIT PROCEDURE Last2()
   ? "exit last2"
RETURN
