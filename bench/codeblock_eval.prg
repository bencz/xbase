// Codeblocks called with Eval(): 10,000,000 calls of each of three blocks kept in an array (no argument, two
// arguments, one that reads a captured local), the results summed.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), i, n := 0, aBlocks := Blocks( 3 ), ok
   FOR i := 1 TO 10000000
      n += Eval( aBlocks[ 1 ] ) + Eval( aBlocks[ 2 ], i % 100, 1 ) + Eval( aBlocks[ 3 ], i % 10 )
   NEXT
   ok := n == 1145000000
   ? "codeblock_eval", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN

STATIC FUNCTION Blocks( nBase )
   RETURN { {|| 7 }, {| x, y | x * 2 + y }, {| x | x + nBase } }
