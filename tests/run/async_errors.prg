// An error nobody handles in an ASYNC function fails its task; awaited, it is raised again where it is awaited, and
// reaches the default handler there, which shows where it happened (in the task that failed, Called from) and each
// place it was waited for on its way (Waited for in): here Inner fails, Middle awaits it, Main awaits Middle. The
// task of a block waited for with Result shows its calls the same way (Before).
PROCEDURE Main()
   ? "before:", Before()
   ? "middle:", AWAIT Middle( 4 )
   ? "not reached"
RETURN

STATIC FUNCTION Before()
   LOCAL t := TaskRun( {|| Divide( 1, 0 ) + Len( 5 ) } )
   t:Wait()
RETURN t:Error:Description + ", " + t:Error:Operation

STATIC FUNCTION Divide( a, b )
RETURN a / b

ASYNC FUNCTION Middle( n )
   LOCAL x := AWAIT TaskDelay( 1 )
   x := AWAIT Inner( n )
RETURN x

ASYNC FUNCTION Inner( n )
   AWAIT TaskDelay( 1 )
RETURN Helper( n )

STATIC FUNCTION Helper( n )
RETURN n + "text"
