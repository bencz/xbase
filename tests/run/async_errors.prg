// An error nobody handles in an ASYNC function fails its task; awaited, it is raised again where it is awaited, and
// reaches the default handler there, which shows where it happened (in the task that failed, Called from) and each
// place it was waited for on its way (Waited for in): here Inner fails, Middle awaits it, Main awaits Middle. The
// task of a block waited for with Result shows its calls the same way (Before). Inner waits for a channel that
// receives its value only once both functions have suspended: the report is the same however loaded the machine is.
PROCEDURE Main()
   LOCAL ch := Channel(), oT
   ? "before:", Before()
   oT := Middle( 4, ch )
   ch:TrySend( 1 )
   ? "middle:", AWAIT oT
   ? "not reached"
RETURN

STATIC FUNCTION Before()
   LOCAL t := TaskRun( {|| Divide( 1, 0 ) + Len( 5 ) } )
   t:Wait()
RETURN t:Error:Description + ", " + t:Error:Operation

STATIC FUNCTION Divide( a, b )
RETURN a / b

ASYNC FUNCTION Middle( n, ch )
   LOCAL x := AWAIT Inner( n, ch )
RETURN x

ASYNC FUNCTION Inner( n, ch )
   AWAIT ch:ReceiveAsync()
RETURN Helper( n )

STATIC FUNCTION Helper( n )
RETURN n + "text"
