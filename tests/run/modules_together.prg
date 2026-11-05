// Three modules compiled in one xbc run (tests/run/modules_together.with): calls between them need no declaration,
// each module has its own file-wide STATICs and STATIC functions (Tag() here is not the one of repository.prg), a
// reference reaches another module's function, and the public MAIN starts the program although it is not the first
// function (a STATIC PROCEDURE Main of another module does not count).
STATIC s_count := 1

FUNCTION First()
   ? "First() must not start the program"
   RETURN NIL

PROCEDURE Main()
   LOCAL x := 21
   ? RepoAdd( 5 ), RepoAdd( 1 ), Tag(), s_count
   Twice( @x )
   ? x, RepoCount()
   RETURN

STATIC FUNCTION Tag()
   RETURN "main"
