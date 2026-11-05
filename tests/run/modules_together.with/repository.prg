// Part of tests/run/modules_together.prg.
STATIC s_count := 100

FUNCTION RepoAdd( n )
   s_count += n
   RETURN Tag() + Str( s_count, 4 )

FUNCTION RepoCount()
   RETURN s_count

STATIC FUNCTION Tag()
   RETURN "repo"

STATIC PROCEDURE Main()
   ? "a STATIC Main must not start the program"
   RETURN
